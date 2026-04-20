/**
 * @file c21_dpm_tuning_test.cpp
 * @brief DPM (Detection of Plunger Movement) tuning + CSV-logging cycle test.
 *
 * @details
 *   Cycles a Parker C21 24V solenoid on MAX22200 channel 0, with DPM
 *   enabled and configured via the parameters at the top of this file.
 *   During every ENERGISE phase the FAULT register is polled at high
 *   rate (~1 kHz) for the first ~200 ms so we can capture exactly when
 *   (and whether) DPM fires within the HIT window.
 *
 *   The MAX22200 does NOT expose continuous current measurements over
 *   SPI — DPM is a binary "did we see a current dip" flag handled by
 *   the chip's analogue front-end. So instead of plotting the actual
 *   current waveform we plot the DPM event timing across many cycles.
 *
 *   Per the datasheet (CFG_DPM section):
 *     - ISTART (7-bit, 0-127): current above which the chip starts
 *       monitoring for a dip. ISTART = value × IFS/127. With IFS=1000mA,
 *       value=15 → ISTART ≈ 118 mA. Must be < HIT current.
 *     - IPTH   (4-bit, 0-15): minimum dip amplitude to count as valid.
 *       IPTH = value × IFS/127. With IFS=1000mA, value=1 → 7.87 mA dip.
 *       Lower = more sensitive.
 *     - TDEB   (4-bit, 0-15): minimum dip duration in chopping periods.
 *       Lower = more sensitive but also more false positives.
 *
 *   Recommended starting tune for C21 (102 mA hit / 51 mA hold):
 *     - ISTART_MA = 30   (start monitoring well below HIT)
 *     - IPTH_MA   = 8    (catch ~10% dips, typical plunger BEMF)
 *     - TDEB_MS   = 0.5  (filter out noise, allow short dips)
 *
 *   CSV output format on every cycle:
 *     cycle,phase,t_us,fault_byte,status_byte,dpm_fired
 *   - phase: ENERGISE | RELEASE
 *   - t_us:  microseconds since the ENERGISE / RELEASE banner
 *   - fault_byte: lower 8 bits of FAULT register
 *   - status_byte: lower 8 bits of STATUS register (fault flags)
 *   - dpm_fired: 1 if FAULT.DPM[0] is set this sample, else 0
 *
 *   Pipe the serial output through a logger and grep for `CSV,` lines
 *   to plot in your tool of choice (matplotlib, Excel, octave, etc.).
 *
 *   At the end of N cycles a summary prints the DPM hit-rate and the
 *   distribution of "time-to-fire" so you can see whether your
 *   ISTART/IPTH/TDEB tune is catching real plunger movements.
 *
 * @par DPM polarity (datasheet §"Detection of Plunger Movement")
 *
 *     FAULT.DPM[ch] = 0  →  current dip seen → plunger MOVED (healthy)
 *     FAULT.DPM[ch] = 1  →  drop NOT revealed → plunger STUCK (fault)
 *
 *   A free-moving valve should produce **zero DPM fires** across many
 *   cycles. To validate the DPM logic, hold the plunger still by hand
 *   during a cycle — then DPM should fire (DPM=1) for that cycle.
 *
 * @par Tuning workflow
 *   1. Run with the C21 plunger free. Expect 0 DPM fires per cycle —
 *      confirms healthy operation and that the tune isn't false-
 *      positive on noise.
 *   2. Hold the plunger physically still and run again. DPM SHOULD
 *      fire on every cycle — confirms the algorithm catches the
 *      "no-movement" condition.
 *   3. If step 2 doesn't fire DPM, lower IPTH (catch smaller dips
 *      → easier for chip to "miss" them when no real movement) or
 *      raise ISTART (start monitoring later in the rise).
 *   4. If step 1 produces false positives, raise IPTH (need bigger
 *      dips to count).
 *   5. Adjust ISTART so monitoring starts before the BEMF dip but
 *      doesn't trigger on the inrush slope.
 *
 * @author HardFOC
 * @date   2026
 */

#include <cinttypes>
#include <cstdio>
#include <memory>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_timer.h"

#include "esp32_max22200_bus.hpp"
#include "esp32_max22200_test_config.hpp"
#include "max22200.hpp"
#include "max22200_registers.hpp"
#include "max22200_types.hpp"

using namespace max22200;
using namespace MAX22200_TestConfig;

static const char* TAG = "C21Dpm";

//==============================================================================
// TUNING PARAMETERS — edit these and re-flash
//==============================================================================

namespace cfg {

// ─── Channel + drive profile (matches c21_cycle_test) ─────────────────
constexpr uint8_t  kChannel        = 0;
constexpr uint16_t kHitCurrent_mA  = 102;
constexpr uint16_t kHoldCurrent_mA =  51;
constexpr float    kHitTime_ms     = 100.0f;
constexpr ChopFreq kChopFreq       = ChopFreq::FMAIN_DIV4;
constexpr bool     kEnableSlewRate = true;

// ─── DPM tuning parameters ────────────────────────────────────────────
// Start with these defaults; iterate based on CSV / summary output.
//
// First-light tune (most sensitive): IPTH=4 mA (raw=1, smallest nonzero),
// TDEB=0.05 ms (raw=1, ~40 µs at FMAIN_DIV4=25 kHz), ISTART=20 mA.
// If DPM fires too easily / on noise, raise IPTH; if it misses real
// movements, lower TDEB (already minimal) or lower ISTART further.
constexpr float kDpmStartCurrent_mA   = 20.0f;   // ISTART — start monitoring early
constexpr float kDpmThreshold_mA      = 4.0f;    // IPTH   — most-sensitive non-zero
constexpr float kDpmDebounce_ms       = 0.05f;   // TDEB   — minimum (1 chopping period)

// ─── Polling cadence during HIT window ────────────────────────────────
// Each FAULT read is one SPI cmd + 4 bytes = ~50 µs at 1 MHz, plus GPIO
// overhead → conservatively ~150–200 µs per sample. 1 ms cadence is
// safe and gives 100 samples per HIT window.
constexpr uint32_t kPollPeriod_us     = 1000;
constexpr uint32_t kEnergisePoll_ms   = 200;     // poll for first 200 ms
constexpr uint32_t kReleasePoll_ms    = 50;      // brief poll after release

// ─── Cycle profile ────────────────────────────────────────────────────
constexpr uint32_t kOnDuration_ms     = 500;
constexpr uint32_t kOffDuration_ms    = 500;
constexpr uint32_t kCycleCount        = 10;      // 0 = forever

}  // namespace cfg

//==============================================================================
// GLOBAL RESOURCES
//==============================================================================

static std::unique_ptr<Esp32Max22200SpiBus>            g_bus;
static std::unique_ptr<MAX22200<Esp32Max22200SpiBus>>  g_driver;

// DPM event statistics across the run
static uint32_t g_total_cycles            = 0;
static uint32_t g_cycles_with_dpm         = 0;
static uint64_t g_dpm_first_fire_us_sum   = 0;
static uint32_t g_dpm_first_fire_min_us   = UINT32_MAX;
static uint32_t g_dpm_first_fire_max_us   = 0;

//==============================================================================
// HIGH-RATE POLLER
//==============================================================================

/// Poll FAULT + STATUS at ~1 kHz for a fixed window, emit CSV per sample,
/// and return the µs offset at which DPM first fired (or UINT32_MAX if it
/// never did).
static uint32_t poll_csv(uint32_t cycle, const char* phase, uint32_t window_ms) noexcept {
    const int64_t  t0_us           = esp_timer_get_time();
    const uint32_t end_us          = window_ms * 1000U;
    uint32_t       first_dpm_us    = UINT32_MAX;
    uint8_t        prev_dpm_byte   = 0;

    while (true) {
        const int64_t  now_us = esp_timer_get_time();
        const uint32_t t_us   = static_cast<uint32_t>(now_us - t0_us);
        if (t_us >= end_us) break;

        FaultStatus  faults{};
        StatusConfig status{};
        const DriverStatus fr = g_driver->ReadFaultRegister(faults);
        const DriverStatus sr = g_driver->ReadStatus(status);
        if (fr != DriverStatus::OK || sr != DriverStatus::OK) {
            ESP_LOGW(TAG, "  poll read failed (cycle=%u t=%uus)",
                     static_cast<unsigned>(cycle), static_cast<unsigned>(t_us));
            vTaskDelay(pdMS_TO_TICKS(1));
            continue;
        }

        const uint8_t dpm_byte = faults.plunger_movement_fault_channel_mask;
        const bool dpm_fired   = (dpm_byte & (1U << cfg::kChannel)) != 0;
        const bool ocp_fired   = (faults.overcurrent_channel_mask
                                  & (1U << cfg::kChannel)) != 0;
        const bool hhf_fired   = (faults.hit_not_reached_channel_mask
                                  & (1U << cfg::kChannel)) != 0;
        const bool olf_fired   = (faults.open_load_fault_channel_mask
                                  & (1U << cfg::kChannel)) != 0;

        // Capture-and-print only when something changes (or every 10 ms
        // tick) so the CSV stream stays manageable but doesn't miss
        // edge transitions.
        const bool new_dpm_edge = dpm_fired && (prev_dpm_byte == 0);
        if (new_dpm_edge && first_dpm_us == UINT32_MAX) {
            first_dpm_us = t_us;
        }
        prev_dpm_byte = dpm_byte;

        // CSV: cycle,phase,t_us,fault_dpm_byte,status_byte,dpm_fired,ocp,hhf,olf
        printf("CSV,%u,%s,%u,0x%02X,0x%02X,%d,%d,%d,%d\n",
               static_cast<unsigned>(cycle), phase,
               static_cast<unsigned>(t_us),
               dpm_byte, static_cast<uint8_t>(status.toRegister() & 0xFF),
               dpm_fired ? 1 : 0,
               ocp_fired ? 1 : 0, hhf_fired ? 1 : 0, olf_fired ? 1 : 0);

        // Sleep-until next sample
        vTaskDelay(pdMS_TO_TICKS(cfg::kPollPeriod_us / 1000U));
    }

    return first_dpm_us;
}

//==============================================================================
// SETUP
//==============================================================================

static bool wake_chip() noexcept {
    if (auto s = g_driver->Initialize(); s != DriverStatus::OK) {
        ESP_LOGE(TAG, "Initialize failed: %s", DriverStatusToStr(s));
        return false;
    }

    // Wake-up: re-issue bare ACTIVE=1 writes until STATUS reads back
    // ACTIVE=1, accommodating the chip's tWU=2.5 ms wake-up time and
    // any V18 LDO settling delay.
    vTaskDelay(pdMS_TO_TICKS(50));
    StatusConfig st{};
    for (int i = 0; i < 100; ++i) {
        (void)g_driver->WriteRegister32(RegBank::STATUS, 0x00000001U);
        (void)g_driver->ReadStatus(st);
        if (st.active && !st.undervoltage) break;
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    if (!st.active) {
        ESP_LOGE(TAG, "Chip never woke (ACTIVE=%d UVM=%d)",
                 st.active, st.undervoltage);
        return false;
    }
    ESP_LOGI(TAG, "✅ Chip awake — ACTIVE=1, UVM=0");

    FaultStatus drain{};
    (void)g_driver->ReadFaultRegister(drain);
    return true;
}

static bool configure_for_dpm() noexcept {
    // Configure CH0 with DPM enabled. SetDpmEnabledChannels(0x01) sets
    // the per-channel DPM_EN bit in CFG_CH0 byte 0.
    const DriverStatus rc = g_driver->ConfigureChannelCdr(
        cfg::kChannel,
        cfg::kHitCurrent_mA,
        cfg::kHoldCurrent_mA,
        cfg::kHitTime_ms,
        SideMode::LOW_SIDE,
        cfg::kChopFreq,
        cfg::kEnableSlewRate,
        /*open_load_detection_enabled=*/false,
        /*plunger_movement_detection_enabled=*/true,   // ← enable DPM
        /*hit_current_check_enabled=*/false);
    if (rc != DriverStatus::OK) {
        ESP_LOGE(TAG, "ConfigureChannelCdr failed: %s", DriverStatusToStr(rc));
        return false;
    }
    ESP_LOGI(TAG, "✅ CH%u configured for CDR + DPM enabled", cfg::kChannel);

    // Configure global DPM algorithm parameters.
    const DriverStatus drc = g_driver->ConfigureDpm(
        cfg::kDpmStartCurrent_mA,
        cfg::kDpmThreshold_mA,
        cfg::kDpmDebounce_ms);
    if (drc != DriverStatus::OK) {
        ESP_LOGE(TAG, "ConfigureDpm failed: %s", DriverStatusToStr(drc));
        return false;
    }
    ESP_LOGI(TAG,
             "✅ DPM tune: ISTART=%.1f mA, IPTH=%.1f mA, TDEB=%.2f ms",
             static_cast<double>(cfg::kDpmStartCurrent_mA),
             static_cast<double>(cfg::kDpmThreshold_mA),
             static_cast<double>(cfg::kDpmDebounce_ms));

    // Read back what landed in CFG_DPM (raw + decoded) so the operator
    // can see exactly what the chip is using.
    DpmConfig live{};
    if (g_driver->ReadDpmConfig(live) == DriverStatus::OK) {
        ESP_LOGI(TAG,
                 "  CFG_DPM read-back: ISTART_raw=%u TDEB_raw=%u IPTH_raw=%u",
                 live.plunger_movement_start_current,
                 live.plunger_movement_debounce_time,
                 live.plunger_movement_current_threshold);
    }
    return true;
}

//==============================================================================
// CYCLE
//==============================================================================

static void run_cycle(uint32_t cycle) noexcept {
    const uint8_t ch_bit = static_cast<uint8_t>(1U << cfg::kChannel);
    g_total_cycles++;

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "═══ Cycle %u: ENERGISE  CH%u  (hit %u mA → hold %u mA) ═══",
             static_cast<unsigned>(cycle + 1U), cfg::kChannel,
             cfg::kHitCurrent_mA, cfg::kHoldCurrent_mA);

    // Drain any stale FAULT bits so DPM-fire timing is fresh.
    FaultStatus drain{};
    (void)g_driver->ReadFaultRegister(drain);

    // ENERGISE: 8-bit MSB-only ONCH write to STATUS[31:24]
    (void)g_driver->SetChannelsOn(ch_bit);

    const uint32_t first_us = poll_csv(cycle + 1U, "ENERGISE",
                                       cfg::kEnergisePoll_ms);

    // Hold for the remainder of ON time after the polling window.
    if (cfg::kOnDuration_ms > cfg::kEnergisePoll_ms) {
        vTaskDelay(pdMS_TO_TICKS(cfg::kOnDuration_ms - cfg::kEnergisePoll_ms));
    }

    if (first_us != UINT32_MAX) {
        g_cycles_with_dpm++;
        g_dpm_first_fire_us_sum += first_us;
        if (first_us < g_dpm_first_fire_min_us) g_dpm_first_fire_min_us = first_us;
        if (first_us > g_dpm_first_fire_max_us) g_dpm_first_fire_max_us = first_us;
        ESP_LOGW(TAG, "  → DPM=1 at t=%u µs (%.2f ms) — chip did NOT see plunger move",
                 static_cast<unsigned>(first_us), first_us / 1000.0);
    } else {
        ESP_LOGI(TAG, "  → DPM=0 — plunger movement DETECTED ✓ (healthy valve)");
    }

    // RELEASE
    ESP_LOGI(TAG, "═══ Cycle %u: RELEASE   CH%u ════════════════════════",
             static_cast<unsigned>(cycle + 1U), cfg::kChannel);
    (void)g_driver->SetChannelsOn(0);
    (void)poll_csv(cycle + 1U, "RELEASE", cfg::kReleasePoll_ms);
    if (cfg::kOffDuration_ms > cfg::kReleasePoll_ms) {
        vTaskDelay(pdMS_TO_TICKS(cfg::kOffDuration_ms - cfg::kReleasePoll_ms));
    }
}

//==============================================================================
// app_main
//==============================================================================

extern "C" void app_main() {
    vTaskDelay(pdMS_TO_TICKS(200));
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "╔══════════════════════════════════════════════════════════════════════╗");
    ESP_LOGI(TAG, "║   MAX22200 — C21 + DPM tuning + CSV log                              ║");
    ESP_LOGI(TAG, "║   target: CH%u  hit=%u mA  hold=%u mA  hit_time=%.0f ms                ║",
             cfg::kChannel, cfg::kHitCurrent_mA, cfg::kHoldCurrent_mA,
             static_cast<double>(cfg::kHitTime_ms));
    ESP_LOGI(TAG, "║   DPM tune: ISTART=%.0f mA  IPTH=%.1f mA  TDEB=%.2f ms                  ║",
             static_cast<double>(cfg::kDpmStartCurrent_mA),
             static_cast<double>(cfg::kDpmThreshold_mA),
             static_cast<double>(cfg::kDpmDebounce_ms));
    ESP_LOGI(TAG, "║   cycles=%u  ON %u ms  OFF %u ms  poll @ %u µs                          ║",
             static_cast<unsigned>(cfg::kCycleCount),
             static_cast<unsigned>(cfg::kOnDuration_ms),
             static_cast<unsigned>(cfg::kOffDuration_ms),
             static_cast<unsigned>(cfg::kPollPeriod_us));
    ESP_LOGI(TAG, "╚══════════════════════════════════════════════════════════════════════╝");
    ESP_LOGI(TAG, "CSV header: cycle,phase,t_us,fault_dpm_byte,status_byte,dpm_fired,ocp,hhf,olf");

    g_bus = CreateEsp32Max22200SpiBus();
    if (!g_bus) { ESP_LOGE(TAG, "Bus alloc failed"); return; }

    using Bd = MAX22200_TestConfig::BoardTestConfig;
    BoardConfig board(Bd::RREF_KOHM, Bd::HFS);
    board.max_current_ma   = Bd::MAX_CURRENT_MA;
    board.max_duty_percent = Bd::MAX_DUTY_PERCENT;
    g_driver = std::make_unique<MAX22200<Esp32Max22200SpiBus>>(*g_bus, board);
    if (!g_driver) { ESP_LOGE(TAG, "Driver alloc failed"); return; }

    if (!wake_chip())          return;
    if (!configure_for_dpm())  return;

    for (uint32_t i = 0; i < cfg::kCycleCount || cfg::kCycleCount == 0U; ++i) {
        run_cycle(i);
    }

    // Final summary
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "╔══════════════════════════════════════════════════════════════════════╗");
    ESP_LOGI(TAG, "║                     DPM TUNING SUMMARY                                ║");
    ESP_LOGI(TAG, "╠══════════════════════════════════════════════════════════════════════╣");
    ESP_LOGI(TAG, "  Cycles run:                          %u",
             static_cast<unsigned>(g_total_cycles));
    ESP_LOGI(TAG, "  Cycles with DPM=1 (no movement):     %u (%.1f %%)",
             static_cast<unsigned>(g_cycles_with_dpm),
             g_total_cycles > 0
                 ? (100.0 * g_cycles_with_dpm / g_total_cycles)
                 : 0.0);
    ESP_LOGI(TAG, "  Cycles with DPM=0 (movement seen):   %u (%.1f %%)",
             static_cast<unsigned>(g_total_cycles - g_cycles_with_dpm),
             g_total_cycles > 0
                 ? (100.0 * (g_total_cycles - g_cycles_with_dpm) / g_total_cycles)
                 : 0.0);
    if (g_cycles_with_dpm > 0) {
        const uint32_t avg_us = static_cast<uint32_t>(
            g_dpm_first_fire_us_sum / g_cycles_with_dpm);
        ESP_LOGI(TAG, "  DPM=1 first-fire latency (µs): min=%u avg=%u max=%u",
                 static_cast<unsigned>(g_dpm_first_fire_min_us),
                 static_cast<unsigned>(avg_us),
                 static_cast<unsigned>(g_dpm_first_fire_max_us));
        ESP_LOGI(TAG, "  DPM=1 first-fire latency (ms): min=%.2f avg=%.2f max=%.2f",
                 g_dpm_first_fire_min_us / 1000.0,
                 avg_us / 1000.0,
                 g_dpm_first_fire_max_us / 1000.0);
    }
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "  Interpretation:");
    ESP_LOGI(TAG, "    DPM=0 every cycle → plunger is moving freely (healthy valve)");
    ESP_LOGI(TAG, "    DPM=1 every cycle → plunger is stuck (HOLD it manually to test)");
    ESP_LOGI(TAG, "    Mixed              → tune is borderline — adjust IPTH / ISTART");
    ESP_LOGI(TAG, "╚══════════════════════════════════════════════════════════════════════╝");

    // Park
    (void)g_driver->SetChannelsOn(0);
}

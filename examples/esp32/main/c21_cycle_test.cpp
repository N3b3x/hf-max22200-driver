/**
 * @file c21_cycle_test.cpp
 * @brief Real-hardware Parker C21 24 V solenoid cycle test on MAX22200 CH0.
 *
 * @details
 *   Drives a single Parker C21-series 24 V solenoid (235 Ω coil → 102 mA
 *   peak / 51 mA hold per the C21 Hit-and-Hold table) on MAX22200 channel 0
 *   in CDR (current-controlled hit-and-hold) mode and cycles it ON / OFF
 *   periodically while the telemetry task scrapes:
 *
 *     * STATUS register (ACTIVE flag, channels-on bitmask, masked /
 *       unmasked fault summary)
 *     * FAULT register (OCP, HHF, OLF, DPM per-channel bitmasks)
 *     * Last fault byte from the CMD register (the fault byte the chip
 *       echoes back on every transaction)
 *     * nFAULT pin level (open-drain, asserts LOW on any unmasked fault)
 *
 *   This is the canonical bench check for:
 *     - Driver-level initialisation (the same path that exposed the
 *       MAX22200 GPIO 32+ EspGpio bug earlier)
 *     - CDR hit/hold profile actually pulses the coil at the configured
 *       102 mA hit then drops to 51 mA hold
 *     - DPM (plunger movement detection) fires once per actuation if a
 *       real C21 plunger moves
 *     - nFAULT pin and STATUS byte stay clean during normal operation
 *
 *   Datasheet anchors (cribbed from `WhValveCatalog::kSpecC21_24V` in
 *   the Flux WH valve example):
 *     * Coil resistance ≈ 235 Ω → 102 mA hit / 51 mA hold at 24 V
 *     * Minimum hit time per C21 Hit-and-Hold table: 100 ms
 *     * PWM chopping floor: 1 kHz (FMAIN_DIV4 satisfies this with a
 *       comfortable margin)
 *
 *   Default cycle profile:
 *     ON  for 2000 ms (hit pulse for 100 ms then hold at 51 mA)
 *     OFF for 2000 ms (channel released, coil de-energised)
 *     telemetry @ 10 Hz throughout
 *     run forever (Ctrl-] to stop)
 *
 *   Override at the top of the file for a different C21 variant.
 *
 * @par Wiring (ESP32-S3 reference / Flux V1 pinout)
 *   See `main/esp32_max22200_test_config.hpp`. SPI: MISO=GPIO35,
 *   MOSI=GPIO37, SCLK=GPIO36, CS=GPIO38, ENABLE=GPIO2, CMD=GPIO39,
 *   FAULT_N=GPIO42, TRIGA=GPIO40, TRIGB=GPIO41. Coil between OUT0 and
 *   +VM (low-side switching).
 *
 * @par Build
 *   From examples/esp32:
 *       ./scripts/build_app.sh c21_cycle_test Debug
 *       ./scripts/flash_app.sh flash_monitor c21_cycle_test Debug
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

// Set ESP32_MAX22200_ENABLE_DETAILED_SPI_LOGGING to 1 to dump every raw
// SPI TX/RX byte (useful for chasing chip-side comm or UVM contradictions).
// Default 0 once the test is happy — too noisy for steady-state runs.
#ifndef ESP32_MAX22200_ENABLE_DETAILED_SPI_LOGGING
#define ESP32_MAX22200_ENABLE_DETAILED_SPI_LOGGING 0
#endif
#ifndef ESP32_MAX22200_ENABLE_VERBOSE_BUS_LOGGING
#define ESP32_MAX22200_ENABLE_VERBOSE_BUS_LOGGING  1
#endif

#include "esp32_max22200_bus.hpp"
#include "esp32_max22200_test_config.hpp"
#include "max22200.hpp"
#include "max22200_registers.hpp"
#include "max22200_types.hpp"

using namespace max22200;
using namespace MAX22200_TestConfig;

static const char* TAG = "C21Cycle";

//==============================================================================
// TEST CONFIGURATION  (edit here for a different C21 variant / cadence)
//==============================================================================

namespace cfg {

// ─── Channel under test ────────────────────────────────────────────────
constexpr uint8_t kChannel = 0;                  ///< OUT0 / SOL_CH0

// ─── Datasheet specs (Parker C21 24 V) ─────────────────────────────────
constexpr uint16_t kHitCurrent_mA  = 102;        ///< 24 V / 235 Ω ≈ 102 mA
constexpr uint16_t kHoldCurrent_mA =  51;        ///< 50 % of hit per C21 table
constexpr float    kHitTime_ms     = 100.0f;     ///< C21 minimum hit pulse
constexpr ChopFreq kChopFreq       = ChopFreq::FMAIN_DIV4;  ///< ≥ 1 kHz floor

// ─── Cycle profile ─────────────────────────────────────────────────────
constexpr uint32_t kOnDuration_ms  = 2000;       ///< 2 s energised
constexpr uint32_t kOffDuration_ms = 2000;       ///< 2 s released
constexpr uint32_t kCycleCount     = 0;          ///< 0 = run forever

// ─── Telemetry cadence ─────────────────────────────────────────────────
constexpr uint32_t kTelemetryPeriod_ms = 100;    ///< 10 Hz

// ─── Diagnostic feature gates ─────────────────────────────────────────
constexpr bool kEnableSlewRateControl     = true;
constexpr bool kEnableOpenLoadDetection   = true;
constexpr bool kEnablePlungerMovementDet  = true;
constexpr bool kEnableHitCurrentCheck     = true;

}  // namespace cfg

//==============================================================================
// GLOBAL RESOURCES
//==============================================================================

static std::unique_ptr<Esp32Max22200SpiBus>            g_bus;
static std::unique_ptr<MAX22200<Esp32Max22200SpiBus>>  g_driver;

//==============================================================================
// TELEMETRY  (10 Hz scrape of STATUS, FAULT, last-fault byte, nFAULT pin)
//==============================================================================

static volatile bool g_telemetry_running = false;

static void telemetry_task(void* /*arg*/) noexcept {
    ESP_LOGI(TAG, "[telemetry] starting (period=%u ms)",
             static_cast<unsigned>(cfg::kTelemetryPeriod_ms));

    uint32_t tick_count = 0;
    bool     prev_dpm   = false;
    bool     prev_active = false;
    uint8_t  prev_chmask = 0xFF;  // force first print

    while (g_telemetry_running && g_driver) {
        StatusConfig status{};
        FaultStatus  faults{};
        const DriverStatus s_rc = g_driver->ReadStatus(status);
        const DriverStatus f_rc = g_driver->ReadFaultRegister(faults);
        const uint8_t      last_byte = g_driver->GetLastFaultByte();
        bool fault_active = false;
        (void)g_driver->GetFaultPinState(fault_active);

        // Always print one line per tick at INFO so the bench operator
        // sees the chip is alive.
        if (s_rc == DriverStatus::OK && f_rc == DriverStatus::OK) {
            ESP_LOGI(TAG,
                     "[t=%4u s+%03u] active=%d chmask=0x%02X  "
                     "OCP=0x%02X HHF=0x%02X OLF=0x%02X DPM=0x%02X  "
                     "last=0x%02X  nFAULT=%s",
                     static_cast<unsigned>(tick_count * cfg::kTelemetryPeriod_ms / 1000U),
                     static_cast<unsigned>(tick_count * cfg::kTelemetryPeriod_ms % 1000U),
                     status.active, status.channels_on_mask,
                     faults.overcurrent_channel_mask,
                     faults.hit_not_reached_channel_mask,
                     faults.open_load_fault_channel_mask,
                     faults.plunger_movement_fault_channel_mask,
                     last_byte,
                     fault_active ? "ASSERTED" : "released");
        } else {
            ESP_LOGW(TAG, "[telemetry] read failed: status=%s faults=%s",
                     DriverStatusToStr(s_rc), DriverStatusToStr(f_rc));
        }

        // Edge-detected highlights so the user notices significant events
        // even in the busy 10 Hz log stream.
        const bool dpm_now = (faults.plunger_movement_fault_channel_mask
                              & (1U << cfg::kChannel)) != 0;
        if (dpm_now && !prev_dpm) {
            ESP_LOGW(TAG, "    🔔 DPM fired on CH%u — plunger movement detected",
                     cfg::kChannel);
        }
        prev_dpm = dpm_now;

        if (status.active != prev_active) {
            ESP_LOGI(TAG, "    ▶ STATUS.active transition: %d → %d",
                     prev_active, status.active);
            prev_active = status.active;
        }
        if (status.channels_on_mask != prev_chmask) {
            ESP_LOGI(TAG, "    ▶ channels_on_mask transition: 0x%02X → 0x%02X",
                     prev_chmask, status.channels_on_mask);
            prev_chmask = status.channels_on_mask;
        }

        // Every 50 ticks (~5 s) print a one-line "device summary".
        if ((tick_count % 50U) == 0U && (s_rc == DriverStatus::OK)) {
            ESP_LOGI(TAG,
                     "  device summary: active=%d  ovt=%d uvm=%d comer=%d  "
                     "global{ocp=%d olf=%d hhf=%d dpm=%d}  hasFault=%d",
                     status.active,
                     status.overtemperature,
                     status.undervoltage,
                     status.communication_error,
                     status.overcurrent,
                     status.open_load_fault,
                     status.hit_not_reached,
                     status.plunger_movement_fault,
                     status.hasFault());
        }

        ++tick_count;
        vTaskDelay(pdMS_TO_TICKS(cfg::kTelemetryPeriod_ms));
    }

    ESP_LOGI(TAG, "[telemetry] stopping after %u ticks", static_cast<unsigned>(tick_count));
    vTaskDelete(nullptr);
}

//==============================================================================
// SETUP
//==============================================================================

static bool create_bus_and_driver() noexcept {
    g_bus = CreateEsp32Max22200SpiBus();
    if (!g_bus) {
        ESP_LOGE(TAG, "Failed to create Esp32Max22200SpiBus");
        return false;
    }

    // Board configuration: RREF=15 kΩ short on the dev board → IFS = 1000 mA.
    // C21's hit/hold currents (102 / 51 mA) sit comfortably inside this scale.
    using Bd = MAX22200_TestConfig::BoardTestConfig;
    BoardConfig board(Bd::RREF_KOHM, Bd::HFS);
    board.max_current_ma   = Bd::MAX_CURRENT_MA;
    board.max_duty_percent = Bd::MAX_DUTY_PERCENT;

    g_driver = std::make_unique<MAX22200<Esp32Max22200SpiBus>>(*g_bus, board);
    if (!g_driver) {
        ESP_LOGE(TAG, "Failed to allocate MAX22200 driver");
        g_bus.reset();
        return false;
    }

    ESP_LOGI(TAG, "Board: RREF=%.1f kΩ HFS=%d → IFS=%" PRIu32 " mA, max=%lu mA, max_duty=%u%%",
             static_cast<double>(Bd::RREF_KOHM),
             static_cast<int>(Bd::HFS),
             board.full_scale_current_ma,
             static_cast<unsigned long>(board.max_current_ma),
             static_cast<unsigned>(board.max_duty_percent));
    return true;
}

static bool initialize_driver() noexcept {
    ESP_LOGI(TAG, "Initializing MAX22200 driver...");

    // ─── Why this looks paranoid ────────────────────────────────────
    // The MAX22200 latches UVM=1 at POR. Per datasheet UVM is "cleared
    // by reading STATUS", *but only if the chip's internal VL (logic
    // supply, generated by the on-die LDO from VM) has settled by the
    // time we issue the read*. The chip ALSO silently rejects any
    // ACTIVE=1 write while UVM is asserted (interlock).
    //
    // The driver's Initialize() waits 500 µs after driving ENABLE HIGH
    // before the first STATUS read. On bench rigs where VL has a slow-
    // ramping bypass cap (or any series element on +VM), 500 µs isn't
    // long enough — UVM is still real-time-asserted at the first read,
    // so the read doesn't clear it, so ACTIVE=1 is rejected, so we
    // sit in low-power mode forever even though Initialize() returns OK.
    //
    // Mitigation: between attempts give VL multiple ms to settle, then
    // explicitly call EnableDevice() (re-asserts ENABLE HIGH) before
    // each Init+verify cycle. If we still can't bring ACTIVE up after
    // a generous retry budget, the +VM/VL hardware path needs review.
    constexpr int      kInitRetries     = 8;
    constexpr int      kReadClears      = 6;
    constexpr uint32_t kPreInitDelayMs  = 25;   // settle VL before first SPI
    constexpr uint32_t kReadGapMs       = 5;    // between read-clear attempts
    constexpr uint32_t kRetryDelayMs    = 100;  // between full retry attempts

    for (int attempt = 0; attempt < kInitRetries; ++attempt) {
        // First attempt only: long settle delay so VL is solid before
        // Initialize() does its first STATUS read.
        if (attempt > 0) {
            (void)g_driver->Deinitialize();
            vTaskDelay(pdMS_TO_TICKS(kRetryDelayMs));
        } else {
            vTaskDelay(pdMS_TO_TICKS(kPreInitDelayMs));
        }

        DriverStatus s = g_driver->Initialize();
        if (s != DriverStatus::OK) {
            ESP_LOGE(TAG, "Driver Initialize failed (attempt %d/%d): %s",
                     attempt + 1, kInitRetries, DriverStatusToStr(s));
            continue;
        }

        // Re-assert ENABLE (no-op if already HIGH) and give the chip
        // more time before we go hammering reads at it.
        (void)g_driver->EnableDevice();
        vTaskDelay(pdMS_TO_TICKS(kPreInitDelayMs));

        // Drain POR-latched fault bits with several read-clears.
        StatusConfig st{};
        for (int i = 0; i < kReadClears; ++i) {
            (void)g_driver->ReadStatus(st);
            if (!st.undervoltage) break;  // UVM cleared, no need to keep retrying
            vTaskDelay(pdMS_TO_TICKS(kReadGapMs));
        }

        // If UVM finally cleared but we still haven't set ACTIVE, try
        // Re-Init: drives the ACTIVE=1 write while UVM is now down.
        if (!st.undervoltage && !st.active) {
            ESP_LOGI(TAG, "  UVM cleared on attempt %d; re-asserting ACTIVE=1",
                     attempt + 1);
            (void)g_driver->Deinitialize();
            vTaskDelay(pdMS_TO_TICKS(2));
            (void)g_driver->Initialize();
            (void)g_driver->ReadStatus(st);
        }

        if (st.active && !st.undervoltage) {
            ESP_LOGI(TAG, "✅ Driver initialized (attempt %d, ACTIVE=1, UVM=0)",
                     attempt + 1);
            return true;
        }

        ESP_LOGW(TAG,
                 "Init attempt %d/%d landed with ACTIVE=%d UVM=%d — retrying",
                 attempt + 1, kInitRetries, st.active, st.undervoltage);
    }

    ESP_LOGE(TAG,
             "Failed to bring MAX22200 to ACTIVE=1 after %d attempts. "
             "The chip's UVM comparator is reporting VM/VL undervoltage "
             "in real time. Check at the bench:\n"
             "  1. VM pin voltage WHILE the test runs (scope, not just DMM)\n"
             "  2. VL pin bypass capacitor (typ. 1 µF X7R required)\n"
             "  3. Any series element (fuse, sense R, diode) between the "
             "+24V rail and the chip's VM pin",
             kInitRetries);
    return false;
}

static bool configure_c21_channel() noexcept {
    ESP_LOGI(TAG, "Configuring CH%u (CDR low-side, hit=%u mA, hold=%u mA, hit_time=%.0f ms)",
             cfg::kChannel,
             cfg::kHitCurrent_mA,
             cfg::kHoldCurrent_mA,
             static_cast<double>(cfg::kHitTime_ms));

    const DriverStatus s = g_driver->ConfigureChannelCdr(
        cfg::kChannel,
        cfg::kHitCurrent_mA,
        cfg::kHoldCurrent_mA,
        cfg::kHitTime_ms,
        SideMode::LOW_SIDE,
        cfg::kChopFreq,
        cfg::kEnableSlewRateControl,
        cfg::kEnableOpenLoadDetection,
        cfg::kEnablePlungerMovementDet,
        cfg::kEnableHitCurrentCheck);
    if (s != DriverStatus::OK) {
        ESP_LOGE(TAG, "ConfigureChannelCdr failed: %s", DriverStatusToStr(s));
        return false;
    }
    ESP_LOGI(TAG, "✅ CH%u configured for CDR hit/hold profile", cfg::kChannel);
    return true;
}

//==============================================================================
// CYCLE DRIVER
//==============================================================================

static void run_cycle(uint32_t cycle_index) noexcept {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "═══ Cycle %u: ENERGISE  CH%u  (hit %u mA → hold %u mA, %u ms) ═══",
             static_cast<unsigned>(cycle_index + 1U),
             cfg::kChannel, cfg::kHitCurrent_mA, cfg::kHoldCurrent_mA,
             static_cast<unsigned>(cfg::kOnDuration_ms));
    if (auto s = g_driver->EnableChannel(cfg::kChannel); s != DriverStatus::OK) {
        ESP_LOGE(TAG, "EnableChannel(%u) failed: %s",
                 cfg::kChannel, DriverStatusToStr(s));
    }
    vTaskDelay(pdMS_TO_TICKS(cfg::kOnDuration_ms));

    ESP_LOGI(TAG, "═══ Cycle %u: RELEASE   CH%u  (%u ms) ═══════════════════════",
             static_cast<unsigned>(cycle_index + 1U),
             cfg::kChannel,
             static_cast<unsigned>(cfg::kOffDuration_ms));
    if (auto s = g_driver->DisableChannel(cfg::kChannel); s != DriverStatus::OK) {
        ESP_LOGE(TAG, "DisableChannel(%u) failed: %s",
                 cfg::kChannel, DriverStatusToStr(s));
    }
    vTaskDelay(pdMS_TO_TICKS(cfg::kOffDuration_ms));
}

//==============================================================================
// app_main
//==============================================================================

extern "C" void app_main() {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "╔══════════════════════════════════════════════════════════════════════╗");
    ESP_LOGI(TAG, "║   MAX22200 — Parker C21 24 V solenoid hit-and-hold cycle test        ║");
    ESP_LOGI(TAG, "║   target: CH%u (OUT0)  profile: %u mA hit / %u mA hold / %.0f ms hit  ║",
             cfg::kChannel,
             cfg::kHitCurrent_mA, cfg::kHoldCurrent_mA,
             static_cast<double>(cfg::kHitTime_ms));
    ESP_LOGI(TAG, "║   ON %u ms  ·  OFF %u ms  ·  telemetry %u ms                          ║",
             static_cast<unsigned>(cfg::kOnDuration_ms),
             static_cast<unsigned>(cfg::kOffDuration_ms),
             static_cast<unsigned>(cfg::kTelemetryPeriod_ms));
    ESP_LOGI(TAG, "╚══════════════════════════════════════════════════════════════════════╝");

    if (!create_bus_and_driver())   return;
    if (!initialize_driver())       return;
    if (!configure_c21_channel())   return;

    // Spawn telemetry BEFORE the first cycle so we capture the initial
    // (released) state.
    g_telemetry_running = true;
    BaseType_t r = xTaskCreate(telemetry_task, "max_tlm", 4096, nullptr,
                               tskIDLE_PRIORITY + 2, nullptr);
    if (r != pdPASS) {
        ESP_LOGE(TAG, "Failed to create telemetry task");
    }

    // Give the telemetry one tick before the first cycle.
    vTaskDelay(pdMS_TO_TICKS(cfg::kTelemetryPeriod_ms));

    uint32_t cycle = 0;
    while (cfg::kCycleCount == 0U || cycle < cfg::kCycleCount) {
        run_cycle(cycle);
        ++cycle;
    }

    ESP_LOGI(TAG, "All %u cycles complete — releasing channel and stopping telemetry",
             static_cast<unsigned>(cycle));
    (void)g_driver->DisableChannel(cfg::kChannel);
    g_telemetry_running = false;
}

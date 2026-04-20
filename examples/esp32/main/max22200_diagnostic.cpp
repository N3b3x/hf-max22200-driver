/**
 * @file max22200_diagnostic.cpp
 * @brief End-to-end MAX22200 health-check on real hardware.
 *
 * @details
 *   Fault-isolation tool — NOT a feature test. Walks every comm path
 *   the datasheet defines (Figures 7, 10, 11, 12, 13) and dumps the
 *   raw register bytes so they can be correlated against scope/DMM
 *   measurements at the chip pins. Use this when:
 *
 *     - The chip refuses to leave low-power mode (ACTIVE=0, UVM=1)
 *     - nFAULT asserts unexpectedly
 *     - SPI round-trip writes don't match expected bytes
 *     - You suspect a hardware-level issue (VL bypass cap, VM rail,
 *       carrier-board grounding) and need a synchronised log /
 *       scope capture to localize it
 *
 *   The test sequence (each step prints raw HEX so it's unambiguous):
 *
 *     0   Pin-level snapshot of ENABLE / CMD / TRIGA/B / FAULT / CS /
 *         MISO BEFORE any bus init — shows the chip's idle state at
 *         MCU boot.
 *
 *     0.5 Hard ENABLE pulse: drive ENABLE LOW for 50 ms before bus
 *         init. Per datasheet, ENABLE LOW puts the chip in low-power
 *         mode (register state preserved). Useful as a known-clean
 *         starting condition.
 *
 *     1   Bus + driver construction (does NOT init the chip yet).
 *
 *     2   driver.Initialize() — drives ENABLE HIGH, runs the datasheet
 *         init flow (read STATUS to clear UVM, write ACTIVE=1, verify),
 *         then dumps pin levels again.
 *
 *     3   Three consecutive raw STATUS reads — confirms the value is
 *         stable (chip's state machine is healthy).
 *
 *     4   CFG_CH0 round-trip (write 0x28500600, read back, compare):
 *         the canonical SPI sanity check. Confirms cmd-byte format,
 *         byte order, CMD pin sequencing, and CS timing are correct.
 *
 *     5   FAULT register read — clears OCP / HHF / OLF / DPM bits per
 *         datasheet (Figure 8 / standard clear-on-read).
 *
 *     6   STATUS read after FAULT clear — confirms nFAULT releases.
 *
 *     7   Two ACTIVE=1 write variants (0x00000001 and 0x00040001) and
 *         a second CFG_CH0 round-trip after wake-up to confirm both
 *         STATUS and CFG_CHx writes land.
 *
 *     8   Continuous STATUS + FAULT dump @ 5 Hz forever, with a
 *         periodic re-write of ACTIVE=1 every 2 s. Lets the bench
 *         operator probe VM / VL / ENABLE / CMD / nFAULT in real time.
 *
 *   Build / flash:
 *       ./scripts/build_app.sh max22200_diagnostic Debug
 *       ./scripts/flash_app.sh flash_monitor max22200_diagnostic Debug
 *
 * @par Expected healthy output
 *   - Step 2: `Initialize() → OK   (initialized_=1)`
 *   - Step 3: STATUS reads stable across all three samples
 *   - Step 4: `Read CFG_CH0 = 0x28500600  ✅ ROUND-TRIP MATCH`
 *   - Step 7: every write reads back exactly what was written
 *   - Step 8: STATUS = 0x00000001 (ACTIVE=1 only) most of the time,
 *            FAULT cleanly cleared, nFAULT released
 *
 *   Any deviation from the above points at a hardware issue — see
 *   `docs/troubleshooting.md` for the recovery procedure.
 *
 * @author HardFOC
 * @date   2026
 */

// Diagnostic always runs with full SPI hex logging — every TX/RX byte
// goes to the console so we can see exactly what's on the wire.
#define ESP32_MAX22200_ENABLE_DETAILED_SPI_LOGGING 1
#define ESP32_MAX22200_ENABLE_VERBOSE_BUS_LOGGING  1

#include <cinttypes>
#include <cstdio>
#include <memory>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"

#include "esp32_max22200_bus.hpp"
#include "esp32_max22200_test_config.hpp"
#include "max22200.hpp"
#include "max22200_registers.hpp"
#include "max22200_types.hpp"

using namespace max22200;
using namespace MAX22200_TestConfig;

static const char* TAG = "MaxDiag";

//==============================================================================
// HELPERS
//==============================================================================

/// Pretty-print a 32-bit STATUS register value with all fields decoded.
static void log_status(const char* label, uint32_t raw) noexcept {
    const uint8_t b0 = static_cast<uint8_t>(raw         & 0xFF);  // [7:0]   fault flags + ACTIVE
    const uint8_t b1 = static_cast<uint8_t>((raw >> 8 ) & 0xFF);  // [15:8]  channel-pair modes
    const uint8_t b2 = static_cast<uint8_t>((raw >> 16) & 0xFF);  // [23:16] fault masks + FREQM
    const uint8_t b3 = static_cast<uint8_t>((raw >> 24) & 0xFF);  // [31:24] channels-on
    (void)b1;
    ESP_LOGI(TAG, "  %s STATUS = 0x%08" PRIX32 "  bytes=[%02X %02X %02X %02X]",
             label, raw, b3, b2, b1, b0);
    ESP_LOGI(TAG,
             "    flags(b0)=ACTIVE=%d UVM=%d COMER=%d DPM=%d HHF=%d OLF=%d OCP=%d OVT=%d",
             (b0 >> 0) & 1, (b0 >> 1) & 1, (b0 >> 2) & 1, (b0 >> 3) & 1,
             (b0 >> 4) & 1, (b0 >> 5) & 1, (b0 >> 6) & 1, (b0 >> 7) & 1);
    ESP_LOGI(TAG,
             "    mask(b2)=M_OVT=%d M_OCP=%d M_OLF=%d M_HHF=%d M_DPM=%d M_COMF=%d M_UVM=%d  FREQM=%d",
             (b2 >> 7) & 1, (b2 >> 6) & 1, (b2 >> 5) & 1, (b2 >> 4) & 1,
             (b2 >> 3) & 1, (b2 >> 2) & 1, (b2 >> 1) & 1, (b2 >> 0) & 1);
    ESP_LOGI(TAG, "    channels_on(b3)=0x%02X", b3);
}

/// Pretty-print the FAULT register value.
/// Per `max22200_registers.hpp`, byte layout is (MSB→LSB):
///   bits 31:24 = OCP   bits 23:16 = HHF
///   bits 15:8  = OLF   bits  7:0  = DPM
static void log_fault(const char* label, uint32_t raw) noexcept {
    const uint8_t ocp = static_cast<uint8_t>((raw >> 24) & 0xFF);
    const uint8_t hhf = static_cast<uint8_t>((raw >> 16) & 0xFF);
    const uint8_t olf = static_cast<uint8_t>((raw >> 8 ) & 0xFF);
    const uint8_t dpm = static_cast<uint8_t>( raw        & 0xFF);
    ESP_LOGI(TAG, "  %s FAULT = 0x%08" PRIX32 "  OCP=0x%02X HHF=0x%02X OLF=0x%02X DPM=0x%02X",
             label, raw, ocp, hhf, olf, dpm);
}

/// Snapshot the levels of every MAX22200 control + SPI pin via
/// `gpio_get_level()`. Note: pins configured as `GPIO_MODE_OUTPUT`
/// will read 0 because their input function is disconnected.
static void log_pin_levels(const char* label) noexcept {
    using ControlPins = MAX22200_TestConfig::ControlPins;
    using SPIPins     = MAX22200_TestConfig::SPIPins;
    ESP_LOGI(TAG,
             "  %s pin levels: ENABLE(GPIO%d)=%d  CMD(GPIO%d)=%d  "
             "TRIGA(GPIO%d)=%d  TRIGB(GPIO%d)=%d  FAULT(GPIO%d)=%d  "
             "CS(GPIO%d)=%d  MISO(GPIO%d)=%d",
             label,
             ControlPins::ENABLE, gpio_get_level(static_cast<gpio_num_t>(ControlPins::ENABLE)),
             ControlPins::CMD,    gpio_get_level(static_cast<gpio_num_t>(ControlPins::CMD)),
             ControlPins::TRIGA,  gpio_get_level(static_cast<gpio_num_t>(ControlPins::TRIGA)),
             ControlPins::TRIGB,  gpio_get_level(static_cast<gpio_num_t>(ControlPins::TRIGB)),
             ControlPins::FAULT,  gpio_get_level(static_cast<gpio_num_t>(ControlPins::FAULT)),
             SPIPins::CS,         gpio_get_level(static_cast<gpio_num_t>(SPIPins::CS)),
             SPIPins::MISO,       gpio_get_level(static_cast<gpio_num_t>(SPIPins::MISO)));
}

//==============================================================================
// app_main
//==============================================================================

extern "C" void app_main() {
    vTaskDelay(pdMS_TO_TICKS(500));   // let the USB-JTAG console settle
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "╔══════════════════════════════════════════════════════════════════════╗");
    ESP_LOGI(TAG, "║   MAX22200 — End-to-end comm and chip-state diagnostic               ║");
    ESP_LOGI(TAG, "║   Walks the datasheet init flow, round-trips CFG_CH0 to verify SPI,  ║");
    ESP_LOGI(TAG, "║   exercises both STATUS write paths, then continuous STATUS dump.    ║");
    ESP_LOGI(TAG, "╚══════════════════════════════════════════════════════════════════════╝");

    //------------------------------------------------------------------
    // Step 0: pin levels at MCU boot (before bus init)
    //------------------------------------------------------------------
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "── Step 0: pin level snapshot BEFORE bus init ───────────────────────");
    log_pin_levels("[boot]");

    //------------------------------------------------------------------
    // Step 0.5: drive ENABLE LOW for 50 ms before bus init
    //
    //   Per datasheet, ENABLE LOW enters low-power mode (chip stays in
    //   sleep, register contents preserved across ENABLE cycles). Used
    //   here as a known starting state for the rest of the diagnostic.
    //------------------------------------------------------------------
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "── Step 0.5: force ENABLE LOW for 50 ms (low-power start) ────────────");
    {
        using ControlPins = MAX22200_TestConfig::ControlPins;
        gpio_config_t cfg = {
            .pin_bit_mask = (1ULL << ControlPins::ENABLE),
            .mode         = GPIO_MODE_OUTPUT,
            .pull_up_en   = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type    = GPIO_INTR_DISABLE
        };
        gpio_config(&cfg);
        gpio_set_level(static_cast<gpio_num_t>(ControlPins::ENABLE), 0);
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    //------------------------------------------------------------------
    // Step 1: bus + driver construction (no chip access yet)
    //------------------------------------------------------------------
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "── Step 1: bus + driver construction ────────────────────────────────");
    auto bus = CreateEsp32Max22200SpiBus();
    if (!bus) { ESP_LOGE(TAG, "Bus creation failed"); return; }

    using Bd = MAX22200_TestConfig::BoardTestConfig;
    BoardConfig board(Bd::RREF_KOHM, Bd::HFS);
    board.max_current_ma   = Bd::MAX_CURRENT_MA;
    board.max_duty_percent = Bd::MAX_DUTY_PERCENT;
    auto driver = std::make_unique<MAX22200<Esp32Max22200SpiBus>>(*bus, board);
    if (!driver) { ESP_LOGE(TAG, "Driver allocation failed"); return; }
    ESP_LOGI(TAG, "  bus=%p  driver=%p  IFS_full_scale=%" PRIu32 " mA",
             static_cast<void*>(bus.get()),
             static_cast<void*>(driver.get()),
             board.full_scale_current_ma);

    //------------------------------------------------------------------
    // Step 2: driver.Initialize() — datasheet init flow
    //------------------------------------------------------------------
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "── Step 2: driver.Initialize() (drives ENABLE HIGH, sets ACTIVE=1) ──");
    const auto init_rc = driver->Initialize();
    ESP_LOGI(TAG, "  Initialize() → %s   (initialized_=%d)",
             DriverStatusToStr(init_rc), driver->IsInitialized());
    log_pin_levels("[post-init]");

    if (!driver->IsInitialized()) {
        ESP_LOGE(TAG, "Cannot continue — driver did not initialize");
        return;
    }

    //------------------------------------------------------------------
    // Step 3: three consecutive raw STATUS reads (must all match)
    //------------------------------------------------------------------
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "── Step 3: three consecutive raw STATUS reads ────────────────────────");
    for (int i = 0; i < 3; ++i) {
        uint32_t raw = 0;
        if (driver->ReadRegister32(RegBank::STATUS, raw) == DriverStatus::OK) {
            char label[32];
            std::snprintf(label, sizeof(label), "[read #%d]", i + 1);
            log_status(label, raw);
        } else {
            ESP_LOGE(TAG, "  ReadRegister32(STATUS) failed");
        }
    }

    //------------------------------------------------------------------
    // Step 4: CFG_CH0 round-trip — canonical SPI sanity check
    //------------------------------------------------------------------
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "── Step 4: CFG_CH0 round-trip ───────────────────────────────────────");
    constexpr uint32_t kCfgCh0_Pattern = 0x28500600U;  // example from spi_protocol_analysis.md
    if (auto rc = driver->WriteRegister32(RegBank::CFG_CH0, kCfgCh0_Pattern);
        rc != DriverStatus::OK) {
        ESP_LOGE(TAG, "  WriteRegister32(CFG_CH0, 0x%08" PRIX32 ") failed: %s",
                 kCfgCh0_Pattern, DriverStatusToStr(rc));
    } else {
        ESP_LOGI(TAG, "  Wrote CFG_CH0 = 0x%08" PRIX32, kCfgCh0_Pattern);
    }
    {
        uint32_t raw = 0;
        if (driver->ReadRegister32(RegBank::CFG_CH0, raw) == DriverStatus::OK) {
            ESP_LOGI(TAG, "  Read  CFG_CH0 = 0x%08" PRIX32 "  %s",
                     raw,
                     raw == kCfgCh0_Pattern
                         ? "✅ ROUND-TRIP MATCH"
                         : "❌ MISMATCH — SPI byte order / CMD pin / CS timing problem");
        }
    }

    //------------------------------------------------------------------
    // Step 5: FAULT register clear-by-read
    //------------------------------------------------------------------
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "── Step 5: FAULT register clear-by-read ─────────────────────────────");
    {
        uint32_t raw = 0;
        if (driver->ReadRegister32(RegBank::FAULT, raw) == DriverStatus::OK) {
            log_fault("[FAULT initial read]", raw);
        }
    }

    //------------------------------------------------------------------
    // Step 6: STATUS read after FAULT clear (nFAULT should release)
    //------------------------------------------------------------------
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "── Step 6: STATUS after FAULT-clear ─────────────────────────────────");
    {
        uint32_t raw = 0;
        if (driver->ReadRegister32(RegBank::STATUS, raw) == DriverStatus::OK) {
            log_status("[post-FAULT-clear]", raw);
        }
    }

    //------------------------------------------------------------------
    // Step 7: STATUS write variants
    //   (a) bare ACTIVE bit       → minimal write
    //   (b) ACTIVE + M_COMF mask  → driver's default write pattern
    //   (c) CFG_CH0 round-trip again with a different value
    //------------------------------------------------------------------
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "── Step 7a: write STATUS = 0x00000001 (just ACTIVE) ─────────────────");
    (void)driver->WriteRegister32(RegBank::STATUS, 0x00000001U);
    {
        uint32_t raw = 0;
        if (driver->ReadRegister32(RegBank::STATUS, raw) == DriverStatus::OK) {
            log_status("[after 0x00000001 write]", raw);
        }
    }

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "── Step 7b: write STATUS = 0x00040001 (ACTIVE + M_COMF) ─────────────");
    (void)driver->WriteRegister32(RegBank::STATUS, 0x00040001U);
    {
        uint32_t raw = 0;
        if (driver->ReadRegister32(RegBank::STATUS, raw) == DriverStatus::OK) {
            log_status("[after 0x00040001 write]", raw);
        }
    }

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "── Step 7c: second CFG_CH0 round-trip ─────────────────────────────────");
    constexpr uint32_t kCfgCh0_Pattern2 = 0x10101010U;
    (void)driver->WriteRegister32(RegBank::CFG_CH0, kCfgCh0_Pattern2);
    {
        uint32_t raw = 0;
        if (driver->ReadRegister32(RegBank::CFG_CH0, raw) == DriverStatus::OK) {
            ESP_LOGI(TAG, "  Wrote 0x%08" PRIX32 ", read 0x%08" PRIX32 "  %s",
                     kCfgCh0_Pattern2, raw,
                     raw == kCfgCh0_Pattern2 ? "✅ MATCH" : "❌ MISMATCH");
        }
    }

    //------------------------------------------------------------------
    // Step 8: continuous STATUS + FAULT @ 5 Hz with periodic re-write
    //         of ACTIVE=1. Lets the bench operator probe with a scope
    //         while watching the logs.
    //------------------------------------------------------------------
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "── Step 8: continuous STATUS @ 5 Hz (probe with scope now) ──────────");
    uint32_t tick = 0;
    while (true) {
        uint32_t st = 0, ft = 0;
        const bool st_ok = driver->ReadRegister32(RegBank::STATUS, st) == DriverStatus::OK;
        const bool ft_ok = driver->ReadRegister32(RegBank::FAULT,  ft) == DriverStatus::OK;
        bool fault_pin = false;
        (void)driver->GetFaultPinState(fault_pin);

        if (st_ok && ft_ok) {
            const uint8_t b0 = static_cast<uint8_t>(st & 0xFF);
            ESP_LOGI(TAG,
                     "[t=%4u s+%03u]  STATUS=0x%08" PRIX32 "  FAULT=0x%08" PRIX32
                     "  flags[ACTIVE=%d UVM=%d COMER=%d]  nFAULT=%s",
                     static_cast<unsigned>(tick * 200U / 1000U),
                     static_cast<unsigned>((tick * 200U) % 1000U),
                     st, ft,
                     (b0 >> 0) & 1, (b0 >> 1) & 1, (b0 >> 2) & 1,
                     fault_pin ? "ASSERTED" : "released");
        } else {
            ESP_LOGW(TAG, "[t=%u]  read failed (st_ok=%d ft_ok=%d)",
                     static_cast<unsigned>(tick), st_ok, ft_ok);
        }

        // Re-issue ACTIVE=1 every 2 s to catch any chip-side state drift.
        if ((tick % 10U) == 0U && tick > 0) {
            ESP_LOGI(TAG, "  ── periodic poke: write STATUS=0x01 ──");
            (void)driver->WriteRegister32(RegBank::STATUS, 0x00000001U);
        }

        ++tick;
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

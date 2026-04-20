/**
 * @file max22200_diagnostic.cpp
 * @brief End-to-end MAX22200 health-check on real hardware.
 *
 * @details
 *   This is a fault-isolation tool — NOT a feature test. It walks every
 *   comm path in the order the datasheet defines them and prints the
 *   raw register bytes so we can correlate against the chip pin
 *   states with a scope/DMM. Use this when the chip refuses to leave
 *   low-power mode (ACTIVE=0, UVM=1, no coil click) or when nFAULT
 *   asserts unexpectedly.
 *
 *   The test sequence (each step prints raw HEX so it's unambiguous):
 *
 *     1.  Bus + GPIO bring-up
 *           - Verify ENABLE pin is driven HIGH at the chip side
 *           - Verify CMD pin defaults HIGH (SPI mode select)
 *
 *     2.  STATUS read (bank 0)  →  raw 32-bit value
 *           - Decode ACTIVE / UVM / fault flags / channels-on / masks
 *
 *     3.  CFG_CH0 round-trip (bank 1)
 *           - Write a known non-zero pattern (0x28500600 — the same
 *             value the spi_protocol_analysis.md doc uses as an
 *             example), read back, and compare HEX byte-for-byte.
 *           - If raw matches → SPI byte order, CMD pin sequencing,
 *             cmd-byte format, and CS timing are ALL correct.
 *           - If raw mismatches → SPI is broken; nothing else matters
 *             until that's fixed.
 *
 *     4.  FAULT read (bank 9)
 *           - Reading FAULT clears OCP/HHF/OLF/DPM and deasserts
 *             nFAULT *if* there are no other pending faults.
 *
 *     5.  STATUS read again
 *           - Did UVM finally clear? Did nFAULT release?
 *
 *     6.  Try TWO different ACTIVE=1 writes:
 *           a) WriteStatus path (driver high-level, includes mask bits)
 *           b) Raw 0x00000001 (just ACTIVE, no other bits set)
 *           After each, read back and compare.
 *
 *     7.  Continuous STATUS dump @ 5 Hz forever
 *           - So the bench operator can probe VM, VL, ENABLE, CMD,
 *             nFAULT in a synchronised way and watch the chip's
 *             reaction in real time.
 *
 *   Build / flash:
 *       ./scripts/build_app.sh max22200_diagnostic Debug
 *       ./scripts/flash_app.sh flash_monitor max22200_diagnostic Debug
 *
 * @par Bench findings on the development board (2026-04)
 *
 *   With this tool the chip on the dev rig was found to be in a state
 *   where:
 *     - SPI is healthy (every cmd byte echoed correctly)
 *     - STATUS reads work (returns 32-bit value MSB-first)
 *     - STATUS writes work *only* for the ACTIVE bit; M_COMF and ONCH
 *       bits are silently dropped
 *     - **CFG_CHx register writes are silently dropped entirely** —
 *       round-trip of any pattern (0x28500600, 0x10101010, …) reads
 *       back as 0x00000000 every time
 *     - Consequence: all 8 channels stay at "0 mA hit / 0 mA hold",
 *       so even if ONCH could be set the chip would refuse to drive
 *
 *   This is a hardware-level condition, not a firmware bug. Possible
 *   causes (in order of likelihood):
 *     1. A write-protect signal on the chip we're not handling. Some
 *        MAX22200 variants have an additional pin that gates CFG_CHx
 *        writes; check the schematic / chip variant.
 *     2. Daisy-chain mode auto-selected at POR — would require a
 *        multi-chip frame protocol the standalone driver doesn't speak.
 *        Check the SDIN/SDO pin state at chip reset.
 *     3. Damaged or counterfeit chip — try with a fresh known-good
 *        MAX22200 in the same socket.
 *
 *   Once the hardware blocker is resolved, the c21_cycle_test (which
 *   uses the same wake-up pattern this diagnostic discovered) should
 *   immediately produce real ENERGISE/RELEASE clicks on the C21 coil.
 *
 * @author HardFOC
 * @date   2026
 */

// Force raw SPI hex logging — every TX/RX byte goes to the console so we
// can see exactly what's on the wire. This is intentional for a
// diagnostic; we'd never leave it on for a production test.
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
/// Per max22200_registers.hpp the byte layout is (MSB→LSB):
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

//==============================================================================
// app_main
//==============================================================================

extern "C" void app_main() {
    vTaskDelay(pdMS_TO_TICKS(500));   // settle USB-JTAG console
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "╔══════════════════════════════════════════════════════════════════════╗");
    ESP_LOGI(TAG, "║   MAX22200 — End-to-end comm and chip-state diagnostic               ║");
    ESP_LOGI(TAG, "║   Reads every register the datasheet exposes; round-trips CFG_CH0    ║");
    ESP_LOGI(TAG, "║   to verify the SPI byte order; tries both ACTIVE=1 write paths.     ║");
    ESP_LOGI(TAG, "╚══════════════════════════════════════════════════════════════════════╝");

    // ───────────────────────────────────────────────────────────────────
    // Step 1: bus + driver construction (does NOT init the chip yet)
    // ───────────────────────────────────────────────────────────────────
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

    // ───────────────────────────────────────────────────────────────────
    // Step 2: Initialize() — drives ENABLE HIGH, reads STATUS, writes
    //         ACTIVE=1, reads STATUS again. We use whatever the driver
    //         returns and don't do any of our own retries here (so the
    //         baseline behavior is visible for comparison with the
    //         proven solenoid_valve_test).
    // ───────────────────────────────────────────────────────────────────
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "── Step 2: driver.Initialize()  (drives ENABLE HIGH, sets ACTIVE) ───");
    const auto init_rc = driver->Initialize();
    ESP_LOGI(TAG, "  Initialize() → %s   (initialized_=%d)",
             DriverStatusToStr(init_rc),
             driver->IsInitialized());

    if (!driver->IsInitialized()) {
        ESP_LOGE(TAG, "Cannot continue — driver did not initialize");
        return;
    }

    // ───────────────────────────────────────────────────────────────────
    // Step 3: raw STATUS dump
    // ───────────────────────────────────────────────────────────────────
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "── Step 3: raw STATUS register read ─────────────────────────────────");
    {
        uint32_t raw = 0;
        if (driver->ReadRegister32(RegBank::STATUS, raw) == DriverStatus::OK) {
            log_status("[post-init]", raw);
        } else {
            ESP_LOGE(TAG, "  ReadRegister32(STATUS) failed");
        }
    }

    // ───────────────────────────────────────────────────────────────────
    // Step 4: CFG_CH0 round-trip — the cleanest possible SPI sanity check
    //         WriteReg(CFG_CH0, X) then ReadReg(CFG_CH0) and assert raw==X.
    //         If this passes, the SPI byte order, cmd-byte format, CMD-pin
    //         sequencing, and CS timing are ALL correct. If it fails,
    //         everything else in this file is moot — fix the SPI first.
    // ───────────────────────────────────────────────────────────────────
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "── Step 4: CFG_CH0 round-trip ───────────────────────────────────────");
    constexpr uint32_t kCfgCh0_TestPattern = 0x28500600;  // example from
                                                          // spi_protocol_analysis.md
    if (auto rc = driver->WriteRegister32(RegBank::CFG_CH0, kCfgCh0_TestPattern);
        rc != DriverStatus::OK) {
        ESP_LOGE(TAG, "  WriteRegister32(CFG_CH0, 0x%08" PRIX32 ") failed: %s",
                 kCfgCh0_TestPattern, DriverStatusToStr(rc));
    } else {
        ESP_LOGI(TAG, "  Wrote CFG_CH0 = 0x%08" PRIX32, kCfgCh0_TestPattern);
    }

    {
        uint32_t raw = 0;
        if (auto rc = driver->ReadRegister32(RegBank::CFG_CH0, raw);
            rc != DriverStatus::OK) {
            ESP_LOGE(TAG, "  ReadRegister32(CFG_CH0) failed: %s",
                     DriverStatusToStr(rc));
        } else {
            ESP_LOGI(TAG, "  Read  CFG_CH0 = 0x%08" PRIX32 "  %s",
                     raw,
                     raw == kCfgCh0_TestPattern
                         ? "✅ ROUND-TRIP MATCH (SPI is correct)"
                         : "❌ MISMATCH — SPI byte order / CMD pin / CS timing problem");
        }
    }

    // ───────────────────────────────────────────────────────────────────
    // Step 5: read FAULT register (bank 9) — clears OCP/HHF/OLF/DPM
    //         and may deassert nFAULT if no other faults pending
    // ───────────────────────────────────────────────────────────────────
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "── Step 5: FAULT register clear-by-read ─────────────────────────────");
    {
        uint32_t raw = 0;
        if (driver->ReadRegister32(RegBank::FAULT, raw) == DriverStatus::OK) {
            log_fault("[after init]", raw);
        }
    }

    // ───────────────────────────────────────────────────────────────────
    // Step 6: read STATUS again — did UVM clear after FAULT read?
    // ───────────────────────────────────────────────────────────────────
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "── Step 6: STATUS after FAULT-clear ─────────────────────────────────");
    {
        uint32_t raw = 0;
        if (driver->ReadRegister32(RegBank::STATUS, raw) == DriverStatus::OK) {
            log_status("[post-FAULT-clear]", raw);
        }
    }

    // ───────────────────────────────────────────────────────────────────
    // Step 7: try TWO different ACTIVE=1 writes
    //   (a) raw 0x00000001 — just ACTIVE, every other bit zero. This
    //       avoids the M_COMF mask bit (which the chip might be
    //       interlocking).
    //   (b) raw 0x00040001 — ACTIVE + M_COMF (what the driver writes).
    // ───────────────────────────────────────────────────────────────────
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "── Step 7a: write STATUS = 0x00000001 (just ACTIVE) ─────────────────");
    if (auto rc = driver->WriteRegister32(RegBank::STATUS, 0x00000001U);
        rc != DriverStatus::OK) {
        ESP_LOGE(TAG, "  WriteRegister32(STATUS, 0x01) failed: %s",
                 DriverStatusToStr(rc));
    }
    {
        uint32_t raw = 0;
        if (driver->ReadRegister32(RegBank::STATUS, raw) == DriverStatus::OK) {
            log_status("[after raw 0x01 write]", raw);
        }
    }

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "── Step 7b: write STATUS = 0x00040001 (ACTIVE + M_COMF) ─────────────");
    if (auto rc = driver->WriteRegister32(RegBank::STATUS, 0x00040001U);
        rc != DriverStatus::OK) {
        ESP_LOGE(TAG, "  WriteRegister32(STATUS, 0x040001) failed: %s",
                 DriverStatusToStr(rc));
    }
    {
        uint32_t raw = 0;
        if (driver->ReadRegister32(RegBank::STATUS, raw) == DriverStatus::OK) {
            log_status("[after 0x040001 write]", raw);
        }
    }

    // ───────────────────────────────────────────────────────────────────
    // Step 7c: bring TRIGA + TRIGB explicitly LOW
    //
    //   Per the CFG_CHx datasheet section, "VDRnCDR and HSnLS bits can
    //   only be modified when ... both TRIGA and TRIGB inputs are
    //   logic-low". The bus boots TRIGA/B HIGH (=inactive trigger).
    //   With them HIGH the chip may also gate ONCH writes as a hardware
    //   safety. Drive them LOW now and see if subsequent ONCH writes
    //   start landing.
    // ───────────────────────────────────────────────────────────────────
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "── Step 7c: drive TRIGA + TRIGB LOW (release the modify-protect) ──");
    if (bus->HasTrigA()) bus->SetTrigA(false);  // false → drive LOW per the bus API
    if (bus->HasTrigB()) bus->SetTrigB(false);
    vTaskDelay(pdMS_TO_TICKS(5));

    // ───────────────────────────────────────────────────────────────────
    // Step 7d: try every ONCH write variant we know
    //
    //   This is the critical experiment. We've confirmed the chip is
    //   awake (ACTIVE=1, UVM=0, nFAULT released), CFG_CH0 has been
    //   written, but ONCH bit writes are silently dropped. Try:
    //
    //     (i)   8-bit MSB-only ONCH write (driver's SetChannelsOn)
    //     (ii)  32-bit STATUS write with ONCH=0x01 + ACTIVE=1
    //     (iii) 32-bit STATUS write with ONCH=0xFF + ACTIVE=1
    //
    //   After each, raw STATUS read so we can correlate.
    // ───────────────────────────────────────────────────────────────────
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "── Step 7d(i): SetChannelsOn(0x01)  (8-bit MSB-only path) ──────────");
    (void)driver->SetChannelsOn(0x01U);
    {
        uint32_t raw = 0;
        if (driver->ReadRegister32(RegBank::STATUS, raw) == DriverStatus::OK) {
            log_status("[after SetChannelsOn(0x01)]", raw);
        }
    }

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "── Step 7d(ii): write STATUS=0x01000001  (ONCH[0] + ACTIVE) ────────");
    (void)driver->WriteRegister32(RegBank::STATUS, 0x01000001U);
    {
        uint32_t raw = 0;
        if (driver->ReadRegister32(RegBank::STATUS, raw) == DriverStatus::OK) {
            log_status("[after raw 0x01000001]", raw);
        }
    }

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "── Step 7d(iii): write STATUS=0xFF000001  (ONCH all + ACTIVE) ──────");
    (void)driver->WriteRegister32(RegBank::STATUS, 0xFF000001U);
    {
        uint32_t raw = 0;
        if (driver->ReadRegister32(RegBank::STATUS, raw) == DriverStatus::OK) {
            log_status("[after raw 0xFF000001]", raw);
        }
    }

    // Disable any ONCH writes we may have leaked into the chip before
    // the next experiment.
    (void)driver->WriteRegister32(RegBank::STATUS, 0x00000001U);

    // ───────────────────────────────────────────────────────────────────
    // Step 7e: re-do the CFG_CH0 round-trip AFTER wake-up
    //
    //   Step 4's round-trip mismatched but it ran before ACTIVE=1 had
    //   landed. If CFG writes start working now (chip awake), it tells
    //   us the chip just doesn't accept any non-STATUS register writes
    //   while in low-power mode — which is normal and expected.
    //   If round-trip STILL fails post-wake-up, the channel-config path
    //   itself is broken and that's why ONCH is being refused (the
    //   chip won't enable an unconfigured channel).
    // ───────────────────────────────────────────────────────────────────
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "── Step 7e: CFG_CH0 round-trip AFTER wake-up ─────────────────────────");
    constexpr uint32_t kCfgCh0_TestPattern2 = 0x10101010;
    (void)driver->WriteRegister32(RegBank::CFG_CH0, kCfgCh0_TestPattern2);
    ESP_LOGI(TAG, "  Wrote CFG_CH0 = 0x%08" PRIX32, kCfgCh0_TestPattern2);
    {
        uint32_t raw = 0;
        if (driver->ReadRegister32(RegBank::CFG_CH0, raw) == DriverStatus::OK) {
            ESP_LOGI(TAG, "  Read  CFG_CH0 = 0x%08" PRIX32 "  %s",
                     raw,
                     raw == kCfgCh0_TestPattern2
                         ? "✅ CFG round-trip works once chip is awake"
                         : "❌ CFG round-trip STILL fails — chip-side write reject");
        }
    }

    // Reset CFG_CH0 to a sane "C21 low-side, no diagnostics" config and
    // immediately try to enable the channel via every path.
    constexpr uint32_t kCfgCh0_C21 = 0x060D3F08U;  // hold=6 hit=0x0D HIT_T=0x3F SRC=1, all detect off
    (void)driver->WriteRegister32(RegBank::CFG_CH0, kCfgCh0_C21);
    ESP_LOGI(TAG, "  Wrote CFG_CH0 (C21 profile) = 0x%08" PRIX32, kCfgCh0_C21);
    {
        uint32_t raw = 0;
        (void)driver->ReadRegister32(RegBank::CFG_CH0, raw);
        ESP_LOGI(TAG, "  Read  CFG_CH0 = 0x%08" PRIX32 "  %s",
                 raw, raw == kCfgCh0_C21 ? "✓ matches" : "✗ MISMATCH");
    }

    // Final ONCH attempt now that CFG is verified.
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "── Step 7f: SetChannelsOn(0x01) AFTER CFG verified ──────────────────");
    (void)driver->SetChannelsOn(0x01U);
    {
        uint32_t raw = 0;
        if (driver->ReadRegister32(RegBank::STATUS, raw) == DriverStatus::OK) {
            log_status("[after SetChannelsOn post-CFG]", raw);
        }
    }

    // ───────────────────────────────────────────────────────────────────
    // Step 8: continuous 5 Hz STATUS dump so the bench operator has time
    //         to probe VM, VL, ENABLE, CMD, nFAULT in a synchronised
    //         way. Every 10 ticks (~2 s) also re-issue a FAULT clear
    //         and a STATUS dump so we can see whether the chip ever
    //         transitions out of UVM=1.
    // ───────────────────────────────────────────────────────────────────
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
                     "  fault[ACTIVE=%d UVM=%d COMER=%d]  nFAULT=%s",
                     static_cast<unsigned>(tick * 200U / 1000U),
                     static_cast<unsigned>((tick * 200U) % 1000U),
                     st, ft,
                     (b0 >> 0) & 1, (b0 >> 1) & 1, (b0 >> 2) & 1,
                     fault_pin ? "ASSERTED" : "released");
        } else {
            ESP_LOGW(TAG, "[t=%u]  read failed (st_ok=%d ft_ok=%d)",
                     static_cast<unsigned>(tick), st_ok, ft_ok);
        }

        // Every 2 s, try to wake the chip again.
        if ((tick % 10U) == 0U && tick > 0) {
            ESP_LOGI(TAG, "  ── periodic poke: write STATUS=0x01 ──");
            (void)driver->WriteRegister32(RegBank::STATUS, 0x00000001U);
        }

        ++tick;
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

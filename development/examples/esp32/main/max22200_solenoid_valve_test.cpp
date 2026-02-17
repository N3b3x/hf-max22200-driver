/**
 * @file max22200_solenoid_valve_test.cpp
 * @brief Dedicated solenoid/valve test for MAX22200 driver on ESP32
 *
 * Full driver check on valves: configures all 8 channels for the same valve
 * profile (C21-style: 100 ms hit, 50% hold, low-side CDR or VDR per
 * C21ValveConfig). Runs synchronized patterns (sequential follow-up clicking,
 * parallel clicking) and logs comprehensive diagnostics: STATUS, FAULT,
 * last fault byte, nFAULT pin, per-channel config readback, board config,
 * and driver statistics.
 *
 * @details
 * - Board: BoardTestConfig (RREF_KOHM, HFS). Channel profile: C21ValveConfig (USE_CDR,
 *   HIT_TIME_MS, hit/hold currents or percent). All channels get the same configuration.
 * - Pattern timings: SolenoidValvePatternConfig (SEQUENTIAL_HIT_MS, SEQUENTIAL_GAP_MS,
 *   PARALLEL_HOLD_MS, PATTERN_PAUSE_MS, LOOP_COUNT). (1) Sequential: ch0 → ch1 → … → ch7 with delay between each.
 *   (2) Parallel: all channels on together, then off. LOOP_COUNT: 1 = one-shot, N = N times, 0 = infinite.
 * - Diagnostics: ReadStatus, ReadFaultRegister, GetLastFaultByte, GetFaultPinState,
 *   GetChannelConfig per channel, GetBoardConfig, GetStatistics; formatted
 *   logging with section headers and tables for easy interpretation.
 *
 * @par Build
 * From examples/esp32: `./scripts/build_app.sh max22200_solenoid_valve_test Debug`
 *
 * @author HardFOC Development Team
 * @date 2025
 * @copyright HardFOC
 */

#include <cinttypes>
#include <cmath>
#include <memory>

#include "esp32_max22200_bus.hpp"
#include "esp32_max22200_test_config.hpp"
#include "max22200.hpp"
#include "max22200_registers.hpp"
#include "max22200_types.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "esp_log.h"
#ifdef __cplusplus
}
#endif

using namespace max22200;
using namespace MAX22200_TestConfig;

static const char *TAG = "MAX22200_Valve";

//=============================================================================
// SHARED RESOURCES
//=============================================================================

static std::unique_ptr<Esp32Max22200SpiBus> g_spi_interface;
static std::unique_ptr<MAX22200<Esp32Max22200SpiBus>> g_driver;

//=============================================================================
// HELPERS
//=============================================================================

static bool require_ok(DriverStatus status, const char *op) {
  if (status != DriverStatus::OK) {
    ESP_LOGE(TAG, "%s failed: %s", op, DriverStatusToStr(status));
    return false;
  }
  return true;
}

/** Build C21-style ChannelConfig for low-side valve (CDR or VDR per C21ValveConfig). IFS comes from board (SetBoardConfig). */
static ChannelConfig make_valve_channel_config() {
  ChannelConfig config;
  config.drive_mode = C21ValveConfig::USE_CDR ? DriveMode::CDR : DriveMode::VDR;
  config.side_mode = SideMode::LOW_SIDE;
  config.hit_time_ms = C21ValveConfig::HIT_TIME_MS;
  config.half_full_scale = false;
  config.trigger_from_pin = false;
  config.chop_freq = ChopFreq::FMAIN_DIV4;  // ≥1 kHz per C21
  config.slew_rate_control_enabled = false;
  config.open_load_detection_enabled = false;
  config.plunger_movement_detection_enabled = false;
  config.hit_current_check_enabled = false;

  if (C21ValveConfig::USE_CDR) {
    config.hit_setpoint = C21ValveConfig::HIT_CURRENT_MA;
    config.hold_setpoint = C21ValveConfig::HOLD_CURRENT_MA;
  } else {
    config.hit_setpoint = C21ValveConfig::HIT_PERCENT;
    config.hold_setpoint = C21ValveConfig::HOLD_PERCENT;
  }
  return config;
}

//=============================================================================
// DIAGNOSTICS LOGGING
//=============================================================================

/** Decode STATUS[7:0] fault byte for logging (ACTIVE, UVM, COMER, DPM, HHF, OLF, OCP, OVT) */
static void log_fault_byte(uint8_t fault_byte) {
  const bool active   = (fault_byte & (1u << 0)) != 0;
  const bool ovt      = (fault_byte & (1u << 1)) != 0;
  const bool ocp      = (fault_byte & (1u << 2)) != 0;
  const bool olf      = (fault_byte & (1u << 3)) != 0;
  const bool hhf      = (fault_byte & (1u << 4)) != 0;
  const bool dpm      = (fault_byte & (1u << 5)) != 0;
  const bool comer    = (fault_byte & (1u << 6)) != 0;
  const bool uvm      = (fault_byte & (1u << 7)) != 0;
  ESP_LOGI(TAG, "  Fault byte 0x%02" PRIX8 "  ACTIVE=%d OVT=%d OCP=%d OLF=%d HHF=%d DPM=%d COMER=%d UVM=%d",
           fault_byte, active ? 1 : 0, ovt ? 1 : 0, ocp ? 1 : 0, olf ? 1 : 0, hhf ? 1 : 0, dpm ? 1 : 0, comer ? 1 : 0, uvm ? 1 : 0);
}

/** Full diagnostics: STATUS, FAULT, last fault byte, nFAULT, channel configs, board config, statistics */
static void log_diagnostics(const char *phase) {
  if (!g_driver || !g_driver->IsInitialized()) return;

  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "┌─────────────────────────────────────────────────────────────────────────┐");
  ESP_LOGI(TAG, "│ DIAGNOSTICS  %-51s │", phase);
  ESP_LOGI(TAG, "└─────────────────────────────────────────────────────────────────────────┘");

  StatusConfig status;
  if (g_driver->ReadStatus(status) != DriverStatus::OK) {
    ESP_LOGE(TAG, "  ReadStatus failed");
    return;
  }
  ESP_LOGI(TAG, "  STATUS  ACTIVE=%d  channels_on=0x%02" PRIX8 "  FREQM=%d",
           status.active ? 1 : 0, status.channels_on_mask, status.master_clock_80khz ? 1 : 0);
  ESP_LOGI(TAG, "  Fault flags: OVT=%d OCP=%d OLF=%d HHF=%d DPM=%d COMER=%d UVM=%d",
           status.overtemperature ? 1 : 0, status.overcurrent ? 1 : 0, status.open_load_fault ? 1 : 0,
           status.hit_not_reached ? 1 : 0, status.plunger_movement_fault ? 1 : 0,
           status.communication_error ? 1 : 0, status.undervoltage ? 1 : 0);

  FaultStatus faults;
  if (g_driver->ReadFaultRegister(faults) != DriverStatus::OK) {
    ESP_LOGE(TAG, "  ReadFaultRegister failed");
  } else {
    ESP_LOGI(TAG, "  FAULT   OCP=0x%02" PRIX8 "  HHF=0x%02" PRIX8 "  OLF=0x%02" PRIX8 "  DPM=0x%02" PRIX8,
             faults.overcurrent_channel_mask, faults.hit_not_reached_channel_mask,
             faults.open_load_fault_channel_mask, faults.plunger_movement_fault_channel_mask);
    if (faults.hasFault()) {
      ESP_LOGI(TAG, "  Per-channel: OCP [%d%d%d%d%d%d%d%d]  HHF [%d%d%d%d%d%d%d%d]  OLF [%d%d%d%d%d%d%d%d]  DPM [%d%d%d%d%d%d%d%d]",
               (faults.overcurrent_channel_mask >> 0) & 1, (faults.overcurrent_channel_mask >> 1) & 1,
               (faults.overcurrent_channel_mask >> 2) & 1, (faults.overcurrent_channel_mask >> 3) & 1,
               (faults.overcurrent_channel_mask >> 4) & 1, (faults.overcurrent_channel_mask >> 5) & 1,
               (faults.overcurrent_channel_mask >> 6) & 1, (faults.overcurrent_channel_mask >> 7) & 1,
               (faults.hit_not_reached_channel_mask >> 0) & 1, (faults.hit_not_reached_channel_mask >> 1) & 1,
               (faults.hit_not_reached_channel_mask >> 2) & 1, (faults.hit_not_reached_channel_mask >> 3) & 1,
               (faults.hit_not_reached_channel_mask >> 4) & 1, (faults.hit_not_reached_channel_mask >> 5) & 1,
               (faults.hit_not_reached_channel_mask >> 6) & 1, (faults.hit_not_reached_channel_mask >> 7) & 1,
               (faults.open_load_fault_channel_mask >> 0) & 1, (faults.open_load_fault_channel_mask >> 1) & 1,
               (faults.open_load_fault_channel_mask >> 2) & 1, (faults.open_load_fault_channel_mask >> 3) & 1,
               (faults.open_load_fault_channel_mask >> 4) & 1, (faults.open_load_fault_channel_mask >> 5) & 1,
               (faults.open_load_fault_channel_mask >> 6) & 1, (faults.open_load_fault_channel_mask >> 7) & 1,
               (faults.plunger_movement_fault_channel_mask >> 0) & 1, (faults.plunger_movement_fault_channel_mask >> 1) & 1,
               (faults.plunger_movement_fault_channel_mask >> 2) & 1, (faults.plunger_movement_fault_channel_mask >> 3) & 1,
               (faults.plunger_movement_fault_channel_mask >> 4) & 1, (faults.plunger_movement_fault_channel_mask >> 5) & 1,
               (faults.plunger_movement_fault_channel_mask >> 6) & 1, (faults.plunger_movement_fault_channel_mask >> 7) & 1);
    }
  }

  uint8_t last_fault = g_driver->GetLastFaultByte();
  ESP_LOGI(TAG, "  Last fault byte (from Command Reg):");
  log_fault_byte(last_fault);

  bool fault_pin = false;
  if (g_driver->GetFaultPinState(fault_pin) == DriverStatus::OK) {
    ESP_LOGI(TAG, "  nFAULT pin: %s", fault_pin ? "FAULT_ACTIVE (low)" : "no fault");
  }

  const bool any_status_fault = status.overtemperature || status.overcurrent || status.open_load_fault
      || status.hit_not_reached || status.plunger_movement_fault || status.communication_error || status.undervoltage;
  if (fault_pin || any_status_fault || faults.hasFault()) {
    ESP_LOGI(TAG, "  *** nFAULT/FAULTS ACTIVE — POSSIBLE CAUSES ***");
    if (status.undervoltage)
      ESP_LOGI(TAG, "  >>> UVM: Undervoltage — check VM supply and wiring");
    if (status.communication_error)
      ESP_LOGI(TAG, "  >>> COMER: SPI communication error — check CS, CMD, MISO");
    if (status.overtemperature)
      ESP_LOGI(TAG, "  >>> OVT: Overtemperature — check die/cooling");
    if (status.overcurrent)
      ESP_LOGI(TAG, "  >>> OCP: Overcurrent — short or overload");
    if (status.open_load_fault)
      ESP_LOGI(TAG, "  >>> OLF: Open load — solenoid disconnected or broken wire");
    if (status.hit_not_reached)
      ESP_LOGI(TAG, "  >>> HHF: Hit current not reached — check supply/load/wiring");
    if (status.plunger_movement_fault)
      ESP_LOGI(TAG, "  >>> DPM: Plunger movement fault");
    if (faults.overcurrent_channel_mask)
      ESP_LOGI(TAG, "  >>> OCP per-ch 0x%02" PRIX8 " — short/overcurrent on channel(s)", faults.overcurrent_channel_mask);
    if (faults.hit_not_reached_channel_mask)
      ESP_LOGI(TAG, "  >>> HHF per-ch 0x%02" PRIX8 " — hit current not reached", faults.hit_not_reached_channel_mask);
    if (faults.open_load_fault_channel_mask)
      ESP_LOGI(TAG, "  >>> OLF per-ch 0x%02" PRIX8 " — open load / disconnected solenoid", faults.open_load_fault_channel_mask);
    if (faults.plunger_movement_fault_channel_mask)
      ESP_LOGI(TAG, "  >>> DPM per-ch 0x%02" PRIX8 " — plunger movement", faults.plunger_movement_fault_channel_mask);
    for (uint8_t ch = 0; ch < BoardTestConfig::NUM_CHANNELS; ch++) {
      const bool ocp = (faults.overcurrent_channel_mask & (1u << ch)) != 0;
      const bool hhf = (faults.hit_not_reached_channel_mask & (1u << ch)) != 0;
      const bool olf = (faults.open_load_fault_channel_mask & (1u << ch)) != 0;
      const bool dpm = (faults.plunger_movement_fault_channel_mask & (1u << ch)) != 0;
      if (ocp || hhf || olf || dpm)
        ESP_LOGI(TAG, "      CH%u: %s%s%s%s", ch, ocp ? "OCP " : "", hhf ? "HHF " : "", olf ? "OLF " : "", dpm ? "DPM" : "");
    }
    ESP_LOGI(TAG, "  Legend: UVM=undervoltage OCP=short/overcurrent OLF=open/disconnected HHF=hit not reached DPM=plunger COMER=SPI OVT=thermal");
  }

  BoardConfig board = g_driver->GetBoardConfig();
  ESP_LOGI(TAG, "  Channel config readback (summary):");
  for (uint8_t ch = 0; ch < BoardTestConfig::NUM_CHANNELS; ch++) {
    ChannelConfig cfg;
    if (g_driver->GetChannelConfig(ch, cfg) != DriverStatus::OK) continue;
    uint32_t raw = cfg.toRegister(board.full_scale_current_ma, status.master_clock_80khz);
    ESP_LOGI(TAG, "    CH%u  raw=0x%08" PRIX32 "  hit=%.1f hold=%.1f hit_time_ms=%.1f %s %s",
             ch, raw, cfg.hit_setpoint, cfg.hold_setpoint, cfg.hit_time_ms,
             cfg.drive_mode == DriveMode::CDR ? "CDR" : "VDR",
             cfg.side_mode == SideMode::LOW_SIDE ? "LS" : "HS");
  }

  ESP_LOGI(TAG, "  BoardConfig  IFS=%" PRIu32 " mA  max_current_ma=%" PRIu32 "  max_duty%%=%" PRIu8,
           board.full_scale_current_ma, board.max_current_ma, board.max_duty_percent);

  DriverStatistics stats = g_driver->GetStatistics();
  ESP_LOGI(TAG, "  DriverStats  transfers=%" PRIu32 "  failed=%" PRIu32 "  faults=%" PRIu32 "  state_changes=%" PRIu32 "  uptime_ms=%" PRIu32 "  success%%=%.1f",
           stats.total_transfers, stats.failed_transfers, stats.fault_events, stats.state_changes, stats.uptime_ms, stats.getSuccessRate());
  ESP_LOGI(TAG, "");
}

//=============================================================================
// PATTERNS
//=============================================================================

/** Sequential: ch0 → ch1 → … → ch7; timings from SolenoidValvePatternConfig. */
static void run_sequential_pattern() {
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "  ═══ SEQUENTIAL (follow-up clicking)  ch0 → ch1 → … → ch7  ═══");
  for (uint8_t ch = 0; ch < BoardTestConfig::NUM_CHANNELS; ch++) {
    if (g_driver->EnableChannel(ch) != DriverStatus::OK) {
      ESP_LOGE(TAG, "  EnableChannel(%u) failed", ch);
      continue;
    }
    ESP_LOGI(TAG, "  CH%u ON", ch);
    vTaskDelay(pdMS_TO_TICKS(SolenoidValvePatternConfig::SEQUENTIAL_HIT_MS));
    if (g_driver->DisableChannel(ch) != DriverStatus::OK) {
      ESP_LOGE(TAG, "  DisableChannel(%u) failed", ch);
    }
    vTaskDelay(pdMS_TO_TICKS(SolenoidValvePatternConfig::SEQUENTIAL_GAP_MS));
  }
  ESP_LOGI(TAG, "  Sequential pattern done.");
}

/** Parallel: all channels on, hold SolenoidValvePatternConfig::PARALLEL_HOLD_MS, then all off */
static void run_parallel_pattern() {
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "  ═══ PARALLEL (all channels on together)  ═══");
  if (g_driver->SetChannelsOn(0xFF) != DriverStatus::OK) {
    ESP_LOGE(TAG, "  SetChannelsOn(0xFF) failed");
    return;
  }
  ESP_LOGI(TAG, "  All channels ON");
  vTaskDelay(pdMS_TO_TICKS(SolenoidValvePatternConfig::PARALLEL_HOLD_MS));
  if (g_driver->SetChannelsOn(0) != DriverStatus::OK) {
    ESP_LOGE(TAG, "  SetChannelsOn(0) failed");
  }
  ESP_LOGI(TAG, "  All channels OFF");
  ESP_LOGI(TAG, "  Parallel pattern done.");
}

//=============================================================================
// INIT / CLEANUP
//=============================================================================

static bool init_driver_and_valve_config() {
  g_spi_interface = CreateEsp32Max22200SpiBus();
  if (!g_spi_interface) {
    ESP_LOGE(TAG, "Failed to create SPI interface");
    return false;
  }
  BoardConfig board(BoardTestConfig::RREF_KOHM, BoardTestConfig::HFS);
  g_driver = std::make_unique<MAX22200<Esp32Max22200SpiBus>>(*g_spi_interface, board);
  if (!g_driver) {
    ESP_LOGE(TAG, "Failed to create driver");
    return false;
  }

  ESP_LOGI(TAG, "Pins: MISO=%d MOSI=%d SCLK=%d CS=%d  EN=%d FAULT=%d CMD=%d",
           SPIPins::MISO, SPIPins::MOSI, SPIPins::SCLK, SPIPins::CS,
           ControlPins::ENABLE, ControlPins::FAULT, ControlPins::CMD);

  if (!require_ok(g_driver->Initialize(), "Initialize()")) return false;
  if (!g_driver->IsInitialized()) {
    ESP_LOGE(TAG, "Driver not initialized after Initialize()");
    return false;
  }
  if (!require_ok(g_driver->EnableDevice(), "EnableDevice()")) return false;

  ChannelConfig valve_cfg = make_valve_channel_config();
  ESP_LOGI(TAG, "Valve config: %s  hit=%.1f hold=%.1f hit_time_ms=%.1f (C21-style)",
           C21ValveConfig::USE_CDR ? "CDR" : "VDR",
           valve_cfg.hit_setpoint, valve_cfg.hold_setpoint, valve_cfg.hit_time_ms);

  for (uint8_t ch = 0; ch < BoardTestConfig::NUM_CHANNELS; ch++) {
    if (!require_ok(g_driver->ConfigureChannel(ch, valve_cfg), "ConfigureChannel")) return false;
  }
  ESP_LOGI(TAG, "All 8 channels configured for valve profile.");
  return true;
}

static void cleanup_resources() {
  if (g_driver) {
    g_driver->DisableAllChannels();
    g_driver->DisableDevice();
  }
  g_driver.reset();
  g_spi_interface.reset();
  ESP_LOGI(TAG, "Resources cleaned up.");
}

//=============================================================================
// MAIN
//=============================================================================

extern "C" void app_main(void) {
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "╔══════════════════════════════════════════════════════════════════════╗");
  ESP_LOGI(TAG, "║         MAX22200 SOLENOID / VALVE TEST  (all 8 channels)               ║");
  ESP_LOGI(TAG, "║  C21-style: 100ms hit, 50%% hold  •  Sequential + Parallel patterns   ║");
  ESP_LOGI(TAG, "╚══════════════════════════════════════════════════════════════════════╝");
  vTaskDelay(pdMS_TO_TICKS(800));

  if (!init_driver_and_valve_config()) {
    ESP_LOGE(TAG, "Init failed");
    return;
  }

  log_diagnostics("after init and channel config");

  const uint32_t loop_count = SolenoidValvePatternConfig::LOOP_COUNT;
  for (uint32_t iter = 1u; loop_count == 0u || iter <= loop_count; iter++) {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "═══ Loop %" PRIu32 "%s ═══", iter, loop_count == 0u ? " (infinite)" : "");

    run_sequential_pattern();
    vTaskDelay(pdMS_TO_TICKS(SolenoidValvePatternConfig::PATTERN_PAUSE_MS));
    log_diagnostics("after sequential pattern");

    run_parallel_pattern();
    vTaskDelay(pdMS_TO_TICKS(SolenoidValvePatternConfig::PATTERN_PAUSE_MS));
    log_diagnostics("after parallel pattern");
  }

  cleanup_resources();
  ESP_LOGI(TAG, "Solenoid valve test finished (%" PRIu32 " loop(s)).", loop_count);
}

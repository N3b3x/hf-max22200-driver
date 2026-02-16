/**
 * @file max22200_comprehensive_test.cpp
 * @brief Comprehensive test suite for MAX22200 driver on ESP32
 *
 * Tests the two-phase SPI protocol (per MAX22200 datasheet), register
 * read/write, fault handling, and unit-based convenience APIs. All
 * errors are caught and reported via DriverStatusToStr(); debug
 * logging uses tagged prefixes ([init], [cfg], [unit_ma], etc.).
 *
 * @details
 * Test sections (controlled by ENABLE_* defines):
 * - **Basic**: initialization, raw register dump, channel config write/readback,
 *   fault status, control pins (FAULT), trigger pins (TRIGA/TRIGB).
 * - **Unit APIs**: BoardConfig (SetBoardConfig/GetBoardConfig, BoardConfig(rref, hfs)),
 *   current in mA and percent (SetHitCurrentMa, SetHoldCurrentPercent, etc.),
 *   duty in percent (SetHitDutyPercent, GetDutyLimits), HIT time in ms
 *   (SetHitTimeMs, GetHitTimeMs), ConfigureChannelCdr, ConfigureChannelVdr.
 * - **Error handling**: invalid channel (8), GetHitCurrentMa with IFS=0;
 *   expects DriverStatus::INVALID_PARAMETER where appropriate.
 *
 * @par Config
 * Board and channel-config test values (RREF, HFS, IFS, hit/hold setpoints) are defined
 * in esp32_max22200_test_config.hpp: BoardTestConfig.
 *
 * @par Build
 * From examples/esp32: `./scripts/build_app.sh max22200_comprehensive_test Debug`
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
#include "TestFramework.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

using namespace max22200;

#ifdef __cplusplus
extern "C" {
#endif
#include "esp_log.h"
#ifdef __cplusplus
}
#endif

static const char *TAG = "MAX22200_Test";   /**< ESP_LOG tag for this test suite */
static TestResults g_test_results;          /**< Aggregated pass/fail and timing */

//=============================================================================
// TEST CONFIGURATION
//=============================================================================

/** Enable basic tests: init, raw registers, channel config, fault, pins */
static constexpr bool ENABLE_BASIC_TESTS = true;
/** Enable unit-based API tests: BoardConfig, mA/%, duty %, ms, ConfigureChannelCdr/Vdr */
static constexpr bool ENABLE_UNIT_API_TESTS = true;
/** Enable error-handling tests: invalid channel, zero IFS */
static constexpr bool ENABLE_ERROR_HANDLING_TESTS = true;

// Board and test params are in esp32_max22200_test_config.hpp (BoardTestConfig)

//=============================================================================
// SHARED TEST RESOURCES
//=============================================================================

/** SPI bus implementation (ESP32 pins, CMD/ENABLE/FAULT control) */
static std::unique_ptr<Esp32Max22200SpiBus> g_spi_interface;
/** MAX22200 driver instance bound to g_spi_interface */
static std::unique_ptr<max22200::MAX22200<Esp32Max22200SpiBus>> g_driver;

//=============================================================================
// HELPERS: Error display and debug
//=============================================================================

/**
 * @brief Assert driver status is OK; log and return false on failure
 * @param status Driver status from any driver API call
 * @param op     Short description of the operation (e.g. "Initialize()")
 * @return true  if status == DriverStatus::OK
 * @return false otherwise; logs ESP_LOGE with op and DriverStatusToStr(status)
 */
static bool require_ok(DriverStatus status, const char *op) {
  if (status != DriverStatus::OK) {
    ESP_LOGE(TAG, "%s failed: %s", op, DriverStatusToStr(status));
    return false;
  }
  ESP_LOGD(TAG, "%s: OK", op);
  return true;
}

/**
 * @brief Print all fault diagnostics from driver (STATUS, FAULT, last fault byte, nFAULT)
 *        with human-readable fault names and possible causes (solenoid disconnected/short/etc).
 *        Call whenever nFAULT is active or after fault_status to aid debugging.
 */
static void print_all_fault_diagnostics() noexcept {
  if (!g_driver || !g_driver->IsInitialized()) return;

  StatusConfig status;
  if (g_driver->ReadStatus(status) != DriverStatus::OK) return;
  FaultStatus faults;
  if (g_driver->ReadFaultRegister(faults) != DriverStatus::OK) return;
  const uint8_t last_fault_byte = g_driver->GetLastFaultByte();
  bool nfault_active = false;
  g_driver->GetFaultPinState(nfault_active);

  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "======== FULL FAULT DIAGNOSTICS ========");
  ESP_LOGI(TAG, "  STATUS: ACTIVE=%d channels_on=0x%02X FREQM=%d",
           status.active ? 1 : 0, status.channels_on_mask, status.master_clock_80khz ? 1 : 0);
  ESP_LOGI(TAG, "  Global fault flags: OVT=%d OCP=%d OLF=%d HHF=%d DPM=%d COMER=%d UVM=%d",
           status.overtemperature ? 1 : 0, status.overcurrent ? 1 : 0, status.open_load_fault ? 1 : 0,
           status.hit_not_reached ? 1 : 0, status.plunger_movement_fault ? 1 : 0,
           status.communication_error ? 1 : 0, status.undervoltage ? 1 : 0);

  const bool any_status_fault = status.overtemperature || status.overcurrent || status.open_load_fault
      || status.hit_not_reached || status.plunger_movement_fault || status.communication_error || status.undervoltage;
  if (any_status_fault || faults.hasFault()) {
    ESP_LOGI(TAG, "  --- Faults set (possible causes) ---");
    if (status.undervoltage)
      ESP_LOGI(TAG, "  >>> UVM: Undervoltage — check VM supply and wiring");
    if (status.communication_error)
      ESP_LOGI(TAG, "  >>> COMER: Communication error — check SPI (CS, CMD, MISO)");
    if (status.overtemperature)
      ESP_LOGI(TAG, "  >>> OVT: Overtemperature — check die/cooling");
    if (status.overcurrent)
      ESP_LOGI(TAG, "  >>> OCP: Overcurrent (global) — short or overload on a channel");
    if (status.open_load_fault)
      ESP_LOGI(TAG, "  >>> OLF: Open load (global) — solenoid disconnected or broken wire");
    if (status.hit_not_reached)
      ESP_LOGI(TAG, "  >>> HHF: Hit current not reached (global) — check supply/load");
    if (status.plunger_movement_fault)
      ESP_LOGI(TAG, "  >>> DPM: Plunger movement fault (global)");
    if (faults.overcurrent_channel_mask)
      ESP_LOGI(TAG, "  >>> OCP per-ch: 0x%02X — short or overcurrent on channel(s)", faults.overcurrent_channel_mask);
    if (faults.hit_not_reached_channel_mask)
      ESP_LOGI(TAG, "  >>> HHF per-ch: 0x%02X — hit current not reached on channel(s)", faults.hit_not_reached_channel_mask);
    if (faults.open_load_fault_channel_mask)
      ESP_LOGI(TAG, "  >>> OLF per-ch: 0x%02X — open load / disconnected solenoid on channel(s)", faults.open_load_fault_channel_mask);
    if (faults.plunger_movement_fault_channel_mask)
      ESP_LOGI(TAG, "  >>> DPM per-ch: 0x%02X — plunger movement on channel(s)", faults.plunger_movement_fault_channel_mask);
    for (uint8_t ch = 0; ch < 8; ch++) {
      const bool ocp = (faults.overcurrent_channel_mask & (1u << ch)) != 0;
      const bool hhf = (faults.hit_not_reached_channel_mask & (1u << ch)) != 0;
      const bool olf = (faults.open_load_fault_channel_mask & (1u << ch)) != 0;
      const bool dpm = (faults.plunger_movement_fault_channel_mask & (1u << ch)) != 0;
      if (ocp || hhf || olf || dpm) {
        ESP_LOGI(TAG, "      CH%u: %s%s%s%s", ch,
                 ocp ? "OCP " : "", hhf ? "HHF " : "", olf ? "OLF " : "", dpm ? "DPM" : "");
      }
    }
  }

  ESP_LOGI(TAG, "  FAULT register: OCP=0x%02X HHF=0x%02X OLF=0x%02X DPM=0x%02X",
           faults.overcurrent_channel_mask, faults.hit_not_reached_channel_mask,
           faults.open_load_fault_channel_mask, faults.plunger_movement_fault_channel_mask);
  ESP_LOGI(TAG, "  Last fault byte (CMD reg): 0x%02X (ACTIVE=%d OVT=%d OCP=%d OLF=%d HHF=%d DPM=%d COMER=%d UVM=%d)",
           last_fault_byte,
           (last_fault_byte >> 0) & 1, (last_fault_byte >> 1) & 1, (last_fault_byte >> 2) & 1,
           (last_fault_byte >> 3) & 1, (last_fault_byte >> 4) & 1, (last_fault_byte >> 5) & 1,
           (last_fault_byte >> 6) & 1, (last_fault_byte >> 7) & 1);
  ESP_LOGI(TAG, "  nFAULT pin: %s", nfault_active ? "ACTIVE (fault — pin low)" : "INACTIVE (no fault)");

  if (nfault_active || any_status_fault || faults.hasFault()) {
    ESP_LOGI(TAG, "  --- Possible causes (solenoid/wiring) ---");
    ESP_LOGI(TAG, "  UVM=undervoltage; OCP=short/overcurrent; OLF=open load/disconnected; HHF=hit not reached; DPM=plunger; COMER=SPI; OVT=thermal");
  }
  ESP_LOGI(TAG, "========================================");
  ESP_LOGI(TAG, "");
}

//=============================================================================
// TEST HELPER FUNCTIONS
//=============================================================================

/**
 * @brief Create SPI bus and driver instances; log pin configuration
 * @return true if both g_spi_interface and g_driver were created successfully
 * @return false on allocation failure
 */
static bool init_test_resources() noexcept {
  g_spi_interface = CreateEsp32Max22200SpiBus();
  if (!g_spi_interface) {
    ESP_LOGE(TAG, "Failed to create SPI interface");
    return false;
  }

  // Board config (RREF → IFS) is known at construction; from test config
  using namespace MAX22200_TestConfig;
  BoardConfig board(BoardTestConfig::RREF_KOHM, BoardTestConfig::HFS);
  board.max_current_ma = BoardTestConfig::MAX_CURRENT_MA;
  board.max_duty_percent = BoardTestConfig::MAX_DUTY_PERCENT;

  g_driver = std::make_unique<max22200::MAX22200<Esp32Max22200SpiBus>>(
      *g_spi_interface, board);
  if (!g_driver) {
    ESP_LOGE(TAG, "Failed to create driver");
    return false;
  }

  ESP_LOGI(TAG, "Board: RREF=%.1f kOhm HFS=%d IFS=%" PRIu32 " mA",
           BoardTestConfig::RREF_KOHM, static_cast<int>(BoardTestConfig::HFS), board.full_scale_current_ma);
  ESP_LOGI(TAG, "Pin config: MISO=%d MOSI=%d SCLK=%d CS=%d",
           MAX22200_TestConfig::SPIPins::MISO,
           MAX22200_TestConfig::SPIPins::MOSI,
           MAX22200_TestConfig::SPIPins::SCLK,
           MAX22200_TestConfig::SPIPins::CS);
  ESP_LOGI(TAG, "Control pins: EN=%d FAULT=%d CMD=%d TRIGA=%d TRIGB=%d",
           MAX22200_TestConfig::ControlPins::ENABLE,
           MAX22200_TestConfig::ControlPins::FAULT,
           MAX22200_TestConfig::ControlPins::CMD,
           MAX22200_TestConfig::ControlPins::TRIGA,
           MAX22200_TestConfig::ControlPins::TRIGB);
  ESP_LOGI(TAG, "Test resources initialized");
  return true;
}

/**
 * @brief Release driver and SPI interface; reset shared pointers
 */
static void cleanup_test_resources() noexcept {
  g_driver.reset();
  g_spi_interface.reset();
  ESP_LOGI(TAG, "Test resources cleaned up");
}

//=============================================================================
// BASIC TESTS
//=============================================================================

/**
 * @brief Test driver initialization per datasheet flowchart
 *
 * Calls Initialize(), checks IsInitialized(), reads STATUS to verify
 * ACTIVE=1 and logs fault byte from Command Register.
 * @return true if init and STATUS read succeed and ACTIVE is set
 */
static bool test_basic_initialization() noexcept {
  if (!g_driver) {
    ESP_LOGE(TAG, "Driver not created");
    return false;
  }

  ESP_LOGI(TAG, "[init] Calling Initialize()...");
  DriverStatus status = g_driver->Initialize();
  if (!require_ok(status, "Initialize()")) {
    ESP_LOGE(TAG, "Initialization failed: %s", DriverStatusToStr(status));
    return false;
  }

  if (!g_driver->IsInitialized()) {
    ESP_LOGE(TAG, "Driver reports not initialized after Initialize()");
    return false;
  }
  ESP_LOGI(TAG, "[init] Driver is initialized");

  StatusConfig stat;
  status = g_driver->ReadStatus(stat);
  if (!require_ok(status, "ReadStatus()")) {
    return false;
  }

  ESP_LOGI(TAG, "[init] STATUS: ACTIVE=%d UVM=%d OCP=%d OLF=%d COMER=%d channels_on_mask=0x%02X",
           stat.active, stat.undervoltage, stat.overcurrent, stat.open_load_fault,
           stat.communication_error, stat.channels_on_mask);

  uint8_t fault_byte = g_driver->GetLastFaultByte();
  ESP_LOGI(TAG, "[init] Last fault byte from CMD reg: 0x%02X", fault_byte);

  if (!stat.active) {
    ESP_LOGE(TAG, "ACTIVE bit not set after initialization");
    return false;
  }

  ESP_LOGI(TAG, "[init] Basic initialization test passed");
  return true;
}

/**
 * @brief Test channel configuration write and readback
 *
 * Writes a known ChannelConfig to channel 0 (CDR, low-side, fixed HIT/HOLD/hit_time),
 * reads it back and compares drive_mode, side_mode, hit_current, hold_current, hit_time.
 * Also logs raw CFG_CH0 register value for debug.
 * @return true if readback matches written config
 */
static bool test_channel_configuration() noexcept {
  if (!g_driver || !g_driver->IsInitialized()) {
    ESP_LOGE(TAG, "Driver not initialized");
    return false;
  }

  // Same board as construction; hit/hold chosen for predictable round-trip (→ raw 80/40 at IFS=1000)
  using namespace MAX22200_TestConfig;
  const uint32_t board_ifs_ma = g_driver->GetBoardConfig().full_scale_current_ma;
  constexpr float kChannelTestHitMa = 630.0f;
  constexpr float kChannelTestHoldMa = 315.0f;

  StatusConfig st;
  if (g_driver->ReadStatus(st) != DriverStatus::OK) {
    ESP_LOGE(TAG, "ReadStatus failed");
    return false;
  }

  ChannelConfig config;
  config.drive_mode = DriveMode::CDR;
  config.side_mode = SideMode::LOW_SIDE;
  config.hit_setpoint = kChannelTestHitMa;
  config.hold_setpoint = kChannelTestHoldMa;
  config.hit_time_ms = 10.0f;     // 10 ms (converted to register from fCHOP)
  config.half_full_scale = false;
  config.trigger_from_pin = false;
  config.chop_freq = ChopFreq::FMAIN_DIV4;
  config.slew_rate_control_enabled = false;
  config.open_load_detection_enabled = false;
  config.plunger_movement_detection_enabled = false;
  config.hit_current_check_enabled = false;

  // Driver uses board IFS and cached STATUS (FREQM) when writing; match for round-trip comparison
  uint32_t sent_val = config.toRegister(board_ifs_ma, st.master_clock_80khz);
  ESP_LOGI(TAG, "[cfg] Writing CFG_CH0: 0x%08" PRIX32, sent_val);

  DriverStatus status = g_driver->ConfigureChannel(0, config);
  if (!require_ok(status, "ConfigureChannel(0)")) {
    return false;
  }

  ChannelConfig read_config;
  status = g_driver->GetChannelConfig(0, read_config);
  if (!require_ok(status, "GetChannelConfig(0)")) {
    return false;
  }

  uint32_t read_val = read_config.toRegister(board_ifs_ma, st.master_clock_80khz);
  ESP_LOGI(TAG, "[cfg] Read back CFG_CH0: 0x%08" PRIX32, read_val);

  uint32_t raw_val = 0;
  g_driver->ReadRegister32(getChannelCfgBank(0), raw_val);
  ESP_LOGI(TAG, "[cfg] Raw CFG_CH0 register: 0x%08" PRIX32, raw_val);

  // Primary check: raw register round-trip (source of truth for SPI and byte order)
  if (sent_val == raw_val) {
    ESP_LOGI(TAG, "[cfg] Register round-trip OK (sent=read=0x%08" PRIX32 ")", raw_val);
    ESP_LOGI(TAG, "[cfg] Channel configuration test passed");
    return true;
  }

  // Raw mismatch: report decoded fields to help debug
  if (read_config.drive_mode != config.drive_mode) {
    ESP_LOGE(TAG, "  drive_mode mismatch: expected=%d read=%d",
             static_cast<int>(config.drive_mode),
             static_cast<int>(read_config.drive_mode));
  }
  if (read_config.side_mode != config.side_mode) {
    ESP_LOGE(TAG, "  side_mode mismatch: expected=%d read=%d",
             static_cast<int>(config.side_mode),
             static_cast<int>(read_config.side_mode));
  }
  const float tolerance = 1.0f;
  if (std::abs(read_config.hit_setpoint - config.hit_setpoint) > tolerance) {
    ESP_LOGE(TAG, "  hit_setpoint mismatch: expected=%.1f read=%.1f",
             config.hit_setpoint, read_config.hit_setpoint);
  }
  if (std::abs(read_config.hold_setpoint - config.hold_setpoint) > tolerance) {
    ESP_LOGE(TAG, "  hold_setpoint mismatch: expected=%.1f read=%.1f",
             config.hold_setpoint, read_config.hold_setpoint);
  }
  if (std::abs(read_config.hit_time_ms - config.hit_time_ms) > 1.0f) {
    ESP_LOGE(TAG, "  hit_time_ms mismatch: expected=%.2f read=%.2f",
             config.hit_time_ms, read_config.hit_time_ms);
  }

  ESP_LOGE(TAG, "Channel configuration mismatch (sent=0x%08" PRIX32 " raw=0x%08" PRIX32 ")",
           sent_val, raw_val);
  return false;
}

/**
 * @brief Test fault status and per-channel FAULT register
 *
 * Reads STATUS (global fault flags OVT, OCP, OLF, HHF, DPM, COMER, UVM) and
 * FAULT register (per-channel OCP, HHF, OLF, DPM). Logs any channel with a fault set.
 * @return true if both ReadStatus and ReadFaultRegister succeed
 */
static bool test_fault_status() noexcept {
  if (!g_driver || !g_driver->IsInitialized()) {
    ESP_LOGE(TAG, "Driver not initialized");
    return false;
  }

  StatusConfig status;
  DriverStatus result = g_driver->ReadStatus(status);
  if (!require_ok(result, "ReadStatus()")) {
    return false;
  }

  ESP_LOGI(TAG, "[fault] STATUS flags: OVT=%d OCP=%d OLF=%d HHF=%d DPM=%d COMER=%d UVM=%d",
           status.overtemperature, status.overcurrent, status.open_load_fault,
           status.hit_not_reached, status.plunger_movement_fault,
           status.communication_error, status.undervoltage);

  FaultStatus faults;
  result = g_driver->ReadFaultRegister(faults);
  if (!require_ok(result, "ReadFaultRegister()")) {
    return false;
  }

  ESP_LOGI(TAG, "[fault] FAULT register: OCP=0x%02X HHF=0x%02X OLF=0x%02X DPM=0x%02X",
           faults.overcurrent_channel_mask, faults.hit_not_reached_channel_mask,
           faults.open_load_fault_channel_mask, faults.plunger_movement_fault_channel_mask);

  if (faults.hasFault()) {
    for (uint8_t ch = 0; ch < 8; ch++) {
      if (faults.overcurrent_channel_mask & (1u << ch))
        ESP_LOGI(TAG, "  CH%u: OCP (overcurrent)", ch);
      if (faults.hit_not_reached_channel_mask & (1u << ch))
        ESP_LOGI(TAG, "  CH%u: HHF (hit not reached)", ch);
      if (faults.open_load_fault_channel_mask & (1u << ch))
        ESP_LOGI(TAG, "  CH%u: OLF (open load)", ch);
      if (faults.plunger_movement_fault_channel_mask & (1u << ch))
        ESP_LOGI(TAG, "  CH%u: DPM (plunger movement)", ch);
    }
  }

  print_all_fault_diagnostics();
  ESP_LOGI(TAG, "[fault] Fault status test passed");
  return true;
}

/**
 * @brief Test FAULT pin state reading
 *
 * Calls GetFaultPinState() and logs whether the nFAULT pin is active (fault) or inactive.
 * @return true if GetFaultPinState returns OK
 */
static bool test_control_pins() noexcept {
  if (!g_driver || !g_driver->IsInitialized()) {
    ESP_LOGE(TAG, "Driver not initialized");
    return false;
  }

  bool fault_active = false;
  DriverStatus status = g_driver->GetFaultPinState(fault_active);
  if (!require_ok(status, "GetFaultPinState()")) {
    return false;
  }

  ESP_LOGI(TAG, "[pins] FAULT pin: %s", fault_active ? "ACTIVE (fault)" : "INACTIVE (no fault)");
  if (fault_active) {
    print_all_fault_diagnostics();
  }
  ESP_LOGI(TAG, "[pins] Control pins test passed");
  return true;
}

/**
 * @brief Test TRIGA/TRIGB pin toggling
 *
 * If the SPI interface has TRIGA/TRIGB, cycles each high → low → high and logs.
 * @return true (no failure path; skipped if interface has no trigger pins)
 */
static bool test_trigger_pins() noexcept {
  if (!g_spi_interface) {
    ESP_LOGE(TAG, "SPI interface not available");
    return false;
  }

  if (g_spi_interface->HasTrigA()) {
    g_spi_interface->SetTrigA(true);
    g_spi_interface->SetTrigA(false);
    g_spi_interface->SetTrigA(true);
    ESP_LOGI(TAG, "[trig] TRIGA cycled (high -> low -> high)");
  }
  if (g_spi_interface->HasTrigB()) {
    g_spi_interface->SetTrigB(true);
    g_spi_interface->SetTrigB(false);
    g_spi_interface->SetTrigB(true);
    ESP_LOGI(TAG, "[trig] TRIGB cycled (high -> low -> high)");
  }
  ESP_LOGI(TAG, "[trig] Trigger pins test passed");
  return true;
}

/**
 * @brief Dump all 32-bit register banks for debug
 *
 * Reads banks 0x00 (STATUS) through 0x0A (CFG_DPM) and logs name and hex value;
 * on read failure logs DriverStatusToStr(status).
 * @return true (always passes; used for visibility)
 */
static bool test_raw_register_read() noexcept {
  if (!g_driver || !g_driver->IsInitialized()) {
    ESP_LOGE(TAG, "Driver not initialized");
    return false;
  }

  const char *names[] = {"STATUS", "CFG_CH0", "CFG_CH1", "CFG_CH2",
                         "CFG_CH3", "CFG_CH4", "CFG_CH5", "CFG_CH6",
                         "CFG_CH7", "FAULT", "CFG_DPM"};

  for (uint8_t bank = 0; bank <= 0x0A; bank++) {
    uint32_t val = 0;
    DriverStatus status = g_driver->ReadRegister32(bank, val);
    if (status == DriverStatus::OK) {
      ESP_LOGI(TAG, "  [0x%02X] %-8s = 0x%08" PRIX32, bank, names[bank], val);
    } else {
      ESP_LOGE(TAG, "  [0x%02X] %-8s = READ FAILED: %s", bank, names[bank],
               DriverStatusToStr(status));
    }
  }
  ESP_LOGI(TAG, "[raw] Raw register read test passed");
  return true;
}

//=============================================================================
// UNIT-BASED API TESTS (BoardConfig, mA, %, ms, ConfigureChannelCdr/Vdr)
//=============================================================================

/**
 * @brief Test BoardConfig set/get and constructor from RREF
 *
 * Builds config with BoardConfig(BoardTestConfig::RREF_KOHM, BoardTestConfig::HFS), sets
 * max_current_ma and max_duty_percent, writes via SetBoardConfig, reads with
 * GetBoardConfig and verifies all fields match.
 * @return true if readback matches
 */
static bool test_board_config() noexcept {
  if (!g_driver || !g_driver->IsInitialized()) {
    ESP_LOGE(TAG, "Driver not initialized");
    return false;
  }

  using namespace MAX22200_TestConfig;
  ESP_LOGI(TAG, "[board] Setting BoardConfig from RREF=%.1f kOhm, HFS=%d",
           BoardTestConfig::RREF_KOHM, static_cast<int>(BoardTestConfig::HFS));
  BoardConfig board(BoardTestConfig::RREF_KOHM, BoardTestConfig::HFS);
  board.max_current_ma = BoardTestConfig::MAX_CURRENT_MA;
  board.max_duty_percent = BoardTestConfig::MAX_DUTY_PERCENT;

  g_driver->SetBoardConfig(board);
  BoardConfig read_back = g_driver->GetBoardConfig();

  ESP_LOGI(TAG, "[board] full_scale_current_ma=%" PRIu32 " max_current_ma=%" PRIu32 " max_duty_percent=%u",
           read_back.full_scale_current_ma, read_back.max_current_ma, read_back.max_duty_percent);

  if (read_back.full_scale_current_ma != board.full_scale_current_ma || read_back.max_current_ma != board.max_current_ma ||
      read_back.max_duty_percent != board.max_duty_percent) {
    ESP_LOGE(TAG, "BoardConfig readback mismatch");
    return false;
  }
  ESP_LOGI(TAG, "[board] Board config test passed");
  return true;
}

/**
 * @brief Test current unit APIs in mA and percent (CDR)
 *
 * Uses board config from construction; SetHitCurrentMa(ch, 300), SetHoldCurrentMa(ch, 200),
 * GetHitCurrentMa, SetHoldCurrentPercent(ch, 40), GetHoldCurrentPercent. Logs set vs read values.
 * @return true if all driver calls succeed
 */
static bool test_unit_apis_current_ma_percent() noexcept {
  if (!g_driver || !g_driver->IsInitialized()) {
    ESP_LOGE(TAG, "Driver not initialized");
    return false;
  }

  // IFS already set at driver construction (BoardTestConfig)
  BoardConfig board = g_driver->GetBoardConfig();
  ESP_LOGI(TAG, "[unit_ma] BoardConfig (from construction): IFS=%" PRIu32 " mA", board.full_scale_current_ma);

  const uint8_t ch = 0;
  const uint32_t set_ma = 300;
  const float set_pct = 40.0f;

  // Set HIT/HOLD in mA
  DriverStatus st = g_driver->SetHitCurrentMa(ch, set_ma);
  if (!require_ok(st, "SetHitCurrentMa")) {
    return false;
  }
  st = g_driver->SetHoldCurrentMa(ch, 200);
  if (!require_ok(st, "SetHoldCurrentMa")) {
    return false;
  }

  uint32_t read_ma = 0;
  st = g_driver->GetHitCurrentMa(ch, read_ma);
  if (!require_ok(st, "GetHitCurrentMa")) {
    return false;
  }
  ESP_LOGI(TAG, "[unit_ma] CH%u HIT: set=%" PRIu32 " mA read=%" PRIu32 " mA", ch, set_ma, read_ma);

  float read_pct = 0.0f;
  st = g_driver->SetHoldCurrentPercent(ch, set_pct);
  if (!require_ok(st, "SetHoldCurrentPercent")) {
    return false;
  }
  st = g_driver->GetHoldCurrentPercent(ch, read_pct);
  if (!require_ok(st, "GetHoldCurrentPercent")) {
    return false;
  }
  ESP_LOGI(TAG, "[unit_ma] CH%u HOLD: set=%.1f%% read=%.1f%%", ch, set_pct, read_pct);

  ESP_LOGI(TAG, "[unit_ma] Current (mA/percent) unit API test passed");
  return true;
}

/**
 * @brief Test duty cycle unit APIs in percent (VDR)
 *
 * Reads STATUS for FREQM, calls GetDutyLimits, then SetHitDutyPercent/SetHoldDutyPercent
 * on channel 1 and readback via GetHitDutyPercent/GetHoldDutyPercent. Logs limits and set/read.
 * @return true if all driver calls succeed
 */
static bool test_unit_apis_duty_percent() noexcept {
  if (!g_driver || !g_driver->IsInitialized()) {
    ESP_LOGE(TAG, "Driver not initialized");
    return false;
  }

  StatusConfig stat;
  DriverStatus st = g_driver->ReadStatus(stat);
  if (!require_ok(st, "ReadStatus (for FREQM)")) {
    return false;
  }
  DutyLimits limits;
  st = g_driver->GetDutyLimits(stat.master_clock_80khz, ChopFreq::FMAIN_DIV4, false, limits);
  if (!require_ok(st, "GetDutyLimits")) {
    return false;
  }
  ESP_LOGI(TAG, "[unit_duty] Duty limits: min=%u%% max=%u%%",
           limits.min_percent, limits.max_percent);

  const uint8_t ch = 1;
  const float set_hit = 50.0f;
  const float set_hold = 30.0f;

  st = g_driver->SetHitDutyPercent(ch, set_hit);
  if (!require_ok(st, "SetHitDutyPercent")) {
    return false;
  }
  st = g_driver->SetHoldDutyPercent(ch, set_hold);
  if (!require_ok(st, "SetHoldDutyPercent")) {
    return false;
  }

  float read_hit = 0.0f, read_hold = 0.0f;
  st = g_driver->GetHitDutyPercent(ch, read_hit);
  if (!require_ok(st, "GetHitDutyPercent")) {
    return false;
  }
  st = g_driver->GetHoldDutyPercent(ch, read_hold);
  if (!require_ok(st, "GetHoldDutyPercent")) {
    return false;
  }
  ESP_LOGI(TAG, "[unit_duty] CH%u HIT duty: set=%.1f%% read=%.1f%%", ch, set_hit, read_hit);
  ESP_LOGI(TAG, "[unit_duty] CH%u HOLD duty: set=%.1f%% read=%.1f%%", ch, set_hold, read_hold);

  ESP_LOGI(TAG, "[unit_duty] Duty percent unit API test passed");
  return true;
}

/**
 * @brief Test HIT time unit API in milliseconds
 *
 * Sets HIT time to 10 ms on channel 0 via SetHitTimeMs, then reads back with GetHitTimeMs
 * and logs set vs read.
 * @return true if both set and get succeed
 */
static bool test_unit_apis_hit_time_ms() noexcept {
  if (!g_driver || !g_driver->IsInitialized()) {
    ESP_LOGE(TAG, "Driver not initialized");
    return false;
  }

  const uint8_t ch = 0;
  const float set_ms = 10.0f;

  DriverStatus st = g_driver->SetHitTimeMs(ch, set_ms);
  if (!require_ok(st, "SetHitTimeMs")) {
    return false;
  }

  float read_ms = 0.0f;
  st = g_driver->GetHitTimeMs(ch, read_ms);
  if (!require_ok(st, "GetHitTimeMs")) {
    return false;
  }
  ESP_LOGI(TAG, "[unit_ms] CH%u HIT time: set=%.2f ms read=%.2f ms", ch, set_ms, read_ms);

  ESP_LOGI(TAG, "[unit_ms] HIT time (ms) unit API test passed");
  return true;
}

/**
 * @brief Test static GetDutyLimits for two (FREQM, SRC) combinations
 *
 * Calls GetDutyLimits(FREQM=0, FMAIN_DIV4, SRC=0) and (FREQM=1, FMAIN_DIV4, SRC=1),
 * logs min_percent and max_percent for each.
 * @return true if both calls return OK
 */
static bool test_get_duty_limits() noexcept {
  DutyLimits limits;
  DriverStatus st = MAX22200<Esp32Max22200SpiBus>::GetDutyLimits(
      false, ChopFreq::FMAIN_DIV4, false, limits);
  if (!require_ok(st, "GetDutyLimits(FREQM=0, DIV4, SRC=0)")) {
    return false;
  }
  ESP_LOGI(TAG, "[duty_limits] FREQM=0 FMAIN_DIV4 SRC=0 -> min=%u%% max=%u%%",
           limits.min_percent, limits.max_percent);

  st = MAX22200<Esp32Max22200SpiBus>::GetDutyLimits(true, ChopFreq::FMAIN_DIV4, true, limits);
  if (!require_ok(st, "GetDutyLimits(FREQM=1, DIV4, SRC=1)")) {
    return false;
  }
  ESP_LOGI(TAG, "[duty_limits] FREQM=1 FMAIN_DIV4 SRC=1 -> min=%u%% max=%u%%",
           limits.min_percent, limits.max_percent);

  ESP_LOGI(TAG, "[duty_limits] GetDutyLimits test passed");
  return true;
}

/**
 * @brief Test ConfigureChannelCdr and readback in mA/ms
 *
 * Uses board config from construction; ConfigureChannelCdr(ch=2, hit_ma=350, hold_ma=180, hit_time_ms=15)
 * with LOW_SIDE, FMAIN_DIV4. Reads back with GetHitCurrentMa, GetHoldCurrentMa, GetHitTimeMs
 * and logs values.
 * @return true if configure and all readbacks succeed
 */
static bool test_configure_channel_cdr() noexcept {
  if (!g_driver || !g_driver->IsInitialized()) {
    ESP_LOGE(TAG, "Driver not initialized");
    return false;
  }

  // Board config (IFS) already set at driver construction
  const uint8_t ch = 2;
  const uint32_t hit_ma = 350;
  const uint32_t hold_ma = 180;
  const float hit_time_ms = 15.0f;

  ESP_LOGI(TAG, "[cfg_cdr] ConfigureChannelCdr ch=%u hit_ma=%" PRIu32 " hold_ma=%" PRIu32
           " hit_time_ms=%.1f", ch, hit_ma, hold_ma, hit_time_ms);

  DriverStatus st = g_driver->ConfigureChannelCdr(
      ch, hit_ma, hold_ma, hit_time_ms,
      SideMode::LOW_SIDE, ChopFreq::FMAIN_DIV4, false, false, false, false);
  if (!require_ok(st, "ConfigureChannelCdr")) {
    return false;
  }

  uint32_t read_hit_ma = 0, read_hold_ma = 0;
  float read_ms = 0.0f;
  st = g_driver->GetHitCurrentMa(ch, read_hit_ma);
  if (!require_ok(st, "GetHitCurrentMa after ConfigureChannelCdr")) {
    return false;
  }
  st = g_driver->GetHoldCurrentMa(ch, read_hold_ma);
  if (!require_ok(st, "GetHoldCurrentMa after ConfigureChannelCdr")) {
    return false;
  }
  st = g_driver->GetHitTimeMs(ch, read_ms);
  if (!require_ok(st, "GetHitTimeMs after ConfigureChannelCdr")) {
    return false;
  }

  ESP_LOGI(TAG, "[cfg_cdr] Readback: hit_ma=%" PRIu32 " hold_ma=%" PRIu32 " hit_time_ms=%.2f",
           read_hit_ma, read_hold_ma, read_ms);

  ESP_LOGI(TAG, "[cfg_cdr] ConfigureChannelCdr test passed");
  return true;
}

/**
 * @brief Test ConfigureChannelVdr and readback in percent/ms
 *
 * Uses board config from construction; ConfigureChannelVdr(ch=3, hit_duty=60%,
 * hold_duty=35%, hit_time_ms=20). Reads back with GetHitDutyPercent, GetHoldDutyPercent,
 * GetHitTimeMs and logs.
 * @return true if configure and all readbacks succeed
 */
static bool test_configure_channel_vdr() noexcept {
  if (!g_driver || !g_driver->IsInitialized()) {
    ESP_LOGE(TAG, "Driver not initialized");
    return false;
  }

  // Board config (IFS, max_duty) already set at driver construction
  const uint8_t ch = 3;
  const float hit_duty = 60.0f;
  const float hold_duty = 35.0f;
  const float hit_time_ms = 20.0f;

  ESP_LOGI(TAG, "[cfg_vdr] ConfigureChannelVdr ch=%u hit_duty=%.1f%% hold_duty=%.1f%% hit_time_ms=%.1f",
           ch, hit_duty, hold_duty, hit_time_ms);

  DriverStatus st = g_driver->ConfigureChannelVdr(
      ch, hit_duty, hold_duty, hit_time_ms,
      SideMode::LOW_SIDE, ChopFreq::FMAIN_DIV4, false, false, false, false);
  if (!require_ok(st, "ConfigureChannelVdr")) {
    return false;
  }

  float read_hit = 0.0f, read_hold = 0.0f, read_ms = 0.0f;
  st = g_driver->GetHitDutyPercent(ch, read_hit);
  if (!require_ok(st, "GetHitDutyPercent after ConfigureChannelVdr")) {
    return false;
  }
  st = g_driver->GetHoldDutyPercent(ch, read_hold);
  if (!require_ok(st, "GetHoldDutyPercent after ConfigureChannelVdr")) {
    return false;
  }
  st = g_driver->GetHitTimeMs(ch, read_ms);
  if (!require_ok(st, "GetHitTimeMs after ConfigureChannelVdr")) {
    return false;
  }

  ESP_LOGI(TAG, "[cfg_vdr] Readback: hit_duty=%.1f%% hold_duty=%.1f%% hit_time_ms=%.2f",
           read_hit, read_hold, read_ms);

  ESP_LOGI(TAG, "[cfg_vdr] ConfigureChannelVdr test passed");
  return true;
}

//=============================================================================
// ERROR HANDLING TESTS (expect INVALID_PARAMETER or similar)
//=============================================================================

/**
 * @brief Test that invalid inputs return INVALID_PARAMETER
 *
 * - ConfigureChannel(8, cfg) → INVALID_PARAMETER
 * - SetHitCurrentMa(8, 100) → INVALID_PARAMETER
 * - SetHitTimeMs(8, 10.0f) → INVALID_PARAMETER
 * - GetHitCurrentMa(0, ma) with IFS=0 (temporarily set) → INVALID_PARAMETER
 * Logs expected vs actual status on failure.
 * @return true if all four checks get the expected status
 */
static bool test_error_handling() noexcept {
  if (!g_driver || !g_driver->IsInitialized()) {
    ESP_LOGE(TAG, "Driver not initialized");
    return false;
  }

  bool any_failed = false;

  // Invalid channel (8 is out of range 0-7)
  {
    ChannelConfig cfg;
    DriverStatus st = g_driver->ConfigureChannel(8, cfg);
    if (st != DriverStatus::INVALID_PARAMETER) {
      ESP_LOGE(TAG, "[err] ConfigureChannel(8) expected INVALID_PARAMETER, got %s",
               DriverStatusToStr(st));
      any_failed = true;
    } else {
      ESP_LOGI(TAG, "[err] ConfigureChannel(8) correctly returned INVALID_PARAMETER");
    }
  }

  // SetHitCurrentMa with invalid channel
  {
    DriverStatus st = g_driver->SetHitCurrentMa(8, 100);
    if (st != DriverStatus::INVALID_PARAMETER) {
      ESP_LOGE(TAG, "[err] SetHitCurrentMa(8) expected INVALID_PARAMETER, got %s",
               DriverStatusToStr(st));
      any_failed = true;
    } else {
      ESP_LOGI(TAG, "[err] SetHitCurrentMa(8) correctly returned INVALID_PARAMETER");
    }
  }

  // SetHitTimeMs with invalid channel
  {
    DriverStatus st = g_driver->SetHitTimeMs(8, 10.0f);
    if (st != DriverStatus::INVALID_PARAMETER) {
      ESP_LOGE(TAG, "[err] SetHitTimeMs(8) expected INVALID_PARAMETER, got %s",
               DriverStatusToStr(st));
      any_failed = true;
    } else {
      ESP_LOGI(TAG, "[err] SetHitTimeMs(8) correctly returned INVALID_PARAMETER");
    }
  }

  // GetHitCurrentMa with IFS=0 should fail (set board config to zero IFS for one call)
  {
    BoardConfig saved = g_driver->GetBoardConfig();
    BoardConfig zero_ifs;
    zero_ifs.full_scale_current_ma = 0;
    g_driver->SetBoardConfig(zero_ifs);
    uint32_t ma = 0;
    DriverStatus st = g_driver->GetHitCurrentMa(0, ma);
    g_driver->SetBoardConfig(saved);
    if (st != DriverStatus::INVALID_PARAMETER) {
      ESP_LOGE(TAG, "[err] GetHitCurrentMa with IFS=0 expected INVALID_PARAMETER, got %s",
               DriverStatusToStr(st));
      any_failed = true;
    } else {
      ESP_LOGI(TAG, "[err] GetHitCurrentMa with IFS=0 correctly returned INVALID_PARAMETER");
    }
  }

  if (any_failed) {
    ESP_LOGE(TAG, "[err] One or more error-handling checks failed");
    return false;
  }
  ESP_LOGI(TAG, "[err] Error handling test passed");
  return true;
}

//=============================================================================
// MAIN TEST RUNNER
//=============================================================================

/**
 * @brief ESP32 app entry point; runs MAX22200 test suite
 *
 * Initializes shared resources (SPI + driver), runs test tasks in sections:
 * Basic (if ENABLE_BASIC_TESTS), Unit API (if ENABLE_UNIT_API_TESTS),
 * Error handling (if ENABLE_ERROR_HANDLING_TESTS). Prints summary via
 * print_test_summary() and cleans up resources.
 */
extern "C" void app_main(void) {
  ESP_LOGI(TAG, "╔═══════════════════════════════════════════════════════════╗");
  ESP_LOGI(TAG, "║        ESP32 MAX22200 COMPREHENSIVE TEST SUITE           ║");
  ESP_LOGI(TAG, "║         Unit APIs: mA, %%, ms • Errors reported           ║");
  ESP_LOGI(TAG, "╚═══════════════════════════════════════════════════════════╝");

  vTaskDelay(pdMS_TO_TICKS(1000));

  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "╔══════════════════════════════════════════════════════════════════════════════╗");
  ESP_LOGI(TAG, "║                 MAX22200 TEST SECTION CONFIGURATION                            ║");
  ESP_LOGI(TAG, "╚══════════════════════════════════════════════════════════════════════════════╝");

  if (!init_test_resources()) {
    ESP_LOGE(TAG, "Failed to initialize test resources");
    return;
  }

  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "╔══════════════════════════════════════════════════════════════════════════════╗");
  ESP_LOGI(TAG, "║                              BASIC TESTS                                     ║");
  ESP_LOGI(TAG, "╠══════════════════════════════════════════════════════════════════════════════╣");

  if (ENABLE_BASIC_TESTS) {
    RUN_TEST_IN_TASK("basic_initialization", test_basic_initialization, 8192, 1);
    RUN_TEST_IN_TASK("raw_register_read", test_raw_register_read, 8192, 1);
    RUN_TEST_IN_TASK("channel_configuration", test_channel_configuration, 8192, 1);
    RUN_TEST_IN_TASK("fault_status", test_fault_status, 8192, 1);
    RUN_TEST_IN_TASK("control_pins", test_control_pins, 8192, 1);
    RUN_TEST_IN_TASK("trigger_pins", test_trigger_pins, 8192, 1);
  }

  vTaskDelay(pdMS_TO_TICKS(300));

  if (ENABLE_UNIT_API_TESTS) {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "╔══════════════════════════════════════════════════════════════════════════════╗");
    ESP_LOGI(TAG, "║                         UNIT-BASED API TESTS                                 ║");
    ESP_LOGI(TAG, "╠══════════════════════════════════════════════════════════════════════════════╣");
    RUN_TEST_IN_TASK("board_config", test_board_config, 8192, 1);
    RUN_TEST_IN_TASK("get_duty_limits", test_get_duty_limits, 8192, 1);
    RUN_TEST_IN_TASK("unit_apis_current_ma_percent", test_unit_apis_current_ma_percent, 8192, 1);
    RUN_TEST_IN_TASK("unit_apis_duty_percent", test_unit_apis_duty_percent, 8192, 1);
    RUN_TEST_IN_TASK("unit_apis_hit_time_ms", test_unit_apis_hit_time_ms, 8192, 1);
    RUN_TEST_IN_TASK("configure_channel_cdr", test_configure_channel_cdr, 8192, 1);
    RUN_TEST_IN_TASK("configure_channel_vdr", test_configure_channel_vdr, 8192, 1);
    vTaskDelay(pdMS_TO_TICKS(300));
  }

  if (ENABLE_ERROR_HANDLING_TESTS) {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "╔══════════════════════════════════════════════════════════════════════════════╗");
    ESP_LOGI(TAG, "║                         ERROR HANDLING TESTS                                 ║");
    ESP_LOGI(TAG, "╠══════════════════════════════════════════════════════════════════════════════╣");
    RUN_TEST_IN_TASK("error_handling", test_error_handling, 8192, 1);
    vTaskDelay(pdMS_TO_TICKS(300));
  }

  ESP_LOGI(TAG, "╚══════════════════════════════════════════════════════════════════════════════╝");
  cleanup_test_resources();
  ESP_LOGI(TAG, "");
  print_test_summary(g_test_results, "MAX22200", TAG);
}

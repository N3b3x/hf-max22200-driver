/**
 * @file max22200_comprehensive_test.cpp
 * @brief Comprehensive test suite for MAX22200 driver on ESP32-C6
 *
 * This file contains comprehensive testing for MAX22200 features.
 *
 * @author HardFOC Development Team
 * @date 2025
 * @copyright HardFOC
 */

#include <cinttypes>
#include <memory>

#include "esp32_max22200_bus.hpp"
#include "esp32_max22200_test_config.hpp"
#include "max22200.hpp"
#include "max22200_registers.hpp"
#include "TestFramework.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Use namespace for convenience
using namespace max22200;

#ifdef __cplusplus
extern "C" {
#endif
#include "esp_log.h"
#ifdef __cplusplus
}
#endif

static const char *TAG = "MAX22200_Test";
static TestResults g_test_results;

//=============================================================================
// TEST CONFIGURATION
//=============================================================================
static constexpr bool ENABLE_BASIC_TESTS = true;

//=============================================================================
// SHARED TEST RESOURCES
//=============================================================================
static std::unique_ptr<Esp32Max22200SpiBus> g_spi_interface;
static std::unique_ptr<max22200::MAX22200<Esp32Max22200SpiBus>> g_driver;

//=============================================================================
// TEST HELPER FUNCTIONS
//=============================================================================

/**
 * @brief Log pin configuration from test config (for verification and debugging)
 */
static void log_pin_config() noexcept {
  using namespace MAX22200_TestConfig;
  ESP_LOGI(TAG, "Pin config: MISO=%d MOSI=%d SCLK=%d CS=%d",
           static_cast<int>(SPIPins::MISO), static_cast<int>(SPIPins::MOSI),
           static_cast<int>(SPIPins::SCLK), static_cast<int>(SPIPins::CS));
  ESP_LOGI(TAG, "Control pins: EN=%d FAILTN=%d CMD=%d TRIGA=%d TRIGB=%d",
           static_cast<int>(ControlPins::ENABLE), static_cast<int>(ControlPins::FAULT),
           static_cast<int>(ControlPins::CMD), static_cast<int>(ControlPins::TRIGA),
           static_cast<int>(ControlPins::TRIGB));
}

/**
 * @brief Initialize test resources
 */
static bool init_test_resources() noexcept {
  log_pin_config();

  // Create SPI interface using centralized test config
  g_spi_interface = CreateEsp32Max22200SpiBus();

  // Create driver instance
  g_driver = std::make_unique<max22200::MAX22200<Esp32Max22200SpiBus>>(*g_spi_interface);

  ESP_LOGI(TAG, "Test resources initialized");
  return true;
}

/**
 * @brief Cleanup test resources
 *
 * Destruction order: driver first, then SPI bus. The driver destructor
 * deasserts ENABLE via the bus; the bus must still be valid at that time.
 */
static void cleanup_test_resources() noexcept {
  if (g_driver && g_driver->IsInitialized()) {
    g_driver->Deinitialize();
  }
  g_driver.reset();       // Driver destroyed first: deasserts ENABLE using bus
  g_spi_interface.reset(); // Bus destroyed after
  ESP_LOGI(TAG, "Test resources cleaned up");
}

//=============================================================================
// TEST CASES
//=============================================================================

/**
 * @brief Basic initialization test
 */
static bool test_basic_initialization() noexcept {
  if (!g_driver) {
    ESP_LOGE(TAG, "Driver not initialized");
    return false;
  }

  // Test initialization
  DriverStatus status = g_driver->Initialize();
  if (status != DriverStatus::OK) {
    ESP_LOGE(TAG, "Driver initialization failed");
    return false;
  }

  // Verify driver is initialized
  if (!g_driver->IsInitialized()) {
    ESP_LOGE(TAG, "Driver reports not initialized after successful init");
    return false;
  }

  // Test channel validation
  if (!max22200::MAX22200<Esp32Max22200SpiBus>::IsValidChannel(0)) {
    ESP_LOGE(TAG, "Channel 0 should be valid");
    return false;
  }

  if (max22200::MAX22200<Esp32Max22200SpiBus>::IsValidChannel(8)) {
    ESP_LOGE(TAG, "Channel 8 should be invalid");
    return false;
  }

  // Test version retrieval
  const char *version = max22200::MAX22200<Esp32Max22200SpiBus>::GetVersion();
  if (version == nullptr) {
    ESP_LOGE(TAG, "Version string is null");
    return false;
  }

  ESP_LOGI(TAG, "Basic initialization test passed");
  return true;
}

/**
 * @brief Test channel configuration
 */
static bool test_channel_configuration() noexcept {
  if (!g_driver || !g_driver->IsInitialized()) {
    ESP_LOGE(TAG, "Driver not initialized");
    return false;
  }

  // Configure channel 0 (use values that fit the current register format:
  // driver packs hit/hold as 8 bits each in 16-bit register, so 0-255)
  ChannelConfig config;
  config.enabled = true;
  config.drive_mode = DriveMode::CDR;
  config.bridge_mode = BridgeMode::HALF_BRIDGE;
  config.parallel_mode = false;
  config.polarity = OutputPolarity::NORMAL;
  config.hit_current = 200;
  config.hold_current = 150;
  config.hit_time = 1000;

  DriverStatus status = g_driver->ConfigureChannel(0, config);
  if (status != DriverStatus::OK) {
    ESP_LOGE(TAG, "Failed to configure channel 0");
    return false;
  }

  // Build expected channel config register value (what we sent)
  const uint16_t sent_config = (config.drive_mode == DriveMode::VDR ? ChannelConfigBits::DRIVE_MODE_MASK : 0u) |
                               (config.bridge_mode == BridgeMode::FULL_BRIDGE ? ChannelConfigBits::BRIDGE_MODE_MASK : 0u) |
                               (config.parallel_mode ? ChannelConfigBits::PARALLEL_MASK : 0u) |
                               (config.polarity == OutputPolarity::INVERTED ? ChannelConfigBits::POLARITY_MASK : 0u);

  // Read back configuration
  ChannelConfig read_config;
  status = g_driver->GetChannelConfig(0, read_config);
  if (status != DriverStatus::OK) {
    ESP_LOGE(TAG, "Failed to read channel 0 configuration");
    return false;
  }

  // Raw read of channel config register for debug (actual received bytes)
  uint16_t received_config = 0;
  if (g_driver->ReadRegister(getChannelConfigReg(0), received_config) != DriverStatus::OK) {
    ESP_LOGE(TAG, "Failed to read channel 0 config register raw");
  } else {
    ESP_LOGI(TAG, "Channel config register: sent=0x%04" PRIX16 " received=0x%04" PRIX16,
             sent_config, received_config);
  }

  // Require full round-trip: if we write then read, values must match.
  // A mismatch indicates broken SPI communication or wrong register usage.
  bool mode_ok = (read_config.drive_mode == config.drive_mode &&
                  read_config.bridge_mode == config.bridge_mode &&
                  read_config.parallel_mode == config.parallel_mode &&
                  read_config.polarity == config.polarity);
  bool current_ok = (read_config.hit_current == config.hit_current &&
                     read_config.hold_current == config.hold_current);
  bool timing_ok = (read_config.hit_time == config.hit_time);

  if (!mode_ok) {
    ESP_LOGE(TAG, "Channel configuration mismatch (config register)");
    ESP_LOGE(TAG, "  drive_mode: expected=%d read=%d",
             static_cast<int>(config.drive_mode),
             static_cast<int>(read_config.drive_mode));
    ESP_LOGE(TAG, "  bridge_mode: expected=%d read=%d",
             static_cast<int>(config.bridge_mode),
             static_cast<int>(read_config.bridge_mode));
    ESP_LOGE(TAG, "  (raw register: sent=0x%04" PRIX16 " received=0x%04" PRIX16 ")",
             sent_config, received_config);
    return false;
  }

  if (!current_ok || !timing_ok) {
    ESP_LOGE(TAG, "Channel current/timing read-back mismatch (communication/register failure)");
    ESP_LOGE(TAG, "  hit_current: expected=%u read=%u",
             static_cast<unsigned>(config.hit_current),
             static_cast<unsigned>(read_config.hit_current));
    ESP_LOGE(TAG, "  hold_current: expected=%u read=%u",
             static_cast<unsigned>(config.hold_current),
             static_cast<unsigned>(read_config.hold_current));
    ESP_LOGE(TAG, "  hit_time: expected=%u read=%u",
             static_cast<unsigned>(config.hit_time),
             static_cast<unsigned>(read_config.hit_time));
    ESP_LOGE(TAG, "  Fix: ensure SPI (MISO) and register map match MAX22200; expected must equal read after write.");
    return false;
  }

  ESP_LOGI(TAG, "Channel configuration test passed");
  return true;
}

/**
 * @brief Test fault status reading
 */
static bool test_fault_status() noexcept {
  if (!g_driver || !g_driver->IsInitialized()) {
    ESP_LOGE(TAG, "Driver not initialized");
    return false;
  }

  // Clear fault status
  DriverStatus status = g_driver->ClearFaultStatus();
  if (status != DriverStatus::OK) {
    ESP_LOGE(TAG, "Failed to clear fault status");
    return false;
  }

  // Read fault status
  FaultStatus fault_status;
  status = g_driver->ReadFaultStatus(fault_status);
  if (status != DriverStatus::OK) {
    ESP_LOGE(TAG, "Failed to read fault status");
    return false;
  }

  // Check if any faults are active
  bool has_fault = fault_status.hasFault();
  uint8_t fault_count = fault_status.getFaultCount();

  ESP_LOGI(TAG, "Fault status: has_fault=%d, count=%d", has_fault, fault_count);
  if (has_fault) {
    if (fault_status.overcurrent_protection)
      ESP_LOGI(TAG, "  fault: OCP (overcurrent protection)");
    if (fault_status.open_load)
      ESP_LOGI(TAG, "  fault: OL (open load)");
    if (fault_status.plunger_movement)
      ESP_LOGI(TAG, "  fault: DPM (plunger movement)");
    if (fault_status.undervoltage_lockout)
      ESP_LOGI(TAG, "  fault: UVLO (undervoltage lockout)");
    if (fault_status.hit_current_not_reached)
      ESP_LOGI(TAG, "  fault: HHF (hit current not reached)");
    if (fault_status.thermal_shutdown)
      ESP_LOGI(TAG, "  fault: TSD (thermal shutdown)");
  }

  ESP_LOGI(TAG, "Fault status test passed");
  return true;
}

/**
 * @brief Test control pins (ENABLE, CMD, FAULT) via bus interface
 */
static bool test_control_pins() noexcept {
  if (!g_spi_interface || !g_spi_interface->IsReady()) {
    ESP_LOGE(TAG, "SPI interface not ready");
    return false;
  }

  using namespace max22200;

  // CMD high = SPI register mode (required for our driver)
  g_spi_interface->GpioSetActive(CtrlPin::CMD);
  vTaskDelay(pdMS_TO_TICKS(10));

  // ENABLE: assert then deassert (exercise the pin)
  g_spi_interface->GpioSetActive(CtrlPin::ENABLE);
  vTaskDelay(pdMS_TO_TICKS(5));
  g_spi_interface->GpioSetInactive(CtrlPin::ENABLE);
  vTaskDelay(pdMS_TO_TICKS(5));
  g_spi_interface->GpioSetActive(CtrlPin::ENABLE);
  vTaskDelay(pdMS_TO_TICKS(5));

  // Read FAULT pin (hardware line)
  GpioSignal fault_signal = GpioSignal::INACTIVE;
  if (g_spi_interface->GpioRead(CtrlPin::FAULT, fault_signal)) {
    ESP_LOGI(TAG, "FAULT pin read: %s",
             fault_signal == GpioSignal::ACTIVE ? "ACTIVE (fault)" : "INACTIVE (ok)");
  } else {
    ESP_LOGW(TAG, "FAULT pin not configured or read failed");
  }

  ESP_LOGI(TAG, "Control pins test passed");
  return true;
}

/**
 * @brief Test TRIGA/TRIGB trigger pins (direct-drive triggers)
 */
static bool test_trigger_pins() noexcept {
  if (!g_spi_interface || !g_spi_interface->IsReady()) {
    ESP_LOGE(TAG, "SPI interface not ready");
    return false;
  }

  if (!g_spi_interface->HasTrigA() && !g_spi_interface->HasTrigB()) {
    ESP_LOGW(TAG, "TRIGA/TRIGB not configured, skipping trigger pin test");
    return true;
  }

  // Drive trigger pins high (inactive), then briefly low (trigger), then high again
  if (g_spi_interface->HasTrigA()) {
    g_spi_interface->SetTrigA(true);
    vTaskDelay(pdMS_TO_TICKS(2));
    g_spi_interface->SetTrigA(false);
    vTaskDelay(pdMS_TO_TICKS(2));
    g_spi_interface->SetTrigA(true);
    vTaskDelay(pdMS_TO_TICKS(2));
    ESP_LOGI(TAG, "TRIGA cycled (high -> low -> high)");
  }
  if (g_spi_interface->HasTrigB()) {
    g_spi_interface->SetTrigB(true);
    vTaskDelay(pdMS_TO_TICKS(2));
    g_spi_interface->SetTrigB(false);
    vTaskDelay(pdMS_TO_TICKS(2));
    g_spi_interface->SetTrigB(true);
    vTaskDelay(pdMS_TO_TICKS(2));
    ESP_LOGI(TAG, "TRIGB cycled (high -> low -> high)");
  }

  ESP_LOGI(TAG, "Trigger pins test passed");
  return true;
}

//=============================================================================
// MAIN TEST RUNNER
//=============================================================================

extern "C" void app_main() {
  ESP_LOGI(TAG,
           "╔═══════════════════════════════════════════════════════════╗");
  ESP_LOGI(TAG,
           "║        ESP32-C6 MAX22200 COMPREHENSIVE TEST SUITE         ║");
  ESP_LOGI(TAG,
           "║              HardFOC MAX22200 Driver Tests                ║");
  ESP_LOGI(TAG,
           "╚═══════════════════════════════════════════════════════════╝");

  vTaskDelay(pdMS_TO_TICKS(1000));

  // Report test section configuration
  print_test_section_status(TAG, "MAX22200");

  // Initialize test resources
  if (!init_test_resources()) {
    ESP_LOGE(TAG, "Failed to initialize test resources");
    return;
  }

  // Run all tests based on configuration
  RUN_TEST_SECTION_IF_ENABLED_WITH_PATTERN(
      ENABLE_BASIC_TESTS, "MAX22200 BASIC TESTS", 5,
      RUN_TEST_IN_TASK("basic_initialization", test_basic_initialization, 8192,
                       1);
      RUN_TEST_IN_TASK("channel_configuration", test_channel_configuration,
                       8192, 1);
      RUN_TEST_IN_TASK("fault_status", test_fault_status, 8192, 1);
      RUN_TEST_IN_TASK("control_pins", test_control_pins, 8192, 1);
      RUN_TEST_IN_TASK("trigger_pins", test_trigger_pins, 8192, 1);
      flip_test_progress_indicator(););

  // Cleanup
  cleanup_test_resources();

  // Print results
  print_test_summary(g_test_results, "MAX22200", TAG);

  // Blink GPIO14 to indicate completion
  output_section_indicator(5);

  cleanup_test_progress_indicator();

  while (true) {
    vTaskDelay(pdMS_TO_TICKS(10000));
  }
}

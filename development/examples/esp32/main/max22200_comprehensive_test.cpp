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

#include <memory>

#include "esp32_max22200_spi.hpp"
#include "max22200.hpp"
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
static std::unique_ptr<Esp32Max22200Spi> g_spi_interface;
static std::unique_ptr<max22200::MAX22200<Esp32Max22200Spi>> g_driver;

//=============================================================================
// TEST HELPER FUNCTIONS
//=============================================================================

/**
 * @brief Initialize test resources
 */
static bool init_test_resources() noexcept {
  // Create SPI interface
  Esp32Max22200Spi::SPIConfig spi_config;
  spi_config.host = SPI2_HOST;
  spi_config.miso_pin = GPIO_NUM_2;
  spi_config.mosi_pin = GPIO_NUM_7;
  spi_config.sclk_pin = GPIO_NUM_6;
  spi_config.cs_pin = GPIO_NUM_10;
  spi_config.frequency = 10000000; // 10 MHz
  spi_config.mode = 0;             // SPI Mode 0
  spi_config.queue_size = 1;

  g_spi_interface = std::make_unique<Esp32Max22200Spi>(spi_config);

  // Create driver instance
  g_driver = std::make_unique<max22200::MAX22200<Esp32Max22200Spi>>(*g_spi_interface);

  ESP_LOGI(TAG, "Test resources initialized");
  return true;
}

/**
 * @brief Cleanup test resources
 */
static void cleanup_test_resources() noexcept {
  if (g_driver && g_driver->IsInitialized()) {
    g_driver->Deinitialize();
  }
  g_driver.reset();
  g_spi_interface.reset();
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
  if (!max22200::MAX22200<Esp32Max22200Spi>::IsValidChannel(0)) {
    ESP_LOGE(TAG, "Channel 0 should be valid");
    return false;
  }

  if (max22200::MAX22200<Esp32Max22200Spi>::IsValidChannel(8)) {
    ESP_LOGE(TAG, "Channel 8 should be invalid");
    return false;
  }

  // Test version retrieval
  const char *version = max22200::MAX22200<Esp32Max22200Spi>::GetVersion();
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

  // Configure channel 0
  ChannelConfig config;
  config.enabled = true;
  config.drive_mode = DriveMode::CDR;
  config.bridge_mode = BridgeMode::HALF_BRIDGE;
  config.parallel_mode = false;
  config.polarity = OutputPolarity::NORMAL;
  config.hit_current = 500;
  config.hold_current = 200;
  config.hit_time = 1000;

  DriverStatus status = g_driver->ConfigureChannel(0, config);
  if (status != DriverStatus::OK) {
    ESP_LOGE(TAG, "Failed to configure channel 0");
    return false;
  }

  // Read back configuration
  ChannelConfig read_config;
  status = g_driver->GetChannelConfig(0, read_config);
  if (status != DriverStatus::OK) {
    ESP_LOGE(TAG, "Failed to read channel 0 configuration");
    return false;
  }

  // Verify configuration matches
  if (read_config.drive_mode != config.drive_mode ||
      read_config.bridge_mode != config.bridge_mode ||
      read_config.hit_current != config.hit_current ||
      read_config.hold_current != config.hold_current ||
      read_config.hit_time != config.hit_time) {
    ESP_LOGE(TAG, "Channel configuration mismatch");
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

  ESP_LOGI(TAG, "Fault status test passed");
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

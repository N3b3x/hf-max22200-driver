/**
 * @file esp32_max22200_test_config.hpp
 * @brief Hardware configuration for MAX22200 driver on ESP32-C6
 * 
 * This file contains the actual hardware configuration that is used by the HAL
 * and example applications. Modify these values to match your hardware setup.
 * 
 * @author N3b3x
 * @date 2025
 * @version 1.0.0
 */

#pragma once

#include <cstdint>

//==============================================================================
// COMPILE-TIME CONFIGURATION FLAGS
//==============================================================================

/**
 * @brief Enable detailed SPI transaction logging
 * 
 * @details
 * When enabled (set to 1), the Esp32Max22200Spi will log detailed
 * information about each SPI transaction including:
 * - TX/RX frame bytes
 * - Register read/write details
 * 
 * When disabled (set to 0), only basic error logging is performed.
 * 
 * Default: 0 (disabled) - Set to 1 to enable for debugging
 */
#ifndef ESP32_MAX22200_ENABLE_DETAILED_SPI_LOGGING
#define ESP32_MAX22200_ENABLE_DETAILED_SPI_LOGGING 0
#endif

namespace MAX22200_TestConfig {

/**
 * @brief SPI Configuration for ESP32-C6
 *
 * Hardware mapping: MISO 37, MOSI 35, SCK 36, CS 38.
 */
struct SPIPins {
    static constexpr uint8_t MISO = 37;          ///< GPIO37 - SPI MISO (Master In Slave Out)
    static constexpr uint8_t MOSI = 35;          ///< GPIO35 - SPI MOSI (Master Out Slave In)
    static constexpr uint8_t SCLK = 36;          ///< GPIO36 - SPI Clock (SCK)
    static constexpr uint8_t CS   = 38;          ///< GPIO38 - Chip Select (active low)
};

/**
 * @brief Control GPIO Pins for MAX22200
 *
 * EN=2, FAILTN=42 (fault), CMD=39, TRIGA=40, TRIGB=41.
 * Set to -1 if not connected/configured.
 */
struct ControlPins {
    static constexpr int8_t ENABLE = 2;          ///< Enable pin (active-high)
    static constexpr int8_t FAULT = 42;           ///< Fault output (FAILTN, active-low, inactive-high, open-drain)
    static constexpr int8_t CMD = 39;             ///< CMD pin (high = SPI register mode, low = direct drive)
    static constexpr int8_t TRIGA = 40;           ///< TRIGA trigger input (direct drive)
    static constexpr int8_t TRIGB = 41;           ///< TRIGB trigger input (direct drive)
};

/**
 * @brief SPI Communication Parameters
 * 
 * The MAX22200 supports SPI frequencies up to 10MHz standalone
 * and 5MHz when daisy-chained.
 * 
 * SPI Mode: CPOL=0, CPHA=0 (Mode 0)
 * Data format: 24-bit frames (8-bit address + 16-bit data)
 * 
 * MAX22200 SPI timing (per datasheet):
 * - Maximum SPI clock: 10 MHz (standalone), 5 MHz (daisy-chain)
 * - CS setup time: 30ns min before SCLK
 * - CS hold time: 0ns min after SCLK
 */
struct SPIParams {
    static constexpr uint32_t FREQUENCY = 10000000;  ///< 10MHz SPI frequency (standalone max)
    static constexpr uint8_t MODE = 0;               ///< SPI Mode 0 (CPOL=0, CPHA=0)
    static constexpr uint8_t QUEUE_SIZE = 1;         ///< Transaction queue size
    static constexpr uint8_t CS_ENA_PRETRANS = 1;    ///< CS asserted N clock cycles before transaction
    static constexpr uint8_t CS_ENA_POSTTRANS = 1;   ///< CS held N clock cycles after transaction
};

/**
 * @brief Channel Count and Limits
 * 
 * The MAX22200 is an octal (8-channel) solenoid/motor driver.
 */
struct ChannelLimits {
    static constexpr uint8_t NUM_CHANNELS = 8;             ///< Total number of channels
    static constexpr uint16_t MAX_HIT_CURRENT_MA = 1000;   ///< Maximum HIT current (mA)
    static constexpr uint16_t MAX_HOLD_CURRENT_MA = 500;   ///< Maximum HOLD current (mA)
    static constexpr uint16_t MAX_HIT_TIME_MS = 2000;      ///< Maximum HIT time (ms)
};

/**
 * @brief Supply Voltage Specifications (volts)
 * 
 * VM: Main power supply for the load outputs
 * VDD: Logic supply (3.3V or 5V)
 */
struct SupplyVoltage {
    static constexpr float VM_MIN = 4.5f;      ///< Minimum VM voltage (V)
    static constexpr float VM_NOM = 24.0f;     ///< Nominal VM voltage (V)
    static constexpr float VM_MAX = 36.0f;     ///< Maximum VM voltage (V)
    static constexpr float VDD_NOM = 3.3f;     ///< Logic supply voltage (V)
};

/**
 * @brief Temperature Specifications (celsius)
 * 
 * Operating temperature range and limits from datasheet.
 */
struct Temperature {
    static constexpr int16_t OPERATING_MIN = -40;    ///< Minimum operating temperature (°C)
    static constexpr int16_t OPERATING_MAX = 125;    ///< Maximum operating temperature (°C)
    static constexpr int16_t JUNCTION_MAX = 150;     ///< Maximum junction temperature (°C)
    static constexpr int16_t WARNING_THRESHOLD = 120; ///< Temperature warning threshold (°C)
};

/**
 * @brief Timing Parameters
 * 
 * Timing requirements from the MAX22200 datasheet.
 */
struct Timing {
    static constexpr uint16_t POWER_ON_DELAY_MS = 10;     ///< Power-on initialization delay (ms)
    static constexpr uint16_t RESET_DELAY_MS = 5;         ///< Reset recovery time (ms)
    static constexpr uint16_t INTER_FRAME_US = 1;         ///< Minimum time between SPI frames (μs)
};

/**
 * @brief Diagnostic Thresholds
 * 
 * Thresholds for fault detection and diagnostics.
 */
struct Diagnostics {
    static constexpr uint16_t POLL_INTERVAL_MS = 100;      ///< Diagnostic polling interval (ms)
    static constexpr uint8_t MAX_RETRY_COUNT = 3;          ///< Maximum communication retries
};

/**
 * @brief Test Configuration
 * 
 * Default parameters for testing and calibration.
 */
struct TestConfig {
    static constexpr uint16_t DEFAULT_HIT_CURRENT_MA = 500;    ///< Default HIT current for tests (mA)
    static constexpr uint16_t DEFAULT_HOLD_CURRENT_MA = 200;   ///< Default HOLD current for tests (mA)
    static constexpr uint16_t DEFAULT_HIT_TIME_MS = 1000;      ///< Default HIT time for tests (ms)
    static constexpr uint16_t TEST_DURATION_MS = 5000;         ///< Test duration (ms)
    static constexpr uint16_t RAMP_STEP_DELAY_MS = 500;        ///< Delay between ramp steps (ms)
};

/**
 * @brief Application-specific Configuration
 * 
 * Configuration values that can be adjusted per application.
 */
struct AppConfig {
    // Logging
    static constexpr bool ENABLE_DEBUG_LOGGING = true;     ///< Enable detailed debug logs
    static constexpr bool ENABLE_SPI_LOGGING = false;      ///< Enable SPI transaction logs
    
    // Performance
    static constexpr bool ENABLE_PERFORMANCE_MONITORING = true;  ///< Enable performance metrics
    static constexpr uint16_t STATS_REPORT_INTERVAL_MS = 10000;  ///< Statistics reporting interval
    
    // Error handling
    static constexpr bool ENABLE_AUTO_RECOVERY = true;     ///< Enable automatic error recovery
    static constexpr uint8_t MAX_ERROR_COUNT = 10;         ///< Maximum errors before failsafe
};

} // namespace MAX22200_TestConfig

/**
 * @brief Hardware configuration validation
 * 
 * Compile-time checks to ensure configuration is valid.
 */
static_assert(MAX22200_TestConfig::SPIParams::FREQUENCY <= 10000000,
              "SPI frequency exceeds MAX22200 standalone maximum of 10MHz");

static_assert(MAX22200_TestConfig::SPIParams::MODE == 0,
              "MAX22200 requires SPI Mode 0 (CPOL=0, CPHA=0)");

static_assert(MAX22200_TestConfig::ChannelLimits::NUM_CHANNELS == 8,
              "MAX22200 has exactly 8 channels");

/**
 * @brief Helper macro for compile-time GPIO pin validation (ESP32-C6 allows 0–48)
 */
#define MAX22200_VALIDATE_GPIO(pin) \
    static_assert((pin) >= 0 && (pin) <= 48, "Invalid GPIO pin number for ESP32-C6")

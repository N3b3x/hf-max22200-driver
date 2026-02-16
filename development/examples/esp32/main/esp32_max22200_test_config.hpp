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
 * @brief SPI pin assignment for MAX22200 (SDI/SDO/SCK/CSB)
 *
 * Hardware mapping: MISO 37, MOSI 35, SCK 36, CS 38.
 */
struct SPIPins {
    static constexpr uint8_t MISO = 35;  ///< GPIO35 - SPI MISO (SDO from MAX22200)
    static constexpr uint8_t MOSI = 37;  ///< GPIO37 - SPI MOSI (SDI to MAX22200)
    static constexpr uint8_t SCLK = 36;  ///< GPIO36 - SPI Clock (SCK)
    static constexpr uint8_t CS   = 38;  ///< GPIO38 - Chip Select (CSB, active low)
};

/**
 * @brief Control GPIO Pins for MAX22200
 *
 * EN=2, FAILTN=42 (fault), CMD=39, TRIGA=40, TRIGB=41.
 * Set to -1 if not connected/configured.
 */
struct ControlPins {
    static constexpr int8_t ENABLE = 2;   ///< Enable pin (active-high)
    static constexpr int8_t FAULT = 42;   ///< Fault output (nFAULT, active-low, open-drain)
    static constexpr int8_t CMD = 39;     ///< CMD pin (HIGH=Command Reg write, LOW=data transfer)
    static constexpr int8_t TRIGA = 40;   ///< TRIGA trigger input
    static constexpr int8_t TRIGB = 41;   ///< TRIGB trigger input
};

/**
 * @brief SPI Communication Parameters
 *
 * MAX22200 SPI protocol (per datasheet Rev 1, 3/25):
 * - SPI Mode 0: CPOL=0, CPHA=0
 * - Maximum SPI clock: 10 MHz (standalone), 5 MHz (daisy-chain)
 * - Two-phase protocol:
 *   Phase 1: CMD pin HIGH → write 8-bit Command Register
 *   Phase 2: CMD pin LOW  → read/write 8 or 32 bits of data
 * - Registers are 32-bit (or 8-bit MSB-only mode)
 */
struct SPIParams {
    static constexpr uint32_t FREQUENCY = 1000000;  ///< 1MHz SPI frequency
    static constexpr uint8_t MODE = 0;               ///< SPI Mode 0 (CPOL=0, CPHA=0) per datasheet
    static constexpr uint8_t QUEUE_SIZE = 1;         ///< Transaction queue size
    static constexpr uint8_t CS_ENA_PRETRANS = 1;    ///< CS setup cycles before transaction
    static constexpr uint8_t CS_ENA_POSTTRANS = 1;   ///< CS hold cycles after transaction
};

/**
 * @brief Channel Count and Register Limits (per datasheet)
 *
 * HIT/HOLD currents are 7-bit (0-127), HIT time is 8-bit (0-255).
 */
struct ChannelLimits {
    static constexpr uint8_t NUM_CHANNELS = 8;       ///< Total number of channels
    static constexpr uint8_t MAX_HIT_CURRENT = 127;  ///< Maximum HIT current register value (7-bit)
    static constexpr uint8_t MAX_HOLD_CURRENT = 127; ///< Maximum HOLD current register value (7-bit)
    static constexpr uint8_t MAX_HIT_TIME = 255;     ///< Maximum HIT time register value (8-bit)
};

/**
 * @brief Supply Voltage Specifications (volts)
 */
struct SupplyVoltage {
    static constexpr float VM_MIN = 4.5f;    ///< Minimum VM voltage (V)
    static constexpr float VM_NOM = 24.0f;   ///< Nominal VM voltage (V)
    static constexpr float VM_MAX = 36.0f;   ///< Maximum VM voltage (V)
    static constexpr float VDD_NOM = 3.3f;   ///< Logic supply voltage (V)
};

/**
 * @brief Temperature Specifications (celsius)
 */
struct Temperature {
    static constexpr int16_t OPERATING_MIN = -40;     ///< Minimum operating temp (°C)
    static constexpr int16_t OPERATING_MAX = 85;      ///< Maximum operating temp (°C) per datasheet
    static constexpr int16_t TSD_THRESHOLD = 145;     ///< Thermal shutdown threshold (°C)
};

/**
 * @brief Timing Parameters (per datasheet)
 */
struct Timing {
    static constexpr uint16_t POWER_UP_DELAY_US = 500;    ///< Power-up delay after ENABLE (μs)
    static constexpr uint16_t INTER_FRAME_US = 1;         ///< Min time between SPI frames (μs)
};

/**
 * @brief Diagnostic Thresholds
 */
struct Diagnostics {
    static constexpr uint16_t POLL_INTERVAL_MS = 100;   ///< Diagnostic polling interval (ms)
    static constexpr uint8_t MAX_RETRY_COUNT = 3;       ///< Maximum communication retries
};

/**
 * @brief Test Configuration
 *
 * Default parameters for testing. Values are register values (0-127 / 0-255).
 */
struct TestConfig {
    static constexpr uint8_t DEFAULT_HIT_CURRENT = 80;    ///< Default HIT current (7-bit register)
    static constexpr uint8_t DEFAULT_HOLD_CURRENT = 40;   ///< Default HOLD current (7-bit register)
    static constexpr uint8_t DEFAULT_HIT_TIME = 100;      ///< Default HIT time (8-bit register)
    static constexpr uint16_t TEST_DURATION_MS = 5000;    ///< Test duration (ms)
};

/**
 * @brief Application-specific Configuration
 */
struct AppConfig {
    static constexpr bool ENABLE_DEBUG_LOGGING = true;
    static constexpr bool ENABLE_SPI_LOGGING = false;
    static constexpr bool ENABLE_PERFORMANCE_MONITORING = true;
    static constexpr uint16_t STATS_REPORT_INTERVAL_MS = 10000;
    static constexpr bool ENABLE_AUTO_RECOVERY = true;
    static constexpr uint8_t MAX_ERROR_COUNT = 10;
};

/**
 * @brief Parker C21 valve Hit and Hold (compile-time CDR vs VDR)
 *
 * Per C21 datasheet:
 * - Hit: rated voltage; Hold: 50% of rated
 * - Min hit time: 100 ms; PWM freq min: 1 kHz; Hold duty: 50%
 *
 * Set to true to drive C21 with CDR (current regulation, low-side only);
 * false for VDR (PWM duty: hit=100%, hold=50%).
 */
struct C21ValveConfig {
    static constexpr bool USE_CDR = true;   ///< true = CDR (current), false = VDR (PWM duty)
    static constexpr float HIT_TIME_MS = 100.0f;   ///< Min hit time per C21 (ms)
    static constexpr float HOLD_PERCENT = 50.0f;   ///< Hold level: 50% of rated (duty or current)
    static constexpr float HIT_PERCENT = 100.0f;   ///< Hit: full (100% duty or 100% IFS)
    static constexpr uint8_t CHANNEL = 0;           ///< Channel used for C21 (low-side)

    /// Rated current in mA (C21 valve spec). In CDR mode: hit = RATED_CURRENT_MA, hold = 50% of this.
    /// Board IFS must be >= this (e.g. set RREF so IFS >= RATED_CURRENT_MA).
    static constexpr uint32_t RATED_CURRENT_MA = 500u;
};

} // namespace MAX22200_TestConfig

//==============================================================================
// Compile-time validation
//==============================================================================

static_assert(MAX22200_TestConfig::SPIParams::FREQUENCY <= 10000000,
              "SPI frequency exceeds MAX22200 standalone maximum of 10MHz");

static_assert(MAX22200_TestConfig::SPIParams::MODE == 0,
              "MAX22200 requires SPI Mode 0 (CPOL=0, CPHA=0) per datasheet");

static_assert(MAX22200_TestConfig::ChannelLimits::NUM_CHANNELS == 8,
              "MAX22200 has exactly 8 channels");

/**
 * @brief Helper macro for compile-time GPIO pin validation (ESP32-C6 allows 0-48)
 */
#define MAX22200_VALIDATE_GPIO(pin) \
    static_assert((pin) >= 0 && (pin) <= 48, "Invalid GPIO pin number for ESP32-C6")

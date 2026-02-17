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
 * When enabled (set to 1), the Esp32Max22200SpiBus will log detailed
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

/**
 * @brief Enable verbose bus init/pin logging
 *
 * @details
 * When enabled (set to 1), the Esp32Max22200SpiBus will log:
 * - "SPI interface initialized successfully"
 * - Per-pin init messages (ENABLE, CMD, TRIGA, TRIGB, FAULT)
 *
 * When disabled (set to 0), only ESP_LOGE (real failures) and the MISO pullup
 * warning (if it fails) are logged from the bus.
 *
 * Default: 0 (disabled) - Set to 1 for bring-up or pin debugging
 */
#ifndef ESP32_MAX22200_ENABLE_VERBOSE_BUS_LOGGING
#define ESP32_MAX22200_ENABLE_VERBOSE_BUS_LOGGING 1
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
 * @brief Board configuration for tests (single board: RREF via short = 15 kΩ)
 *
 * Board supports 30 kΩ or 15 kΩ (short on board by default = 15 kΩ).
 * IFS = KFS×1000/RREF with KFS = 15k (HFS=0) or 7.5k (HFS=1). 15 kΩ → IFS = 1000 mA.
 * NUM_CHANNELS: number of channels exposed on this board (e.g. 8 full, or 4 if only half wired).
 */
struct BoardTestConfig {
    static constexpr float RREF_KOHM = 15.0f;   ///< RREF in kΩ (short default = 15 kΩ → IFS = 1000 mA)
    static constexpr bool HFS = false;          ///< Half full-scale (false = full-scale)
    static constexpr uint8_t NUM_CHANNELS = 8;   ///< Channels exposed on this board (MAX22200 has 8 max)
    static constexpr uint32_t MAX_CURRENT_MA = 800;   ///< Optional safety limit (0 = no limit)
    static constexpr uint8_t MAX_DUTY_PERCENT = 90;   ///< Optional for VDR (0 = no limit)
};

/**
 * @brief Parker C21 valve Hit and Hold (compile-time CDR vs VDR)
 *
 * CDR mode: use HIT_CURRENT_MA and HOLD_CURRENT_MA (explicit currents).
 * VDR mode: use HIT_PERCENT and HOLD_PERCENT (duty %, 0–100).
 * Min hit time per C21: 100 ms; PWM freq min 1 kHz.
 */
struct C21ValveConfig {
    static constexpr bool USE_CDR = false;   ///< true = CDR (current), false = VDR (PWM duty)
    static constexpr float HIT_TIME_MS = 100.0f;   ///< Min hit time per C21 (ms)

    /// CDR mode: hit and hold current in mA (board IFS must be >= these)
    static constexpr float HIT_CURRENT_MA = 500.0f;
    static constexpr float HOLD_CURRENT_MA = 250.0f;

    /// VDR mode only: duty cycle 0–100%
    static constexpr float HIT_PERCENT = 100.0f;
    static constexpr float HOLD_PERCENT = 50.0f;

    static constexpr uint8_t CHANNEL = 0;   ///< Channel used for C21 (low-side)
};

/**
 * @brief Solenoid valve test pattern timing (used by max22200_solenoid_valve_test)
 *
 * Sequential pattern: ch0 → ch1 → … → ch7; each channel on for SEQUENTIAL_HIT_MS,
 * SEQUENTIAL_GAP_MS between channels. Parallel pattern: all channels on for
 * PARALLEL_HOLD_MS. PATTERN_PAUSE_MS is the pause between pattern runs.
 */
struct SolenoidValvePatternConfig {
    static constexpr uint32_t SEQUENTIAL_HIT_MS = 200;   ///< Time (ms) each channel is on in sequential pattern
    static constexpr uint32_t SEQUENTIAL_GAP_MS = 80;   ///< Delay (ms) between channels in sequential pattern
    static constexpr uint32_t PARALLEL_HOLD_MS = 500;   ///< Time (ms) all channels on in parallel pattern
    static constexpr uint32_t PATTERN_PAUSE_MS = 400;   ///< Pause (ms) between pattern runs
};

} // namespace MAX22200_TestConfig

//==============================================================================
// Compile-time validation
//==============================================================================

static_assert(MAX22200_TestConfig::SPIParams::FREQUENCY <= 10000000,
              "SPI frequency exceeds MAX22200 standalone maximum of 10MHz");

static_assert(MAX22200_TestConfig::SPIParams::MODE == 0,
              "MAX22200 requires SPI Mode 0 (CPOL=0, CPHA=0) per datasheet");

static_assert(MAX22200_TestConfig::BoardTestConfig::NUM_CHANNELS >= 1u && MAX22200_TestConfig::BoardTestConfig::NUM_CHANNELS <= 8u,
              "NUM_CHANNELS must be 1..8 (MAX22200 has 8 channels)");

/**
 * @brief Helper macro for compile-time GPIO pin validation (ESP32-C6 allows 0-48)
 */
#define MAX22200_VALIDATE_GPIO(pin) \
    static_assert((pin) >= 0 && (pin) <= 48, "Invalid GPIO pin number for ESP32-C6")

/**
 * @file max22200_registers.hpp
 * @brief Register definitions and constants for MAX22200 IC
 * @author MAX22200 Driver Library
 * @date 2024
 *
 * This file contains all register addresses, bit field definitions,
 * and constants for the MAX22200 octal solenoid and motor driver.
 */

#ifndef MAX22200_REGISTERS_H
#define MAX22200_REGISTERS_H

#include <cstdint>

namespace max22200 {

/**
 * @brief Number of channels on the MAX22200
 */
constexpr uint8_t NUM_CHANNELS_ = 8;

/**
 * @brief Maximum SPI clock frequency without daisy chaining (Hz)
 */
constexpr uint32_t MAX_SPI_FREQ_STANDALONE_ = 10000000; // 10 MHz

/**
 * @brief Maximum SPI clock frequency with daisy chaining (Hz)
 */
constexpr uint32_t MAX_SPI_FREQ_DAISY_CHAIN_ = 5000000; // 5 MHz

/**
 * @brief Register addresses for MAX22200
 */
namespace Registers {
// Control and Configuration Registers
constexpr uint8_t GLOBAL_CONFIG = 0x00;  ///< Global configuration register
constexpr uint8_t CHANNEL_ENABLE = 0x01; ///< Channel enable register
constexpr uint8_t FAULT_STATUS = 0x02;   ///< Fault status register
constexpr uint8_t FAULT_MASK = 0x03;     ///< Fault mask register
constexpr uint8_t DIAGNOSTIC = 0x04;     ///< Diagnostic register

// Channel-specific registers (0x10-0x17 for channels 0-7)
constexpr uint8_t CH0_CONFIG = 0x10; ///< Channel 0 configuration
constexpr uint8_t CH1_CONFIG = 0x11; ///< Channel 1 configuration
constexpr uint8_t CH2_CONFIG = 0x12; ///< Channel 2 configuration
constexpr uint8_t CH3_CONFIG = 0x13; ///< Channel 3 configuration
constexpr uint8_t CH4_CONFIG = 0x14; ///< Channel 4 configuration
constexpr uint8_t CH5_CONFIG = 0x15; ///< Channel 5 configuration
constexpr uint8_t CH6_CONFIG = 0x16; ///< Channel 6 configuration
constexpr uint8_t CH7_CONFIG = 0x17; ///< Channel 7 configuration

// Current control registers (0x20-0x27 for channels 0-7)
constexpr uint8_t CH0_CURRENT = 0x20; ///< Channel 0 current control
constexpr uint8_t CH1_CURRENT = 0x21; ///< Channel 1 current control
constexpr uint8_t CH2_CURRENT = 0x22; ///< Channel 2 current control
constexpr uint8_t CH3_CURRENT = 0x23; ///< Channel 3 current control
constexpr uint8_t CH4_CURRENT = 0x24; ///< Channel 4 current control
constexpr uint8_t CH5_CURRENT = 0x25; ///< Channel 5 current control
constexpr uint8_t CH6_CURRENT = 0x26; ///< Channel 6 current control
constexpr uint8_t CH7_CURRENT = 0x27; ///< Channel 7 current control

// Timing control registers (0x30-0x37 for channels 0-7)
constexpr uint8_t CH0_TIMING = 0x30; ///< Channel 0 timing control
constexpr uint8_t CH1_TIMING = 0x31; ///< Channel 1 timing control
constexpr uint8_t CH2_TIMING = 0x32; ///< Channel 2 timing control
constexpr uint8_t CH3_TIMING = 0x33; ///< Channel 3 timing control
constexpr uint8_t CH4_TIMING = 0x34; ///< Channel 4 timing control
constexpr uint8_t CH5_TIMING = 0x35; ///< Channel 5 timing control
constexpr uint8_t CH6_TIMING = 0x36; ///< Channel 6 timing control
constexpr uint8_t CH7_TIMING = 0x37; ///< Channel 7 timing control
} // namespace Registers

/**
 * @brief Bit field definitions for Global Configuration Register (0x00)
 */
namespace GlobalConfigBits {
constexpr uint8_t RESET_POS = 0;     ///< Reset bit position
constexpr uint8_t RESET_MASK = 0x01; ///< Reset bit mask

constexpr uint8_t SLEEP_POS = 1;     ///< Sleep mode bit position
constexpr uint8_t SLEEP_MASK = 0x02; ///< Sleep mode bit mask

constexpr uint8_t DIAG_EN_POS = 2;     ///< Diagnostic enable bit position
constexpr uint8_t DIAG_EN_MASK = 0x04; ///< Diagnostic enable bit mask

constexpr uint8_t ICS_EN_POS =
    3; ///< Integrated Current Sensing enable bit position
constexpr uint8_t ICS_EN_MASK =
    0x08; ///< Integrated Current Sensing enable bit mask

constexpr uint8_t DAISY_CHAIN_POS = 4;     ///< Daisy chain mode bit position
constexpr uint8_t DAISY_CHAIN_MASK = 0x10; ///< Daisy chain mode bit mask
} // namespace GlobalConfigBits

/**
 * @brief Bit field definitions for Channel Enable Register (0x01)
 */
namespace ChannelEnable {
constexpr uint8_t CH0_EN_POS = 0;     ///< Channel 0 enable bit position
constexpr uint8_t CH0_EN_MASK = 0x01; ///< Channel 0 enable bit mask

constexpr uint8_t CH1_EN_POS = 1;     ///< Channel 1 enable bit position
constexpr uint8_t CH1_EN_MASK = 0x02; ///< Channel 1 enable bit mask

constexpr uint8_t CH2_EN_POS = 2;     ///< Channel 2 enable bit position
constexpr uint8_t CH2_EN_MASK = 0x04; ///< Channel 2 enable bit mask

constexpr uint8_t CH3_EN_POS = 3;     ///< Channel 3 enable bit position
constexpr uint8_t CH3_EN_MASK = 0x08; ///< Channel 3 enable bit mask

constexpr uint8_t CH4_EN_POS = 4;     ///< Channel 4 enable bit position
constexpr uint8_t CH4_EN_MASK = 0x10; ///< Channel 4 enable bit mask

constexpr uint8_t CH5_EN_POS = 5;     ///< Channel 5 enable bit position
constexpr uint8_t CH5_EN_MASK = 0x20; ///< Channel 5 enable bit mask

constexpr uint8_t CH6_EN_POS = 6;     ///< Channel 6 enable bit position
constexpr uint8_t CH6_EN_MASK = 0x40; ///< Channel 6 enable bit mask

constexpr uint8_t CH7_EN_POS = 7;     ///< Channel 7 enable bit position
constexpr uint8_t CH7_EN_MASK = 0x80; ///< Channel 7 enable bit mask
} // namespace ChannelEnable

/**
 * @brief Bit field definitions for Fault Status Register (0x02)
 */
namespace FaultStatusBits {
constexpr uint8_t OCP_POS = 0;     ///< Overcurrent protection bit position
constexpr uint8_t OCP_MASK = 0x01; ///< Overcurrent protection bit mask

constexpr uint8_t OL_POS = 1;     ///< Open load detection bit position
constexpr uint8_t OL_MASK = 0x02; ///< Open load detection bit mask

constexpr uint8_t DPM_POS = 2; ///< Detection of plunger movement bit position
constexpr uint8_t DPM_MASK = 0x04; ///< Detection of plunger movement bit mask

constexpr uint8_t UVLO_POS = 3;     ///< Undervoltage lockout bit position
constexpr uint8_t UVLO_MASK = 0x08; ///< Undervoltage lockout bit mask

constexpr uint8_t HHF_POS = 4;     ///< HIT current not reached bit position
constexpr uint8_t HHF_MASK = 0x10; ///< HIT current not reached bit mask

constexpr uint8_t TSD_POS = 5;     ///< Thermal shutdown bit position
constexpr uint8_t TSD_MASK = 0x20; ///< Thermal shutdown bit mask
} // namespace FaultStatusBits

/**
 * @brief Bit field definitions for Channel Configuration Register (0x10-0x17)
 */
namespace ChannelConfigBits {
constexpr uint8_t DRIVE_MODE_POS =
    0; ///< Drive mode bit position (0=CDR, 1=VDR)
constexpr uint8_t DRIVE_MODE_MASK = 0x01; ///< Drive mode bit mask

constexpr uint8_t BRIDGE_MODE_POS =
    1; ///< Bridge mode bit position (0=half, 1=full)
constexpr uint8_t BRIDGE_MODE_MASK = 0x02; ///< Bridge mode bit mask

constexpr uint8_t PARALLEL_POS = 2;     ///< Parallel mode bit position
constexpr uint8_t PARALLEL_MASK = 0x04; ///< Parallel mode bit mask

constexpr uint8_t POLARITY_POS = 3;     ///< Output polarity bit position
constexpr uint8_t POLARITY_MASK = 0x08; ///< Output polarity bit mask
} // namespace ChannelConfigBits

// Forward declarations - actual definitions are in max22200_types.hpp
enum class DriveMode : uint8_t;
enum class BridgeMode : uint8_t;
enum class OutputPolarity : uint8_t;
enum class FaultType : uint8_t;

/**
 * @brief Current range definitions
 */
namespace CurrentRange {
constexpr uint16_t MIN_HIT_CURRENT = 0;     ///< Minimum HIT current setting
constexpr uint16_t MAX_HIT_CURRENT = 1023;  ///< Maximum HIT current setting
constexpr uint16_t MIN_HOLD_CURRENT = 0;    ///< Minimum HOLD current setting
constexpr uint16_t MAX_HOLD_CURRENT = 1023; ///< Maximum HOLD current setting
} // namespace CurrentRange

/**
 * @brief Timing range definitions
 */
namespace TimingRange {
constexpr uint16_t MIN_HIT_TIME = 0;     ///< Minimum HIT time setting
constexpr uint16_t MAX_HIT_TIME = 65535; ///< Maximum HIT time setting
} // namespace TimingRange

/**
 * @brief Helper function to get channel configuration register address
 * @param channel Channel number (0-7)
 * @return Register address for the specified channel
 */
constexpr uint8_t getChannelConfigReg(uint8_t channel) {
  return Registers::CH0_CONFIG + channel;
}

/**
 * @brief Helper function to get channel current register address
 * @param channel Channel number (0-7)
 * @return Register address for the specified channel
 */
constexpr uint8_t getChannelCurrentReg(uint8_t channel) {
  return Registers::CH0_CURRENT + channel;
}

/**
 * @brief Helper function to get channel timing register address
 * @param channel Channel number (0-7)
 * @return Register address for the specified channel
 */
constexpr uint8_t getChannelTimingReg(uint8_t channel) {
  return Registers::CH0_TIMING + channel;
}

} // namespace max22200

#endif // MAX22200_REGISTERS_H

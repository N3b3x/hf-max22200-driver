/**
 * @file MAX22200_Types.h
 * @brief Type definitions and structures for MAX22200 driver
 * @author MAX22200 Driver Library
 * @date 2024
 *
 * This file contains all type definitions, structures, and enums
 * used by the MAX22200 driver library.
 */

#ifndef MAX22200_TYPES_H
#define MAX22200_TYPES_H

#include "MAX22200_Registers.h"
#include <array>
#include <cstdint>

namespace MAX22200 {

/**
 * @brief Drive mode enumeration
 */
enum class DriveMode : uint8_t {
  CDR = 0, ///< Current Drive Regulation
  VDR = 1  ///< Voltage Drive Regulation
};

/**
 * @brief Bridge mode enumeration
 */
enum class BridgeMode : uint8_t {
  HALF_BRIDGE = 0, ///< Half-bridge mode
  FULL_BRIDGE = 1  ///< Full-bridge mode
};

/**
 * @brief Output polarity enumeration
 */
enum class OutputPolarity : uint8_t {
  NORMAL = 0,  ///< Normal polarity
  INVERTED = 1 ///< Inverted polarity
};

/**
 * @brief Fault type enumeration
 */
enum class FaultType : uint8_t {
  OCP = 0,  ///< Overcurrent protection
  OL = 1,   ///< Open load detection
  DPM = 2,  ///< Detection of plunger movement
  UVLO = 3, ///< Undervoltage lockout
  HHF = 4,  ///< HIT current not reached
  TSD = 5   ///< Thermal shutdown
};

/**
 * @brief Channel configuration structure
 *
 * This structure contains all configuration parameters for a single channel
 * of the MAX22200 driver.
 */
struct ChannelConfig {
  bool enabled;            ///< Channel enable state
  DriveMode drive_mode;    ///< Drive mode (CDR or VDR)
  BridgeMode bridge_mode;  ///< Bridge mode (half or full)
  bool parallel_mode;      ///< Parallel mode enable
  OutputPolarity polarity; ///< Output polarity
  uint16_t hit_current;    ///< HIT current setting (0-1023)
  uint16_t hold_current;   ///< HOLD current setting (0-1023)
  uint16_t hit_time;       ///< HIT time setting (0-65535)

  /**
   * @brief Default constructor
   *
   * Initializes the channel configuration with default values.
   */
  ChannelConfig()
      : enabled(false), drive_mode(DriveMode::CDR), bridge_mode(BridgeMode::HALF_BRIDGE),
        parallel_mode(false), polarity(OutputPolarity::NORMAL), hit_current(0), hold_current(0),
        hit_time(0) {}

  /**
   * @brief Constructor with parameters
   *
   * @param en Channel enable state
   * @param dm Drive mode
   * @param bm Bridge mode
   * @param pm Parallel mode
   * @param pol Output polarity
   * @param hc HIT current
   * @param hdc HOLD current
   * @param ht HIT time
   */
  ChannelConfig(bool en, DriveMode dm, BridgeMode bm, bool pm, OutputPolarity pol, uint16_t hc,
                uint16_t hdc, uint16_t ht)
      : enabled(en), drive_mode(dm), bridge_mode(bm), parallel_mode(pm), polarity(pol),
        hit_current(hc), hold_current(hdc), hit_time(ht) {}
};

/**
 * @brief Global configuration structure
 *
 * This structure contains global configuration parameters for the MAX22200.
 */
struct GlobalConfig {
  bool reset;             ///< Reset state
  bool sleep_mode;        ///< Sleep mode enable
  bool diagnostic_enable; ///< Diagnostic enable
  bool ics_enable;        ///< Integrated Current Sensing enable
  bool daisy_chain_mode;  ///< Daisy chain mode enable

  /**
   * @brief Default constructor
   *
   * Initializes the global configuration with default values.
   */
  GlobalConfig()
      : reset(false), sleep_mode(false), diagnostic_enable(true), ics_enable(true),
        daisy_chain_mode(false) {}
};

/**
 * @brief Fault status structure
 *
 * This structure contains the fault status information for the MAX22200.
 */
struct FaultStatus {
  bool overcurrent_protection;  ///< Overcurrent protection fault
  bool open_load;               ///< Open load detection fault
  bool plunger_movement;        ///< Detection of plunger movement
  bool undervoltage_lockout;    ///< Undervoltage lockout fault
  bool hit_current_not_reached; ///< HIT current not reached fault
  bool thermal_shutdown;        ///< Thermal shutdown fault

  /**
   * @brief Default constructor
   *
   * Initializes all fault flags to false.
   */
  FaultStatus()
      : overcurrent_protection(false), open_load(false), plunger_movement(false),
        undervoltage_lockout(false), hit_current_not_reached(false), thermal_shutdown(false) {}

  /**
   * @brief Check if any fault is active
   * @return true if any fault is active, false otherwise
   */
  bool hasFault() const {
    return overcurrent_protection || open_load || plunger_movement || undervoltage_lockout ||
           hit_current_not_reached || thermal_shutdown;
  }

  /**
   * @brief Get fault count
   * @return Number of active faults
   */
  uint8_t getFaultCount() const {
    uint8_t count = 0;
    if (overcurrent_protection)
      count++;
    if (open_load)
      count++;
    if (plunger_movement)
      count++;
    if (undervoltage_lockout)
      count++;
    if (hit_current_not_reached)
      count++;
    if (thermal_shutdown)
      count++;
    return count;
  }
};

/**
 * @brief Channel status structure
 *
 * This structure contains the current status of a single channel.
 */
struct ChannelStatus {
  bool enabled;             ///< Channel enable state
  bool fault_active;        ///< Channel fault state
  uint16_t current_reading; ///< Current reading from ICS
  bool hit_phase_active;    ///< HIT phase active state

  /**
   * @brief Default constructor
   *
   * Initializes the channel status with default values.
   */
  ChannelStatus()
      : enabled(false), fault_active(false), current_reading(0), hit_phase_active(false) {}
};

/**
 * @brief Driver status enumeration
 */
enum class DriverStatus : uint8_t {
  OK = 0,               ///< Driver is functioning normally
  INITIALIZATION_ERROR, ///< Initialization failed
  COMMUNICATION_ERROR,  ///< SPI communication error
  INVALID_PARAMETER,    ///< Invalid parameter provided
  HARDWARE_FAULT,       ///< Hardware fault detected
  TIMEOUT               ///< Operation timeout
};

/**
 * @brief Channel state enumeration
 */
enum class ChannelState : uint8_t {
  DISABLED = 0, ///< Channel is disabled
  ENABLED,      ///< Channel is enabled
  HIT_PHASE,    ///< Channel is in HIT phase
  HOLD_PHASE,   ///< Channel is in HOLD phase
  FAULT         ///< Channel has a fault
};

/**
 * @brief Array type for channel configurations
 */
using ChannelConfigArray = std::array<ChannelConfig, NUM_CHANNELS>;

/**
 * @brief Array type for channel statuses
 */
using ChannelStatusArray = std::array<ChannelStatus, NUM_CHANNELS>;

/**
 * @brief Array type for channel states
 */
using ChannelStateArray = std::array<ChannelState, NUM_CHANNELS>;

/**
 * @brief Callback function type for fault events
 *
 * @param channel Channel number (0-7)
 * @param fault_type Type of fault that occurred
 * @param user_data User-provided data pointer
 */
using FaultCallback = void (*)(uint8_t channel, FaultType fault_type, void* user_data);

/**
 * @brief Callback function type for channel state changes
 *
 * @param channel Channel number (0-7)
 * @param old_state Previous channel state
 * @param new_state New channel state
 * @param user_data User-provided data pointer
 */
using StateChangeCallback = void (*)(uint8_t channel, ChannelState old_state,
                                     ChannelState new_state, void* user_data);

/**
 * @brief Driver statistics structure
 *
 * This structure contains runtime statistics for the MAX22200 driver.
 */
struct DriverStatistics {
  uint32_t total_transfers;  ///< Total number of SPI transfers
  uint32_t failed_transfers; ///< Number of failed SPI transfers
  uint32_t fault_events;     ///< Number of fault events
  uint32_t state_changes;    ///< Number of state changes
  uint32_t uptime_ms;        ///< Driver uptime in milliseconds

  /**
   * @brief Default constructor
   *
   * Initializes all statistics to zero.
   */
  DriverStatistics()
      : total_transfers(0), failed_transfers(0), fault_events(0), state_changes(0), uptime_ms(0) {}

  /**
   * @brief Get transfer success rate
   * @return Success rate as a percentage (0.0 to 100.0)
   */
  float getSuccessRate() const {
    if (total_transfers == 0)
      return 100.0f;
    return ((float)(total_transfers - failed_transfers) / total_transfers) * 100.0f;
  }
};

} // namespace MAX22200

#endif // MAX22200_TYPES_H

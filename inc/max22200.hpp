/**
 * @file max22200.hpp
 * @brief Main driver class for MAX22200 octal solenoid and motor driver
 * @copyright Copyright (c) 2024-2025 HardFOC. All rights reserved.
 */
#pragma once
#include "max22200_registers.hpp"
#include "max22200_types.hpp"
#include "max22200_spi_interface.hpp"
#include <array>
#include <cstdint>
#include <functional>

namespace max22200 {

/**
 * @brief Main driver class for MAX22200 IC
 *
 * This class provides a comprehensive interface for controlling the MAX22200
 * octal solenoid and motor driver. It supports all features of the IC including
 * current regulation, voltage regulation, integrated current sensing, and
 * comprehensive fault detection.
 *
 * @tparam SpiType The SPI interface implementation type that inherits from
 * max22200::SpiInterface<SpiType>
 *
 * @note This class is designed for embedded systems and does not use
 * exceptions. All error conditions are reported through return values.
 *
 * @note The driver uses CRTP-based SPI interface for zero virtual call
 * overhead.
 *
 * @example Basic usage:
 * @code
 * // Create SPI interface implementation
 * class MySPI : public max22200::SpiInterface<MySPI> { ... };
 * MySPI spi;
 *
 * // Create MAX22200 driver
 * MAX22200<MySPI> driver(spi);
 *
 * // Initialize the driver
 * if (driver.Initialize() == DriverStatus::OK) {
 *     // Configure channel 0
 *     ChannelConfig config;
 *     config.enabled = true;
 *     config.drive_mode = DriveMode::CDR;
 *     config.hit_current = 500;
 *     config.hold_current = 200;
 *     config.hit_time = 1000;
 *
 *     driver.ConfigureChannel(0, config);
 *     driver.EnableChannel(0, true);
 * }
 * @endcode
 */
template <typename SpiType> class MAX22200 {
public:
  /**
   * @brief Constructor
   *
   * @param spi_interface Reference to SPI interface implementation (must
   * inherit from max22200::SpiInterface<SpiType>). Must outlive this driver:
   * destroy the MAX22200 before the SPI bus so the destructor can deassert
   * the ENABLE pin.
   * @param enable_diagnostics Enable diagnostic features (default: true)
   */
  explicit MAX22200(SpiType &spi_interface, bool enable_diagnostics = true);

  /**
   * @brief Destructor
   *
   * Calls Deinitialize() if initialized (disables channels, sleep, then
   * deasserts ENABLE). The SPI interface must still be valid during
   * destruction so ENABLE can be driven low.
   */
  ~MAX22200();

  // Disable copy constructor and assignment operator
  MAX22200(const MAX22200 &) = delete;
  MAX22200 &operator=(const MAX22200 &) = delete;

  // Enable move constructor and assignment operator
  MAX22200(MAX22200 &&) = default;
  MAX22200 &
  operator=(MAX22200 &&) = delete; // Can't move due to reference member

  /**
   * @brief Initialize the MAX22200 driver
   *
   * This method initializes the SPI interface and configures the MAX22200
   * with default settings. It should be called before using any other
   * driver functions.
   *
   * @return DriverStatus indicating success or failure
   */
  DriverStatus Initialize();

  /**
   * @brief Deinitialize the MAX22200 driver
   *
   * Safely shuts down the driver, disables all channels, and puts the
   * device into sleep mode.
   *
   * @return DriverStatus indicating success or failure
   */
  DriverStatus Deinitialize();

  /**
   * @brief Reset the MAX22200 device
   *
   * Performs a software reset of the MAX22200 device, clearing all
   * configuration and returning to default state.
   *
   * @return DriverStatus indicating success or failure
   */
  DriverStatus Reset();

  // Global Configuration Methods

  /**
   * @brief Configure global settings
   *
   * @param config Global configuration structure
   * @return DriverStatus indicating success or failure
   */
  DriverStatus ConfigureGlobal(const GlobalConfig &config);

  /**
   * @brief Get current global configuration
   *
   * @param config Reference to store the current configuration
   * @return DriverStatus indicating success or failure
   */
  DriverStatus GetGlobalConfig(GlobalConfig &config) const;

  /**
   * @brief Enable or disable sleep mode
   *
   * @param enable true to enable sleep mode, false to disable
   * @return DriverStatus indicating success or failure
   */
  DriverStatus SetSleepMode(bool enable);

  /**
   * @brief Enable or disable diagnostic features
   *
   * @param enable true to enable diagnostics, false to disable
   * @return DriverStatus indicating success or failure
   */
  DriverStatus SetDiagnosticMode(bool enable);

  /**
   * @brief Enable or disable integrated current sensing
   *
   * @param enable true to enable ICS, false to disable
   * @return DriverStatus indicating success or failure
   */
  DriverStatus SetIntegratedCurrentSensing(bool enable);

  // Channel Configuration Methods

  /**
   * @brief Configure a specific channel
   *
   * @param channel Channel number (0-7)
   * @param config Channel configuration structure
   * @return DriverStatus indicating success or failure
   */
  DriverStatus ConfigureChannel(uint8_t channel, const ChannelConfig &config);

  /**
   * @brief Get configuration of a specific channel
   *
   * @param channel Channel number (0-7)
   * @param config Reference to store the channel configuration
   * @return DriverStatus indicating success or failure
   */
  DriverStatus GetChannelConfig(uint8_t channel, ChannelConfig &config) const;

  /**
   * @brief Read a 16-bit register (for debug/diagnostics)
   *
   * @param reg Register address (0x00-0xFF)
   * @param value Reference to store the read value
   * @return DriverStatus indicating success or failure
   */
  DriverStatus ReadRegister(uint8_t reg, uint16_t &value) const;

  /**
   * @brief Configure all channels at once
   *
   * @param configs Array of channel configurations
   * @return DriverStatus indicating success or failure
   */
  DriverStatus ConfigureAllChannels(const ChannelConfigArray &configs);

  /**
   * @brief Get configuration of all channels
   *
   * @param configs Reference to store the channel configurations
   * @return DriverStatus indicating success or failure
   */
  DriverStatus GetAllChannelConfigs(ChannelConfigArray &configs) const;

  // Device and Channel Control Methods

  /**
   * @brief Assert or deassert the device ENABLE pin (hardware enable).
   *
   * Gives the user direct control of the ENABLE pin. When false, the device
   * is disabled (outputs off); when true, the device is enabled. The driver
   * also manages ENABLE automatically in Initialize() (assert) and
   * Deinitialize() / destructor (deassert).
   *
   * @param enable true to assert ENABLE (device on), false to deassert (device off)
   * @return DriverStatus::OK on success, INITIALIZATION_ERROR if not initialized
   */
  DriverStatus SetDeviceEnable(bool enable);

  /**
   * @brief Read the current state of the device ENABLE pin.
   *
   * @param[out] enable true if ENABLE is asserted (device on), false otherwise
   * @return DriverStatus::OK on success, INITIALIZATION_ERROR if not initialized
   */
  DriverStatus GetDeviceEnable(bool &enable) const;

  /**
   * @brief Enable or disable a specific channel
   *
   * @param channel Channel number (0-7)
   * @param enable true to enable, false to disable
   * @return DriverStatus indicating success or failure
   */
  DriverStatus EnableChannel(uint8_t channel, bool enable);

  /**
   * @brief Enable or disable all channels
   *
   * @param enable true to enable all, false to disable all
   * @return DriverStatus indicating success or failure
   */
  DriverStatus EnableAllChannels(bool enable);

  /**
   * @brief Set channel drive mode
   *
   * @param channel Channel number (0-7)
   * @param mode Drive mode (CDR or VDR)
   * @return DriverStatus indicating success or failure
   */
  DriverStatus SetChannelDriveMode(uint8_t channel, DriveMode mode);

  /**
   * @brief Set channel bridge mode
   *
   * @param channel Channel number (0-7)
   * @param mode Bridge mode (half or full)
   * @return DriverStatus indicating success or failure
   */
  DriverStatus SetChannelBridgeMode(uint8_t channel, BridgeMode mode);

  /**
   * @brief Set channel output polarity
   *
   * @param channel Channel number (0-7)
   * @param polarity Output polarity (normal or inverted)
   * @return DriverStatus indicating success or failure
   */
  DriverStatus SetChannelPolarity(uint8_t channel, OutputPolarity polarity);

  // Current Control Methods

  /**
   * @brief Set HIT current for a channel
   *
   * @param channel Channel number (0-7)
   * @param current HIT current value (0-1023)
   * @return DriverStatus indicating success or failure
   */
  DriverStatus SetHitCurrent(uint8_t channel, uint16_t current);

  /**
   * @brief Set HOLD current for a channel
   *
   * @param channel Channel number (0-7)
   * @param current HOLD current value (0-1023)
   * @return DriverStatus indicating success or failure
   */
  DriverStatus SetHoldCurrent(uint8_t channel, uint16_t current);

  /**
   * @brief Set both HIT and HOLD currents for a channel
   *
   * @param channel Channel number (0-7)
   * @param hit_current HIT current value (0-1023)
   * @param hold_current HOLD current value (0-1023)
   * @return DriverStatus indicating success or failure
   */
  DriverStatus SetCurrents(uint8_t channel, uint16_t hit_current,
                           uint16_t hold_current);

  /**
   * @brief Get current settings for a channel
   *
   * @param channel Channel number (0-7)
   * @param hit_current Reference to store HIT current
   * @param hold_current Reference to store HOLD current
   * @return DriverStatus indicating success or failure
   */
  DriverStatus GetCurrents(uint8_t channel, uint16_t &hit_current,
                           uint16_t &hold_current) const;

  // Timing Control Methods

  /**
   * @brief Set HIT time for a channel
   *
   * @param channel Channel number (0-7)
   * @param time HIT time value (0-65535)
   * @return DriverStatus indicating success or failure
   */
  DriverStatus SetHitTime(uint8_t channel, uint16_t time);

  /**
   * @brief Get HIT time for a channel
   *
   * @param channel Channel number (0-7)
   * @param time Reference to store HIT time
   * @return DriverStatus indicating success or failure
   */
  DriverStatus GetHitTime(uint8_t channel, uint16_t &time) const;

  // Status and Diagnostic Methods

  /**
   * @brief Read fault status
   *
   * @param status Reference to store fault status
   * @return DriverStatus indicating success or failure
   */
  DriverStatus ReadFaultStatus(FaultStatus &status) const;

  /**
   * @brief Clear fault status
   *
   * Clears all fault flags in the device.
   *
   * @return DriverStatus indicating success or failure
   */
  DriverStatus ClearFaultStatus();

  /**
   * @brief Read channel status
   *
   * @param channel Channel number (0-7)
   * @param status Reference to store channel status
   * @return DriverStatus indicating success or failure
   */
  DriverStatus ReadChannelStatus(uint8_t channel, ChannelStatus &status) const;

  /**
   * @brief Read all channel statuses
   *
   * @param statuses Reference to store channel statuses
   * @return DriverStatus indicating success or failure
   */
  DriverStatus ReadAllChannelStatuses(ChannelStatusArray &statuses) const;

  /**
   * @brief Get driver statistics
   *
   * @param stats Reference to store driver statistics
   * @return DriverStatus indicating success or failure
   */
  DriverStatus GetStatistics(DriverStatistics &stats) const;

  /**
   * @brief Reset driver statistics
   *
   * Resets all driver statistics to zero.
   *
   * @return DriverStatus indicating success or failure
   */
  DriverStatus ResetStatistics();

  // Callback Methods

  /**
   * @brief Set fault callback function
   *
   * @param callback Function to call when a fault occurs
   * @param user_data User data to pass to callback
   */
  void SetFaultCallback(FaultCallback callback, void *user_data = nullptr);

  /**
   * @brief Set state change callback function
   *
   * @param callback Function to call when channel state changes
   * @param user_data User data to pass to callback
   */
  void SetStateChangeCallback(StateChangeCallback callback,
                              void *user_data = nullptr);

  // Utility Methods

  /**
   * @brief Check if driver is initialized
   *
   * @return true if driver is initialized, false otherwise
   */
  bool IsInitialized() const;

  /**
   * @brief Check if a channel is valid
   *
   * @param channel Channel number to check
   * @return true if channel is valid (0-7), false otherwise
   */
  static constexpr bool IsValidChannel(uint8_t channel) {
    return channel < NUM_CHANNELS_;
  }

  /**
   * @brief Get driver version string
   *
   * @return Version string
   */
  static constexpr const char *GetVersion() { return "1.0.0"; }

private:
  // Private member variables
  SpiType &spi_interface_;              ///< Reference to SPI interface
  bool initialized_;                    ///< Initialization state
  bool diagnostics_enabled_;            ///< Diagnostic mode state
  mutable DriverStatistics statistics_; ///< Driver statistics

  // Callback functions
  FaultCallback fault_callback_;       ///< Fault callback function
  void *fault_user_data_;              ///< User data for fault callback
  StateChangeCallback state_callback_; ///< State change callback function
  void *state_user_data_;              ///< User data for state callback

  // Private methods
  DriverStatus writeRegister(uint8_t reg, uint16_t value) const;
  DriverStatus readRegister(uint8_t reg, uint16_t &value) const;
  DriverStatus writeRegisterArray(uint8_t reg, const uint8_t *data,
                                  size_t length) const;
  DriverStatus readRegisterArray(uint8_t reg, uint8_t *data,
                                 size_t length) const;

  DriverStatus updateChannelEnableRegister() const;
  DriverStatus updateGlobalConfigRegister() const;

  void updateStatistics(bool success) const;
  void triggerFaultCallback(uint8_t channel, FaultType fault_type) const;
  void triggerStateChangeCallback(uint8_t channel, ChannelState old_state,
                                  ChannelState new_state) const;

  // Helper methods for register manipulation
  uint16_t buildChannelConfigValue(const ChannelConfig &config) const;
  ChannelConfig parseChannelConfigValue(uint16_t value) const;
  uint16_t buildGlobalConfigValue(const GlobalConfig &config) const;
  GlobalConfig parseGlobalConfigValue(uint16_t value) const;
  FaultStatus parseFaultStatusValue(uint16_t value) const;
};

// Include template implementation
#define MAX22200_HEADER_INCLUDED
// NOLINTNEXTLINE(bugprone-suspicious-include) - Intentional: template implementation file
#include "../src/max22200.ipp"
#undef MAX22200_HEADER_INCLUDED

} // namespace max22200

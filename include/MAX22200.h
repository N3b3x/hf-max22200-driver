/**
 * @file MAX22200.h
 * @brief Main driver class for MAX22200 octal solenoid and motor driver
 * @author MAX22200 Driver Library
 * @date 2024
 * 
 * This file contains the main MAX22200 driver class that provides
 * a comprehensive interface for controlling the MAX22200 IC.
 */

#ifndef MAX22200_H
#define MAX22200_H

#include "SPIInterface.h"
#include "MAX22200_Registers.h"
#include "MAX22200_Types.h"
#include <cstdint>
#include <array>
#include <functional>

namespace MAX22200 {

/**
 * @brief Main driver class for MAX22200 IC
 * 
 * This class provides a comprehensive interface for controlling the MAX22200
 * octal solenoid and motor driver. It supports all features of the IC including
 * current regulation, voltage regulation, integrated current sensing, and
 * comprehensive fault detection.
 * 
 * @note This class is designed for embedded systems and does not use exceptions.
 *       All error conditions are reported through return values.
 * 
 * @example Basic usage:
 * @code
 * // Create SPI interface implementation
 * MySPIImplementation spi;
 * 
 * // Create MAX22200 driver
 * MAX22200 driver(spi);
 * 
 * // Initialize the driver
 * if (driver.initialize() == DriverStatus::OK) {
 *     // Configure channel 0
 *     ChannelConfig config;
 *     config.enabled = true;
 *     config.drive_mode = DriveMode::CDR;
 *     config.hit_current = 500;
 *     config.hold_current = 200;
 *     config.hit_time = 1000;
 *     
 *     driver.configureChannel(0, config);
 *     driver.enableChannel(0, true);
 * }
 * @endcode
 */
class MAX22200 {
public:
    /**
     * @brief Constructor
     * 
     * @param spi_interface Reference to SPI interface implementation
     * @param enable_diagnostics Enable diagnostic features (default: true)
     */
    explicit MAX22200(SPIInterface& spi_interface, bool enable_diagnostics = true);
    
    /**
     * @brief Destructor
     * 
     * Safely shuts down the driver and disables all channels.
     */
    ~MAX22200();
    
    // Disable copy constructor and assignment operator
    MAX22200(const MAX22200&) = delete;
    MAX22200& operator=(const MAX22200&) = delete;
    
    // Enable move constructor and assignment operator
    MAX22200(MAX22200&&) = default;
    MAX22200& operator=(MAX22200&&) = delete; // Cannot move due to reference member
    
    /**
     * @brief Initialize the MAX22200 driver
     * 
     * This method initializes the SPI interface and configures the MAX22200
     * with default settings. It should be called before using any other
     * driver functions.
     * 
     * @return DriverStatus indicating success or failure
     */
    DriverStatus initialize();
    
    /**
     * @brief Deinitialize the MAX22200 driver
     * 
     * Safely shuts down the driver, disables all channels, and puts the
     * device into sleep mode.
     * 
     * @return DriverStatus indicating success or failure
     */
    DriverStatus deinitialize();
    
    /**
     * @brief Reset the MAX22200 device
     * 
     * Performs a software reset of the MAX22200 device, clearing all
     * configuration and returning to default state.
     * 
     * @return DriverStatus indicating success or failure
     */
    DriverStatus reset();
    
    // Global Configuration Methods
    
    /**
     * @brief Configure global settings
     * 
     * @param config Global configuration structure
     * @return DriverStatus indicating success or failure
     */
    DriverStatus configureGlobal(const GlobalConfig& config);
    
    /**
     * @brief Get current global configuration
     * 
     * @param config Reference to store the current configuration
     * @return DriverStatus indicating success or failure
     */
    DriverStatus getGlobalConfig(GlobalConfig& config) const;
    
    /**
     * @brief Enable or disable sleep mode
     * 
     * @param enable true to enable sleep mode, false to disable
     * @return DriverStatus indicating success or failure
     */
    DriverStatus setSleepMode(bool enable);
    
    /**
     * @brief Enable or disable diagnostic features
     * 
     * @param enable true to enable diagnostics, false to disable
     * @return DriverStatus indicating success or failure
     */
    DriverStatus setDiagnosticMode(bool enable);
    
    /**
     * @brief Enable or disable integrated current sensing
     * 
     * @param enable true to enable ICS, false to disable
     * @return DriverStatus indicating success or failure
     */
    DriverStatus setIntegratedCurrentSensing(bool enable);
    
    // Channel Configuration Methods
    
    /**
     * @brief Configure a specific channel
     * 
     * @param channel Channel number (0-7)
     * @param config Channel configuration structure
     * @return DriverStatus indicating success or failure
     */
    DriverStatus configureChannel(uint8_t channel, const ChannelConfig& config);
    
    /**
     * @brief Get configuration of a specific channel
     * 
     * @param channel Channel number (0-7)
     * @param config Reference to store the channel configuration
     * @return DriverStatus indicating success or failure
     */
    DriverStatus getChannelConfig(uint8_t channel, ChannelConfig& config) const;
    
    /**
     * @brief Configure all channels at once
     * 
     * @param configs Array of channel configurations
     * @return DriverStatus indicating success or failure
     */
    DriverStatus configureAllChannels(const ChannelConfigArray& configs);
    
    /**
     * @brief Get configuration of all channels
     * 
     * @param configs Reference to store the channel configurations
     * @return DriverStatus indicating success or failure
     */
    DriverStatus getAllChannelConfigs(ChannelConfigArray& configs) const;
    
    // Channel Control Methods
    
    /**
     * @brief Enable or disable a specific channel
     * 
     * @param channel Channel number (0-7)
     * @param enable true to enable, false to disable
     * @return DriverStatus indicating success or failure
     */
    DriverStatus enableChannel(uint8_t channel, bool enable);
    
    /**
     * @brief Enable or disable all channels
     * 
     * @param enable true to enable all, false to disable all
     * @return DriverStatus indicating success or failure
     */
    DriverStatus enableAllChannels(bool enable);
    
    /**
     * @brief Set channel drive mode
     * 
     * @param channel Channel number (0-7)
     * @param mode Drive mode (CDR or VDR)
     * @return DriverStatus indicating success or failure
     */
    DriverStatus setChannelDriveMode(uint8_t channel, DriveMode mode);
    
    /**
     * @brief Set channel bridge mode
     * 
     * @param channel Channel number (0-7)
     * @param mode Bridge mode (half or full)
     * @return DriverStatus indicating success or failure
     */
    DriverStatus setChannelBridgeMode(uint8_t channel, BridgeMode mode);
    
    /**
     * @brief Set channel output polarity
     * 
     * @param channel Channel number (0-7)
     * @param polarity Output polarity (normal or inverted)
     * @return DriverStatus indicating success or failure
     */
    DriverStatus setChannelPolarity(uint8_t channel, OutputPolarity polarity);
    
    // Current Control Methods
    
    /**
     * @brief Set HIT current for a channel
     * 
     * @param channel Channel number (0-7)
     * @param current HIT current value (0-1023)
     * @return DriverStatus indicating success or failure
     */
    DriverStatus setHitCurrent(uint8_t channel, uint16_t current);
    
    /**
     * @brief Set HOLD current for a channel
     * 
     * @param channel Channel number (0-7)
     * @param current HOLD current value (0-1023)
     * @return DriverStatus indicating success or failure
     */
    DriverStatus setHoldCurrent(uint8_t channel, uint16_t current);
    
    /**
     * @brief Set both HIT and HOLD currents for a channel
     * 
     * @param channel Channel number (0-7)
     * @param hit_current HIT current value (0-1023)
     * @param hold_current HOLD current value (0-1023)
     * @return DriverStatus indicating success or failure
     */
    DriverStatus setCurrents(uint8_t channel, uint16_t hit_current, uint16_t hold_current);
    
    /**
     * @brief Get current settings for a channel
     * 
     * @param channel Channel number (0-7)
     * @param hit_current Reference to store HIT current
     * @param hold_current Reference to store HOLD current
     * @return DriverStatus indicating success or failure
     */
    DriverStatus getCurrents(uint8_t channel, uint16_t& hit_current, uint16_t& hold_current) const;
    
    // Timing Control Methods
    
    /**
     * @brief Set HIT time for a channel
     * 
     * @param channel Channel number (0-7)
     * @param time HIT time value (0-65535)
     * @return DriverStatus indicating success or failure
     */
    DriverStatus setHitTime(uint8_t channel, uint16_t time);
    
    /**
     * @brief Get HIT time for a channel
     * 
     * @param channel Channel number (0-7)
     * @param time Reference to store HIT time
     * @return DriverStatus indicating success or failure
     */
    DriverStatus getHitTime(uint8_t channel, uint16_t& time) const;
    
    // Status and Diagnostic Methods
    
    /**
     * @brief Read fault status
     * 
     * @param status Reference to store fault status
     * @return DriverStatus indicating success or failure
     */
    DriverStatus readFaultStatus(FaultStatus& status) const;
    
    /**
     * @brief Clear fault status
     * 
     * Clears all fault flags in the device.
     * 
     * @return DriverStatus indicating success or failure
     */
    DriverStatus clearFaultStatus();
    
    /**
     * @brief Read channel status
     * 
     * @param channel Channel number (0-7)
     * @param status Reference to store channel status
     * @return DriverStatus indicating success or failure
     */
    DriverStatus readChannelStatus(uint8_t channel, ChannelStatus& status) const;
    
    /**
     * @brief Read all channel statuses
     * 
     * @param statuses Reference to store channel statuses
     * @return DriverStatus indicating success or failure
     */
    DriverStatus readAllChannelStatuses(ChannelStatusArray& statuses) const;
    
    /**
     * @brief Get driver statistics
     * 
     * @param stats Reference to store driver statistics
     * @return DriverStatus indicating success or failure
     */
    DriverStatus getStatistics(DriverStatistics& stats) const;
    
    /**
     * @brief Reset driver statistics
     * 
     * Resets all driver statistics to zero.
     * 
     * @return DriverStatus indicating success or failure
     */
    DriverStatus resetStatistics();
    
    // Callback Methods
    
    /**
     * @brief Set fault callback function
     * 
     * @param callback Function to call when a fault occurs
     * @param user_data User data to pass to callback
     */
    void setFaultCallback(FaultCallback callback, void* user_data = nullptr);
    
    /**
     * @brief Set state change callback function
     * 
     * @param callback Function to call when channel state changes
     * @param user_data User data to pass to callback
     */
    void setStateChangeCallback(StateChangeCallback callback, void* user_data = nullptr);
    
    // Utility Methods
    
    /**
     * @brief Check if driver is initialized
     * 
     * @return true if driver is initialized, false otherwise
     */
    bool isInitialized() const;
    
    /**
     * @brief Check if a channel is valid
     * 
     * @param channel Channel number to check
     * @return true if channel is valid (0-7), false otherwise
     */
    static constexpr bool isValidChannel(uint8_t channel) {
        return channel < NUM_CHANNELS;
    }
    
    /**
     * @brief Get driver version string
     * 
     * @return Version string
     */
    static constexpr const char* getVersion() {
        return "1.0.0";
    }

private:
    // Private member variables
    SPIInterface& spi_interface_;           ///< Reference to SPI interface
    bool initialized_;                      ///< Initialization state
    bool diagnostics_enabled_;              ///< Diagnostic mode state
    mutable DriverStatistics statistics_;   ///< Driver statistics
    
    // Callback functions
    FaultCallback fault_callback_;          ///< Fault callback function
    void* fault_user_data_;                 ///< User data for fault callback
    StateChangeCallback state_callback_;    ///< State change callback function
    void* state_user_data_;                 ///< User data for state callback
    
    // Private methods
    DriverStatus writeRegister(uint8_t reg, uint16_t value) const;
    DriverStatus readRegister(uint8_t reg, uint16_t& value) const;
    DriverStatus writeRegisterArray(uint8_t reg, const uint8_t* data, size_t length) const;
    DriverStatus readRegisterArray(uint8_t reg, uint8_t* data, size_t length) const;
    
    DriverStatus updateChannelEnableRegister() const;
    DriverStatus updateGlobalConfigRegister() const;
    
    void updateStatistics(bool success) const;
    void triggerFaultCallback(uint8_t channel, FaultType fault_type) const;
    void triggerStateChangeCallback(uint8_t channel, ChannelState old_state, 
                                   ChannelState new_state) const;
    
    // Helper methods for register manipulation
    uint16_t buildChannelConfigValue(const ChannelConfig& config) const;
    ChannelConfig parseChannelConfigValue(uint16_t value) const;
    uint16_t buildGlobalConfigValue(const GlobalConfig& config) const;
    GlobalConfig parseGlobalConfigValue(uint16_t value) const;
    FaultStatus parseFaultStatusValue(uint16_t value) const;
};

} // namespace MAX22200

#endif // MAX22200_H

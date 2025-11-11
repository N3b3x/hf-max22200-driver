/**
 * @file MAX22200.cpp
 * @brief Implementation of MAX22200 driver class
 * @author MAX22200 Driver Library
 * @date 2024
 * 
 * This file contains the implementation of the MAX22200 driver class,
 * providing comprehensive control over the MAX22200 octal solenoid
 * and motor driver IC.
 */

#include "../inc/MAX22200.h"
#include <cstring>

namespace MAX22200 {

// Constructor
MAX22200::MAX22200(SPIInterface& spi_interface, bool enable_diagnostics)
    : spi_interface_(spi_interface)
    , initialized_(false)
    , diagnostics_enabled_(enable_diagnostics)
    , fault_callback_(nullptr)
    , fault_user_data_(nullptr)
    , state_callback_(nullptr)
    , state_user_data_(nullptr)
{
    // Initialize statistics
    statistics_ = DriverStatistics();
}

// Destructor
MAX22200::~MAX22200() {
    if (initialized_) {
        deinitialize();
    }
}

// Initialize the MAX22200 driver
DriverStatus MAX22200::initialize() {
    // Check if already initialized
    if (initialized_) {
        return DriverStatus::OK;
    }
    
    // Initialize SPI interface
    if (!spi_interface_.initialize()) {
        updateStatistics(false);
        return DriverStatus::INITIALIZATION_ERROR;
    }
    
    // Configure SPI for MAX22200
    if (!spi_interface_.configure(MAX_SPI_FREQ_STANDALONE, 0, true)) {
        updateStatistics(false);
        return DriverStatus::INITIALIZATION_ERROR;
    }
    
    // Reset the device
    DriverStatus status = reset();
    if (status != DriverStatus::OK) {
        updateStatistics(false);
        return status;
    }
    
    // Configure global settings
    GlobalConfig global_config;
    global_config.diagnostic_enable = diagnostics_enabled_;
    global_config.ics_enable = true;
    global_config.daisy_chain_mode = false;
    global_config.sleep_mode = false;
    global_config.reset = false;
    
    status = configureGlobal(global_config);
    if (status != DriverStatus::OK) {
        updateStatistics(false);
        return status;
    }
    
    // Clear any existing faults
    clearFaultStatus();
    
    initialized_ = true;
    updateStatistics(true);
    return DriverStatus::OK;
}

// Deinitialize the MAX22200 driver
DriverStatus MAX22200::deinitialize() {
    if (!initialized_) {
        return DriverStatus::OK;
    }
    
    // Disable all channels
    enableAllChannels(false);
    
    // Put device into sleep mode
    setSleepMode(true);
    
    initialized_ = false;
    updateStatistics(true);
    return DriverStatus::OK;
}

// Reset the MAX22200 device
DriverStatus MAX22200::reset() {
    // Set reset bit
    DriverStatus status = writeRegister(Registers::GLOBAL_CONFIG, 
                                       GlobalConfigBits::RESET_MASK);
    if (status != DriverStatus::OK) {
        updateStatistics(false);
        return status;
    }
    
    // Clear reset bit
    status = writeRegister(Registers::GLOBAL_CONFIG, 0x0000);
    if (status != DriverStatus::OK) {
        updateStatistics(false);
        return status;
    }
    
    // Wait for device to stabilize (implementation dependent)
    // This would typically involve a delay function
    
    updateStatistics(true);
    return DriverStatus::OK;
}

// Configure global settings
DriverStatus MAX22200::configureGlobal(const GlobalConfig& config) {
    uint16_t reg_value = buildGlobalConfigValue(config);
    DriverStatus status = writeRegister(Registers::GLOBAL_CONFIG, reg_value);
    updateStatistics(status == DriverStatus::OK);
    return status;
}

// Get current global configuration
DriverStatus MAX22200::getGlobalConfig(GlobalConfig& config) const {
    uint16_t reg_value;
    DriverStatus status = readRegister(Registers::GLOBAL_CONFIG, reg_value);
    if (status == DriverStatus::OK) {
        config = parseGlobalConfigValue(reg_value);
    }
    updateStatistics(status == DriverStatus::OK);
    return status;
}

// Enable or disable sleep mode
DriverStatus MAX22200::setSleepMode(bool enable) {
    uint16_t current_value;
    DriverStatus status = readRegister(Registers::GLOBAL_CONFIG, current_value);
    if (status != DriverStatus::OK) {
        updateStatistics(false);
        return status;
    }
    
    if (enable) {
        current_value |= GlobalConfigBits::SLEEP_MASK;
    } else {
        current_value &= ~GlobalConfigBits::SLEEP_MASK;
    }
    
    status = writeRegister(Registers::GLOBAL_CONFIG, current_value);
    updateStatistics(status == DriverStatus::OK);
    return status;
}

// Enable or disable diagnostic features
DriverStatus MAX22200::setDiagnosticMode(bool enable) {
    uint16_t current_value;
    DriverStatus status = readRegister(Registers::GLOBAL_CONFIG, current_value);
    if (status != DriverStatus::OK) {
        updateStatistics(false);
        return status;
    }
    
    if (enable) {
        current_value |= GlobalConfigBits::DIAG_EN_MASK;
    } else {
        current_value &= ~GlobalConfigBits::DIAG_EN_MASK;
    }
    
    status = writeRegister(Registers::GLOBAL_CONFIG, current_value);
    diagnostics_enabled_ = enable;
    updateStatistics(status == DriverStatus::OK);
    return status;
}

// Enable or disable integrated current sensing
DriverStatus MAX22200::setIntegratedCurrentSensing(bool enable) {
    uint16_t current_value;
    DriverStatus status = readRegister(Registers::GLOBAL_CONFIG, current_value);
    if (status != DriverStatus::OK) {
        updateStatistics(false);
        return status;
    }
    
    if (enable) {
        current_value |= GlobalConfigBits::ICS_EN_MASK;
    } else {
        current_value &= ~GlobalConfigBits::ICS_EN_MASK;
    }
    
    status = writeRegister(Registers::GLOBAL_CONFIG, current_value);
    updateStatistics(status == DriverStatus::OK);
    return status;
}

// Configure a specific channel
DriverStatus MAX22200::configureChannel(uint8_t channel, const ChannelConfig& config) {
    if (!isValidChannel(channel)) {
        updateStatistics(false);
        return DriverStatus::INVALID_PARAMETER;
    }
    
    // Validate current values
    if (config.hit_current > CurrentRange::MAX_HIT_CURRENT ||
        config.hold_current > CurrentRange::MAX_HOLD_CURRENT ||
        config.hit_time > TimingRange::MAX_HIT_TIME) {
        updateStatistics(false);
        return DriverStatus::INVALID_PARAMETER;
    }
    
    // Write channel configuration
    uint16_t config_value = buildChannelConfigValue(config);
    DriverStatus status = writeRegister(getChannelConfigReg(channel), config_value);
    if (status != DriverStatus::OK) {
        updateStatistics(false);
        return status;
    }
    
    // Write current settings
    uint16_t current_value = (config.hit_current << 8) | config.hold_current;
    status = writeRegister(getChannelCurrentReg(channel), current_value);
    if (status != DriverStatus::OK) {
        updateStatistics(false);
        return status;
    }
    
    // Write timing settings
    status = writeRegister(getChannelTimingReg(channel), config.hit_time);
    if (status != DriverStatus::OK) {
        updateStatistics(false);
        return status;
    }
    
    // Update channel enable register
    status = updateChannelEnableRegister();
    updateStatistics(status == DriverStatus::OK);
    return status;
}

// Get configuration of a specific channel
DriverStatus MAX22200::getChannelConfig(uint8_t channel, ChannelConfig& config) const {
    if (!isValidChannel(channel)) {
        updateStatistics(false);
        return DriverStatus::INVALID_PARAMETER;
    }
    
    // Read channel configuration
    uint16_t config_value;
    DriverStatus status = readRegister(getChannelConfigReg(channel), config_value);
    if (status != DriverStatus::OK) {
        updateStatistics(false);
        return status;
    }
    
    config = parseChannelConfigValue(config_value);
    
    // Read current settings
    uint16_t current_value;
    status = readRegister(getChannelCurrentReg(channel), current_value);
    if (status == DriverStatus::OK) {
        config.hit_current = (current_value >> 8) & 0x03FF;
        config.hold_current = current_value & 0x03FF;
    }
    
    // Read timing settings
    uint16_t timing_value;
    status = readRegister(getChannelTimingReg(channel), timing_value);
    if (status == DriverStatus::OK) {
        config.hit_time = timing_value;
    }
    
    // Read enable state from channel enable register
    uint16_t enable_value;
    status = readRegister(Registers::CHANNEL_ENABLE, enable_value);
    if (status == DriverStatus::OK) {
        config.enabled = (enable_value & (1 << channel)) != 0;
    }
    
    updateStatistics(status == DriverStatus::OK);
    return status;
}

// Configure all channels at once
DriverStatus MAX22200::configureAllChannels(const ChannelConfigArray& configs) {
    DriverStatus status = DriverStatus::OK;
    
    for (uint8_t channel = 0; channel < NUM_CHANNELS; ++channel) {
        DriverStatus channel_status = configureChannel(channel, configs[channel]);
        if (channel_status != DriverStatus::OK) {
            status = channel_status;
        }
    }
    
    updateStatistics(status == DriverStatus::OK);
    return status;
}

// Get configuration of all channels
DriverStatus MAX22200::getAllChannelConfigs(ChannelConfigArray& configs) const {
    DriverStatus status = DriverStatus::OK;
    
    for (uint8_t channel = 0; channel < NUM_CHANNELS; ++channel) {
        DriverStatus channel_status = getChannelConfig(channel, configs[channel]);
        if (channel_status != DriverStatus::OK) {
            status = channel_status;
        }
    }
    
    updateStatistics(status == DriverStatus::OK);
    return status;
}

// Enable or disable a specific channel
DriverStatus MAX22200::enableChannel(uint8_t channel, bool enable) {
    if (!isValidChannel(channel)) {
        updateStatistics(false);
        return DriverStatus::INVALID_PARAMETER;
    }
    
    uint16_t current_value;
    DriverStatus status = readRegister(Registers::CHANNEL_ENABLE, current_value);
    if (status != DriverStatus::OK) {
        updateStatistics(false);
        return status;
    }
    
    if (enable) {
        current_value |= (1 << channel);
    } else {
        current_value &= ~(1 << channel);
    }
    
    status = writeRegister(Registers::CHANNEL_ENABLE, current_value);
    updateStatistics(status == DriverStatus::OK);
    return status;
}

// Enable or disable all channels
DriverStatus MAX22200::enableAllChannels(bool enable) {
    uint16_t value = enable ? 0x00FF : 0x0000;
    DriverStatus status = writeRegister(Registers::CHANNEL_ENABLE, value);
    updateStatistics(status == DriverStatus::OK);
    return status;
}

// Set channel drive mode
DriverStatus MAX22200::setChannelDriveMode(uint8_t channel, DriveMode mode) {
    if (!isValidChannel(channel)) {
        updateStatistics(false);
        return DriverStatus::INVALID_PARAMETER;
    }
    
    uint16_t current_value;
    DriverStatus status = readRegister(getChannelConfigReg(channel), current_value);
    if (status != DriverStatus::OK) {
        updateStatistics(false);
        return status;
    }
    
    if (mode == DriveMode::VDR) {
        current_value |= ChannelConfigBits::DRIVE_MODE_MASK;
    } else {
        current_value &= ~ChannelConfigBits::DRIVE_MODE_MASK;
    }
    
    status = writeRegister(getChannelConfigReg(channel), current_value);
    updateStatistics(status == DriverStatus::OK);
    return status;
}

// Set channel bridge mode
DriverStatus MAX22200::setChannelBridgeMode(uint8_t channel, BridgeMode mode) {
    if (!isValidChannel(channel)) {
        updateStatistics(false);
        return DriverStatus::INVALID_PARAMETER;
    }
    
    uint16_t current_value;
    DriverStatus status = readRegister(getChannelConfigReg(channel), current_value);
    if (status != DriverStatus::OK) {
        updateStatistics(false);
        return status;
    }
    
    if (mode == BridgeMode::FULL_BRIDGE) {
        current_value |= ChannelConfigBits::BRIDGE_MODE_MASK;
    } else {
        current_value &= ~ChannelConfigBits::BRIDGE_MODE_MASK;
    }
    
    status = writeRegister(getChannelConfigReg(channel), current_value);
    updateStatistics(status == DriverStatus::OK);
    return status;
}

// Set channel output polarity
DriverStatus MAX22200::setChannelPolarity(uint8_t channel, OutputPolarity polarity) {
    if (!isValidChannel(channel)) {
        updateStatistics(false);
        return DriverStatus::INVALID_PARAMETER;
    }
    
    uint16_t current_value;
    DriverStatus status = readRegister(getChannelConfigReg(channel), current_value);
    if (status != DriverStatus::OK) {
        updateStatistics(false);
        return status;
    }
    
    if (polarity == OutputPolarity::INVERTED) {
        current_value |= ChannelConfigBits::POLARITY_MASK;
    } else {
        current_value &= ~ChannelConfigBits::POLARITY_MASK;
    }
    
    status = writeRegister(getChannelConfigReg(channel), current_value);
    updateStatistics(status == DriverStatus::OK);
    return status;
}

// Set HIT current for a channel
DriverStatus MAX22200::setHitCurrent(uint8_t channel, uint16_t current) {
    if (!isValidChannel(channel) || current > CurrentRange::MAX_HIT_CURRENT) {
        updateStatistics(false);
        return DriverStatus::INVALID_PARAMETER;
    }
    
    uint16_t current_value;
    DriverStatus status = readRegister(getChannelCurrentReg(channel), current_value);
    if (status != DriverStatus::OK) {
        updateStatistics(false);
        return status;
    }
    
    current_value = (current << 8) | (current_value & 0x03FF);
    status = writeRegister(getChannelCurrentReg(channel), current_value);
    updateStatistics(status == DriverStatus::OK);
    return status;
}

// Set HOLD current for a channel
DriverStatus MAX22200::setHoldCurrent(uint8_t channel, uint16_t current) {
    if (!isValidChannel(channel) || current > CurrentRange::MAX_HOLD_CURRENT) {
        updateStatistics(false);
        return DriverStatus::INVALID_PARAMETER;
    }
    
    uint16_t current_value;
    DriverStatus status = readRegister(getChannelCurrentReg(channel), current_value);
    if (status != DriverStatus::OK) {
        updateStatistics(false);
        return status;
    }
    
    current_value = (current_value & 0xFC00) | current;
    status = writeRegister(getChannelCurrentReg(channel), current_value);
    updateStatistics(status == DriverStatus::OK);
    return status;
}

// Set both HIT and HOLD currents for a channel
DriverStatus MAX22200::setCurrents(uint8_t channel, uint16_t hit_current, uint16_t hold_current) {
    if (!isValidChannel(channel) || 
        hit_current > CurrentRange::MAX_HIT_CURRENT ||
        hold_current > CurrentRange::MAX_HOLD_CURRENT) {
        updateStatistics(false);
        return DriverStatus::INVALID_PARAMETER;
    }
    
    uint16_t current_value = (hit_current << 8) | hold_current;
    DriverStatus status = writeRegister(getChannelCurrentReg(channel), current_value);
    updateStatistics(status == DriverStatus::OK);
    return status;
}

// Get current settings for a channel
DriverStatus MAX22200::getCurrents(uint8_t channel, uint16_t& hit_current, uint16_t& hold_current) const {
    if (!isValidChannel(channel)) {
        updateStatistics(false);
        return DriverStatus::INVALID_PARAMETER;
    }
    
    uint16_t current_value;
    DriverStatus status = readRegister(getChannelCurrentReg(channel), current_value);
    if (status == DriverStatus::OK) {
        hit_current = (current_value >> 8) & 0x03FF;
        hold_current = current_value & 0x03FF;
    }
    updateStatistics(status == DriverStatus::OK);
    return status;
}

// Set HIT time for a channel
DriverStatus MAX22200::setHitTime(uint8_t channel, uint16_t time) {
    if (!isValidChannel(channel) || time > TimingRange::MAX_HIT_TIME) {
        updateStatistics(false);
        return DriverStatus::INVALID_PARAMETER;
    }
    
    DriverStatus status = writeRegister(getChannelTimingReg(channel), time);
    updateStatistics(status == DriverStatus::OK);
    return status;
}

// Get HIT time for a channel
DriverStatus MAX22200::getHitTime(uint8_t channel, uint16_t& time) const {
    if (!isValidChannel(channel)) {
        updateStatistics(false);
        return DriverStatus::INVALID_PARAMETER;
    }
    
    DriverStatus status = readRegister(getChannelTimingReg(channel), time);
    updateStatistics(status == DriverStatus::OK);
    return status;
}

// Read fault status
DriverStatus MAX22200::readFaultStatus(FaultStatus& status) const {
    uint16_t fault_value;
    DriverStatus result = readRegister(Registers::FAULT_STATUS, fault_value);
    if (result == DriverStatus::OK) {
        status = parseFaultStatusValue(fault_value);
    }
    updateStatistics(result == DriverStatus::OK);
    return result;
}

// Clear fault status
DriverStatus MAX22200::clearFaultStatus() {
    // Write 0x00FF to clear all fault flags
    DriverStatus status = writeRegister(Registers::FAULT_STATUS, 0x00FF);
    updateStatistics(status == DriverStatus::OK);
    return status;
}

// Read channel status
DriverStatus MAX22200::readChannelStatus(uint8_t channel, ChannelStatus& status) const {
    if (!isValidChannel(channel)) {
        updateStatistics(false);
        return DriverStatus::INVALID_PARAMETER;
    }
    
    // Read channel enable state
    uint16_t enable_value;
    DriverStatus result = readRegister(Registers::CHANNEL_ENABLE, enable_value);
    if (result != DriverStatus::OK) {
        updateStatistics(false);
        return result;
    }
    
    status.enabled = (enable_value & (1 << channel)) != 0;
    
    // Read fault status for this channel
    FaultStatus fault_status;
    result = readFaultStatus(fault_status);
    if (result == DriverStatus::OK) {
        // Check if this channel has a fault (simplified - would need channel-specific fault bits)
        status.fault_active = fault_status.hasFault();
    }
    
    // Read current from ICS (if enabled)
    if (diagnostics_enabled_) {
        // This would require reading from a diagnostic register
        // Implementation depends on specific register layout
        status.current_reading = 0; // Placeholder
    }
    
    updateStatistics(result == DriverStatus::OK);
    return result;
}

// Read all channel statuses
DriverStatus MAX22200::readAllChannelStatuses(ChannelStatusArray& statuses) const {
    DriverStatus status = DriverStatus::OK;
    
    for (uint8_t channel = 0; channel < NUM_CHANNELS; ++channel) {
        DriverStatus channel_status = readChannelStatus(channel, statuses[channel]);
        if (channel_status != DriverStatus::OK) {
            status = channel_status;
        }
    }
    
    updateStatistics(status == DriverStatus::OK);
    return status;
}

// Get driver statistics
DriverStatus MAX22200::getStatistics(DriverStatistics& stats) const {
    stats = statistics_;
    return DriverStatus::OK;
}

// Reset driver statistics
DriverStatus MAX22200::resetStatistics() {
    statistics_ = DriverStatistics();
    return DriverStatus::OK;
}

// Set fault callback function
void MAX22200::setFaultCallback(FaultCallback callback, void* user_data) {
    fault_callback_ = callback;
    fault_user_data_ = user_data;
}

// Set state change callback function
void MAX22200::setStateChangeCallback(StateChangeCallback callback, void* user_data) {
    state_callback_ = callback;
    state_user_data_ = user_data;
}

// Check if driver is initialized
bool MAX22200::isInitialized() const {
    return initialized_;
}

// Private method implementations

// Write register
DriverStatus MAX22200::writeRegister(uint8_t reg, uint16_t value) const {
    uint8_t tx_data[3] = {reg, static_cast<uint8_t>(value >> 8), static_cast<uint8_t>(value & 0xFF)};
    uint8_t rx_data[3];
    
    spi_interface_.setChipSelect(false);
    bool success = spi_interface_.transfer(tx_data, rx_data, 3);
    spi_interface_.setChipSelect(true);
    
    if (!success) {
        return DriverStatus::COMMUNICATION_ERROR;
    }
    
    return DriverStatus::OK;
}

// Read register
DriverStatus MAX22200::readRegister(uint8_t reg, uint16_t& value) const {
    uint8_t tx_data[3] = {static_cast<uint8_t>(reg | 0x80), 0x00, 0x00};
    uint8_t rx_data[3];
    
    spi_interface_.setChipSelect(false);
    bool success = spi_interface_.transfer(tx_data, rx_data, 3);
    spi_interface_.setChipSelect(true);
    
    if (!success) {
        return DriverStatus::COMMUNICATION_ERROR;
    }
    
    value = (static_cast<uint16_t>(rx_data[1]) << 8) | rx_data[2];
    return DriverStatus::OK;
}

// Write register array
DriverStatus MAX22200::writeRegisterArray(uint8_t reg, const uint8_t* data, size_t length) const {
    uint8_t* tx_data = new uint8_t[length + 1];
    uint8_t* rx_data = new uint8_t[length + 1];
    
    tx_data[0] = reg;
    std::memcpy(&tx_data[1], data, length);
    
    spi_interface_.setChipSelect(false);
    bool success = spi_interface_.transfer(tx_data, rx_data, length + 1);
    spi_interface_.setChipSelect(true);
    
    delete[] tx_data;
    delete[] rx_data;
    
    if (!success) {
        return DriverStatus::COMMUNICATION_ERROR;
    }
    
    return DriverStatus::OK;
}

// Read register array
DriverStatus MAX22200::readRegisterArray(uint8_t reg, uint8_t* data, size_t length) const {
    uint8_t* tx_data = new uint8_t[length + 1];
    uint8_t* rx_data = new uint8_t[length + 1];
    
    tx_data[0] = static_cast<uint8_t>(reg | 0x80);
    std::memset(&tx_data[1], 0x00, length);
    
    spi_interface_.setChipSelect(false);
    bool success = spi_interface_.transfer(tx_data, rx_data, length + 1);
    spi_interface_.setChipSelect(true);
    
    if (success) {
        std::memcpy(data, &rx_data[1], length);
    }
    
    delete[] tx_data;
    delete[] rx_data;
    
    if (!success) {
        return DriverStatus::COMMUNICATION_ERROR;
    }
    
    return DriverStatus::OK;
}

// Update channel enable register
DriverStatus MAX22200::updateChannelEnableRegister() const {
    // This would read all channel configurations and update the enable register
    // Implementation depends on how channel enable state is managed
    return DriverStatus::OK;
}

// Update global config register
DriverStatus MAX22200::updateGlobalConfigRegister() const {
    // This would update the global configuration register
    // Implementation depends on current global config state
    return DriverStatus::OK;
}

// Update statistics
void MAX22200::updateStatistics(bool success) const {
    statistics_.total_transfers++;
    if (!success) {
        statistics_.failed_transfers++;
    }
}

// Trigger fault callback
void MAX22200::triggerFaultCallback(uint8_t channel, FaultType fault_type) const {
    if (fault_callback_) {
        fault_callback_(channel, fault_type, fault_user_data_);
        statistics_.fault_events++;
    }
}

// Trigger state change callback
void MAX22200::triggerStateChangeCallback(uint8_t channel, ChannelState old_state, 
                                         ChannelState new_state) const {
    if (state_callback_) {
        state_callback_(channel, old_state, new_state, state_user_data_);
        statistics_.state_changes++;
    }
}

// Build channel config value
uint16_t MAX22200::buildChannelConfigValue(const ChannelConfig& config) const {
    uint16_t value = 0;
    
    if (config.drive_mode == DriveMode::VDR) {
        value |= ChannelConfigBits::DRIVE_MODE_MASK;
    }
    
    if (config.bridge_mode == BridgeMode::FULL_BRIDGE) {
        value |= ChannelConfigBits::BRIDGE_MODE_MASK;
    }
    
    if (config.parallel_mode) {
        value |= ChannelConfigBits::PARALLEL_MASK;
    }
    
    if (config.polarity == OutputPolarity::INVERTED) {
        value |= ChannelConfigBits::POLARITY_MASK;
    }
    
    return value;
}

// Parse channel config value
ChannelConfig MAX22200::parseChannelConfigValue(uint16_t value) const {
    ChannelConfig config;
    
    config.drive_mode = (value & ChannelConfigBits::DRIVE_MODE_MASK) ? 
                        DriveMode::VDR : DriveMode::CDR;
    
    config.bridge_mode = (value & ChannelConfigBits::BRIDGE_MODE_MASK) ? 
                         BridgeMode::FULL_BRIDGE : BridgeMode::HALF_BRIDGE;
    
    config.parallel_mode = (value & ChannelConfigBits::PARALLEL_MASK) != 0;
    
    config.polarity = (value & ChannelConfigBits::POLARITY_MASK) ? 
                      OutputPolarity::INVERTED : OutputPolarity::NORMAL;
    
    return config;
}

// Build global config value
uint16_t MAX22200::buildGlobalConfigValue(const GlobalConfig& config) const {
    uint16_t value = 0;
    
    if (config.reset) {
        value |= GlobalConfigBits::RESET_MASK;
    }
    
    if (config.sleep_mode) {
        value |= GlobalConfigBits::SLEEP_MASK;
    }
    
    if (config.diagnostic_enable) {
        value |= GlobalConfigBits::DIAG_EN_MASK;
    }
    
    if (config.ics_enable) {
        value |= GlobalConfigBits::ICS_EN_MASK;
    }
    
    if (config.daisy_chain_mode) {
        value |= GlobalConfigBits::DAISY_CHAIN_MASK;
    }
    
    return value;
}

// Parse global config value
GlobalConfig MAX22200::parseGlobalConfigValue(uint16_t value) const {
    GlobalConfig config;
    
    config.reset = (value & GlobalConfigBits::RESET_MASK) != 0;
    config.sleep_mode = (value & GlobalConfigBits::SLEEP_MASK) != 0;
    config.diagnostic_enable = (value & GlobalConfigBits::DIAG_EN_MASK) != 0;
    config.ics_enable = (value & GlobalConfigBits::ICS_EN_MASK) != 0;
    config.daisy_chain_mode = (value & GlobalConfigBits::DAISY_CHAIN_MASK) != 0;
    
    return config;
}

// Parse fault status value
FaultStatus MAX22200::parseFaultStatusValue(uint16_t value) const {
    FaultStatus status;
    
    status.overcurrent_protection = (value & FaultStatusBits::OCP_MASK) != 0;
    status.open_load = (value & FaultStatusBits::OL_MASK) != 0;
    status.plunger_movement = (value & FaultStatusBits::DPM_MASK) != 0;
    status.undervoltage_lockout = (value & FaultStatusBits::UVLO_MASK) != 0;
    status.hit_current_not_reached = (value & FaultStatusBits::HHF_MASK) != 0;
    status.thermal_shutdown = (value & FaultStatusBits::TSD_MASK) != 0;
    
    return status;
}

} // namespace MAX22200

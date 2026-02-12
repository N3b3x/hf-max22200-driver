/**
 * @file max22200.ipp
 * @brief Implementation of MAX22200 driver class
 * @copyright Copyright (c) 2024-2025 HardFOC. All rights reserved.
 */
#ifndef MAX22200_IMPL
#define MAX22200_IMPL

#include <string.h> // Use C string.h for ESP-IDF compatibility (cstring has issues with ESP-IDF toolchain)

// Note: This file is included inside namespace max22200 in MAX22200.h,
//       so we don't declare the namespace here and don't include MAX22200.h
//       (it's already included before this file is included)

// Constructor
template <typename SpiType>
MAX22200<SpiType>::MAX22200(SpiType &spi_interface, bool enable_diagnostics)
    : spi_interface_(spi_interface), initialized_(false),
      diagnostics_enabled_(enable_diagnostics), fault_callback_(nullptr),
      fault_user_data_(nullptr), state_callback_(nullptr),
      state_user_data_(nullptr) {
  // Initialize statistics
  statistics_ = DriverStatistics();
}

// Destructor
template <typename SpiType> MAX22200<SpiType>::~MAX22200() {
  if (initialized_) {
    Deinitialize();
  }
}

// Initialize the MAX22200 driver
template <typename SpiType> DriverStatus MAX22200<SpiType>::Initialize() {
  // Check if already initialized
  if (initialized_) {
    return DriverStatus::OK;
  }

  // Initialize SPI interface
  if (!spi_interface_.Initialize()) {
    updateStatistics(false);
    return DriverStatus::INITIALIZATION_ERROR;
  }

  // Configure SPI for MAX22200
  if (!spi_interface_.Configure(MAX_SPI_FREQ_STANDALONE_, 0, true)) {
    updateStatistics(false);
    return DriverStatus::INITIALIZATION_ERROR;
  }

  // Reset the device
  DriverStatus status = Reset();
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

  status = ConfigureGlobal(global_config);
  if (status != DriverStatus::OK) {
    updateStatistics(false);
    return status;
  }

  // Clear any existing faults
  ClearFaultStatus();

  initialized_ = true;
  updateStatistics(true);
  return DriverStatus::OK;
}

// Deinitialize the MAX22200 driver
template <typename SpiType> DriverStatus MAX22200<SpiType>::Deinitialize() {
  if (!initialized_) {
    return DriverStatus::OK;
  }

  // Disable all channels
  EnableAllChannels(false);

  // Put device into sleep mode
  SetSleepMode(true);

  initialized_ = false;
  updateStatistics(true);
  return DriverStatus::OK;
}

// Reset the MAX22200 device
template <typename SpiType> DriverStatus MAX22200<SpiType>::Reset() {
  // Set reset bit
  DriverStatus status =
      writeRegister(Registers::GLOBAL_CONFIG, GlobalConfigBits::RESET_MASK);
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
template <typename SpiType>
DriverStatus MAX22200<SpiType>::ConfigureGlobal(const GlobalConfig &config) {
  uint16_t reg_value = buildGlobalConfigValue(config);
  DriverStatus status = writeRegister(Registers::GLOBAL_CONFIG, reg_value);
  updateStatistics(status == DriverStatus::OK);
  return status;
}

// Get current global configuration
template <typename SpiType>
DriverStatus MAX22200<SpiType>::GetGlobalConfig(GlobalConfig &config) const {
  uint16_t reg_value;
  DriverStatus status = readRegister(Registers::GLOBAL_CONFIG, reg_value);
  if (status == DriverStatus::OK) {
    config = parseGlobalConfigValue(reg_value);
  }
  updateStatistics(status == DriverStatus::OK);
  return status;
}

// Enable or disable sleep mode
template <typename SpiType>
DriverStatus MAX22200<SpiType>::SetSleepMode(bool enable) {
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
template <typename SpiType>
DriverStatus MAX22200<SpiType>::SetDiagnosticMode(bool enable) {
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
template <typename SpiType>
DriverStatus MAX22200<SpiType>::SetIntegratedCurrentSensing(bool enable) {
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
template <typename SpiType>
DriverStatus MAX22200<SpiType>::ConfigureChannel(uint8_t channel,
                                                 const ChannelConfig &config) {
  if (!IsValidChannel(channel)) {
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
  DriverStatus status =
      writeRegister(getChannelConfigReg(channel), config_value);
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
template <typename SpiType>
DriverStatus MAX22200<SpiType>::GetChannelConfig(uint8_t channel,
                                                 ChannelConfig &config) const {
  if (!IsValidChannel(channel)) {
    updateStatistics(false);
    return DriverStatus::INVALID_PARAMETER;
  }

  // Read channel configuration
  uint16_t config_value;
  DriverStatus status =
      readRegister(getChannelConfigReg(channel), config_value);
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
template <typename SpiType>
DriverStatus
MAX22200<SpiType>::ConfigureAllChannels(const ChannelConfigArray &configs) {
  DriverStatus status = DriverStatus::OK;

  for (uint8_t channel = 0; channel < NUM_CHANNELS_; ++channel) {
    DriverStatus channel_status = ConfigureChannel(channel, configs[channel]);
    if (channel_status != DriverStatus::OK) {
      status = channel_status;
    }
  }

  updateStatistics(status == DriverStatus::OK);
  return status;
}

// Get configuration of all channels
template <typename SpiType>
DriverStatus
MAX22200<SpiType>::GetAllChannelConfigs(ChannelConfigArray &configs) const {
  DriverStatus status = DriverStatus::OK;

  for (uint8_t channel = 0; channel < NUM_CHANNELS_; ++channel) {
    DriverStatus channel_status = GetChannelConfig(channel, configs[channel]);
    if (channel_status != DriverStatus::OK) {
      status = channel_status;
    }
  }

  updateStatistics(status == DriverStatus::OK);
  return status;
}

// Enable or disable a specific channel
template <typename SpiType>
DriverStatus MAX22200<SpiType>::EnableChannel(uint8_t channel, bool enable) {
  if (!IsValidChannel(channel)) {
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
template <typename SpiType>
DriverStatus MAX22200<SpiType>::EnableAllChannels(bool enable) {
  uint16_t value = enable ? 0x00FF : 0x0000;
  DriverStatus status = writeRegister(Registers::CHANNEL_ENABLE, value);
  updateStatistics(status == DriverStatus::OK);
  return status;
}

// Set channel drive mode
template <typename SpiType>
DriverStatus MAX22200<SpiType>::SetChannelDriveMode(uint8_t channel,
                                                    DriveMode mode) {
  if (!IsValidChannel(channel)) {
    updateStatistics(false);
    return DriverStatus::INVALID_PARAMETER;
  }

  uint16_t current_value;
  DriverStatus status =
      readRegister(getChannelConfigReg(channel), current_value);
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
template <typename SpiType>
DriverStatus MAX22200<SpiType>::SetChannelBridgeMode(uint8_t channel,
                                                     BridgeMode mode) {
  if (!IsValidChannel(channel)) {
    updateStatistics(false);
    return DriverStatus::INVALID_PARAMETER;
  }

  uint16_t current_value;
  DriverStatus status =
      readRegister(getChannelConfigReg(channel), current_value);
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
template <typename SpiType>
DriverStatus MAX22200<SpiType>::SetChannelPolarity(uint8_t channel,
                                                   OutputPolarity polarity) {
  if (!IsValidChannel(channel)) {
    updateStatistics(false);
    return DriverStatus::INVALID_PARAMETER;
  }

  uint16_t current_value;
  DriverStatus status =
      readRegister(getChannelConfigReg(channel), current_value);
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
template <typename SpiType>
DriverStatus MAX22200<SpiType>::SetHitCurrent(uint8_t channel,
                                              uint16_t current) {
  if (!IsValidChannel(channel) || current > CurrentRange::MAX_HIT_CURRENT) {
    updateStatistics(false);
    return DriverStatus::INVALID_PARAMETER;
  }

  uint16_t current_value;
  DriverStatus status =
      readRegister(getChannelCurrentReg(channel), current_value);
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
template <typename SpiType>
DriverStatus MAX22200<SpiType>::SetHoldCurrent(uint8_t channel,
                                               uint16_t current) {
  if (!IsValidChannel(channel) || current > CurrentRange::MAX_HOLD_CURRENT) {
    updateStatistics(false);
    return DriverStatus::INVALID_PARAMETER;
  }

  uint16_t current_value;
  DriverStatus status =
      readRegister(getChannelCurrentReg(channel), current_value);
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
template <typename SpiType>
DriverStatus MAX22200<SpiType>::SetCurrents(uint8_t channel,
                                            uint16_t hit_current,
                                            uint16_t hold_current) {
  if (!IsValidChannel(channel) || hit_current > CurrentRange::MAX_HIT_CURRENT ||
      hold_current > CurrentRange::MAX_HOLD_CURRENT) {
    updateStatistics(false);
    return DriverStatus::INVALID_PARAMETER;
  }

  uint16_t current_value = (hit_current << 8) | hold_current;
  DriverStatus status =
      writeRegister(getChannelCurrentReg(channel), current_value);
  updateStatistics(status == DriverStatus::OK);
  return status;
}

// Get current settings for a channel
template <typename SpiType>
DriverStatus MAX22200<SpiType>::GetCurrents(uint8_t channel,
                                            uint16_t &hit_current,
                                            uint16_t &hold_current) const {
  if (!IsValidChannel(channel)) {
    updateStatistics(false);
    return DriverStatus::INVALID_PARAMETER;
  }

  uint16_t current_value;
  DriverStatus status =
      readRegister(getChannelCurrentReg(channel), current_value);
  if (status == DriverStatus::OK) {
    hit_current = (current_value >> 8) & 0x03FF;
    hold_current = current_value & 0x03FF;
  }
  updateStatistics(status == DriverStatus::OK);
  return status;
}

// Set HIT time for a channel
template <typename SpiType>
DriverStatus MAX22200<SpiType>::SetHitTime(uint8_t channel, uint16_t time) {
  if (!IsValidChannel(channel) || time > TimingRange::MAX_HIT_TIME) {
    updateStatistics(false);
    return DriverStatus::INVALID_PARAMETER;
  }

  DriverStatus status = writeRegister(getChannelTimingReg(channel), time);
  updateStatistics(status == DriverStatus::OK);
  return status;
}

// Get HIT time for a channel
template <typename SpiType>
DriverStatus MAX22200<SpiType>::GetHitTime(uint8_t channel,
                                           uint16_t &time) const {
  if (!IsValidChannel(channel)) {
    updateStatistics(false);
    return DriverStatus::INVALID_PARAMETER;
  }

  DriverStatus status = readRegister(getChannelTimingReg(channel), time);
  updateStatistics(status == DriverStatus::OK);
  return status;
}

// Read fault status
template <typename SpiType>
DriverStatus MAX22200<SpiType>::ReadFaultStatus(FaultStatus &status) const {
  uint16_t fault_value;
  DriverStatus result = readRegister(Registers::FAULT_STATUS, fault_value);
  if (result == DriverStatus::OK) {
    status = parseFaultStatusValue(fault_value);
  }
  updateStatistics(result == DriverStatus::OK);
  return result;
}

// Clear fault status
template <typename SpiType> DriverStatus MAX22200<SpiType>::ClearFaultStatus() {
  // Write 0x00FF to clear all fault flags
  DriverStatus status = writeRegister(Registers::FAULT_STATUS, 0x00FF);
  updateStatistics(status == DriverStatus::OK);
  return status;
}

// Read channel status
template <typename SpiType>
DriverStatus MAX22200<SpiType>::ReadChannelStatus(uint8_t channel,
                                                  ChannelStatus &status) const {
  if (!IsValidChannel(channel)) {
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
  result = ReadFaultStatus(fault_status);
  if (result == DriverStatus::OK) {
    // Check if this channel has a fault (simplified - would need
    // channel-specific fault bits)
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
template <typename SpiType>
DriverStatus
MAX22200<SpiType>::ReadAllChannelStatuses(ChannelStatusArray &statuses) const {
  DriverStatus status = DriverStatus::OK;

  for (uint8_t channel = 0; channel < NUM_CHANNELS_; ++channel) {
    DriverStatus channel_status = ReadChannelStatus(channel, statuses[channel]);
    if (channel_status != DriverStatus::OK) {
      status = channel_status;
    }
  }

  updateStatistics(status == DriverStatus::OK);
  return status;
}

// Get driver statistics
template <typename SpiType>
DriverStatus MAX22200<SpiType>::GetStatistics(DriverStatistics &stats) const {
  stats = statistics_;
  return DriverStatus::OK;
}

// Reset driver statistics
template <typename SpiType> DriverStatus MAX22200<SpiType>::ResetStatistics() {
  statistics_ = DriverStatistics();
  return DriverStatus::OK;
}

// Set fault callback function
template <typename SpiType>
void MAX22200<SpiType>::SetFaultCallback(FaultCallback callback,
                                         void *user_data) {
  fault_callback_ = callback;
  fault_user_data_ = user_data;
}

// Set state change callback function
template <typename SpiType>
void MAX22200<SpiType>::SetStateChangeCallback(StateChangeCallback callback,
                                               void *user_data) {
  state_callback_ = callback;
  state_user_data_ = user_data;
}

// Check if driver is initialized
template <typename SpiType> bool MAX22200<SpiType>::IsInitialized() const {
  return initialized_;
}

// Private method implementations

// Write register
template <typename SpiType>
DriverStatus MAX22200<SpiType>::writeRegister(uint8_t reg,
                                              uint16_t value) const {
  uint8_t tx_data[3] = {reg, static_cast<uint8_t>(value >> 8),
                        static_cast<uint8_t>(value & 0xFF)};
  uint8_t rx_data[3];

  spi_interface_.SetChipSelect(false);
  bool success = spi_interface_.Transfer(tx_data, rx_data, 3);
  spi_interface_.SetChipSelect(true);

  if (!success) {
    return DriverStatus::COMMUNICATION_ERROR;
  }

  return DriverStatus::OK;
}

// Read register
template <typename SpiType>
DriverStatus MAX22200<SpiType>::readRegister(uint8_t reg,
                                             uint16_t &value) const {
  uint8_t tx_data[3] = {static_cast<uint8_t>(reg | 0x80), 0x00, 0x00};
  uint8_t rx_data[3];

  spi_interface_.SetChipSelect(false);
  bool success = spi_interface_.Transfer(tx_data, rx_data, 3);
  spi_interface_.SetChipSelect(true);

  if (!success) {
    return DriverStatus::COMMUNICATION_ERROR;
  }

  value = (static_cast<uint16_t>(rx_data[1]) << 8) | rx_data[2];
  return DriverStatus::OK;
}

// Write register array
template <typename SpiType>
DriverStatus MAX22200<SpiType>::writeRegisterArray(uint8_t reg,
                                                   const uint8_t *data,
                                                   size_t length) const {
  uint8_t *tx_data = new uint8_t[length + 1];
  uint8_t *rx_data = new uint8_t[length + 1];

  tx_data[0] = reg;
  memcpy(&tx_data[1], data, length);

  spi_interface_.SetChipSelect(false);
  bool success = spi_interface_.Transfer(tx_data, rx_data, length + 1);
  spi_interface_.SetChipSelect(true);

  delete[] tx_data;
  delete[] rx_data;

  if (!success) {
    return DriverStatus::COMMUNICATION_ERROR;
  }

  return DriverStatus::OK;
}

// Read register array
template <typename SpiType>
DriverStatus MAX22200<SpiType>::readRegisterArray(uint8_t reg, uint8_t *data,
                                                  size_t length) const {
  uint8_t *tx_data = new uint8_t[length + 1];
  uint8_t *rx_data = new uint8_t[length + 1];

  tx_data[0] = static_cast<uint8_t>(reg | 0x80);
  memset(&tx_data[1], 0x00, length);

  spi_interface_.SetChipSelect(false);
  bool success = spi_interface_.Transfer(tx_data, rx_data, length + 1);
  spi_interface_.SetChipSelect(true);

  if (success) {
    memcpy(data, &rx_data[1], length);
  }

  delete[] tx_data;
  delete[] rx_data;

  if (!success) {
    return DriverStatus::COMMUNICATION_ERROR;
  }

  return DriverStatus::OK;
}

// Update channel enable register
template <typename SpiType>
DriverStatus MAX22200<SpiType>::updateChannelEnableRegister() const {
  // This would read all channel configurations and update the enable register
  // Implementation depends on how channel enable state is managed
  return DriverStatus::OK;
}

// Update global config register
template <typename SpiType>
DriverStatus MAX22200<SpiType>::updateGlobalConfigRegister() const {
  // This would update the global configuration register
  // Implementation depends on current global config state
  return DriverStatus::OK;
}

// Update statistics
template <typename SpiType>
void MAX22200<SpiType>::updateStatistics(bool success) const {
  statistics_.total_transfers++;
  if (!success) {
    statistics_.failed_transfers++;
  }
}

// Trigger fault callback
template <typename SpiType>
void MAX22200<SpiType>::triggerFaultCallback(uint8_t channel,
                                             FaultType fault_type) const {
  if (fault_callback_) {
    fault_callback_(channel, fault_type, fault_user_data_);
    statistics_.fault_events++;
  }
}

// Trigger state change callback
template <typename SpiType>
void MAX22200<SpiType>::triggerStateChangeCallback(
    uint8_t channel, ChannelState old_state, ChannelState new_state) const {
  if (state_callback_) {
    state_callback_(channel, old_state, new_state, state_user_data_);
    statistics_.state_changes++;
  }
}

// Build channel config value
template <typename SpiType>
uint16_t
MAX22200<SpiType>::buildChannelConfigValue(const ChannelConfig &config) const {
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
template <typename SpiType>
ChannelConfig MAX22200<SpiType>::parseChannelConfigValue(uint16_t value) const {
  ChannelConfig config;

  config.drive_mode = (value & ChannelConfigBits::DRIVE_MODE_MASK)
                          ? DriveMode::VDR
                          : DriveMode::CDR;

  config.bridge_mode = (value & ChannelConfigBits::BRIDGE_MODE_MASK)
                           ? BridgeMode::FULL_BRIDGE
                           : BridgeMode::HALF_BRIDGE;

  config.parallel_mode = (value & ChannelConfigBits::PARALLEL_MASK) != 0;

  config.polarity = (value & ChannelConfigBits::POLARITY_MASK)
                        ? OutputPolarity::INVERTED
                        : OutputPolarity::NORMAL;

  return config;
}

// Build global config value
template <typename SpiType>
uint16_t
MAX22200<SpiType>::buildGlobalConfigValue(const GlobalConfig &config) const {
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
template <typename SpiType>
GlobalConfig MAX22200<SpiType>::parseGlobalConfigValue(uint16_t value) const {
  GlobalConfig config;

  config.reset = (value & GlobalConfigBits::RESET_MASK) != 0;
  config.sleep_mode = (value & GlobalConfigBits::SLEEP_MASK) != 0;
  config.diagnostic_enable = (value & GlobalConfigBits::DIAG_EN_MASK) != 0;
  config.ics_enable = (value & GlobalConfigBits::ICS_EN_MASK) != 0;
  config.daisy_chain_mode = (value & GlobalConfigBits::DAISY_CHAIN_MASK) != 0;

  return config;
}

// Parse fault status value
template <typename SpiType>
FaultStatus MAX22200<SpiType>::parseFaultStatusValue(uint16_t value) const {
  FaultStatus status;

  status.overcurrent_protection = (value & FaultStatusBits::OCP_MASK) != 0;
  status.open_load = (value & FaultStatusBits::OL_MASK) != 0;
  status.plunger_movement = (value & FaultStatusBits::DPM_MASK) != 0;
  status.undervoltage_lockout = (value & FaultStatusBits::UVLO_MASK) != 0;
  status.hit_current_not_reached = (value & FaultStatusBits::HHF_MASK) != 0;
  status.thermal_shutdown = (value & FaultStatusBits::TSD_MASK) != 0;

  return status;
}

// Note: Namespace is closed in MAX22200.h, not here

#endif // MAX22200_IMPL

/**
 * @file max22200.ipp
 * @brief Template implementation for MAX22200 driver
 *
 * Implements the two-phase SPI protocol per MAX22200 datasheet (Rev 1, 3/25):
 *   Phase 1: CMD pin HIGH → write 8-bit Command Register → CMD pin LOW
 *   Phase 2: CMD pin LOW  → read/write 8 or 32 bits of data
 *
 * @copyright Copyright (c) 2024-2025 HardFOC. All rights reserved.
 */
#pragma once
#include <cmath>
#include <cstring>

namespace max22200 {

// ============================================================================
// Constructor / Destructor
// ============================================================================

template <typename SpiType>
MAX22200<SpiType>::MAX22200(SpiType &spi_interface)
    : spi_interface_(spi_interface), initialized_(false), statistics_(),
      last_fault_byte_(0xFF), cached_status_(), board_config_(),
      fault_callback_(nullptr), fault_user_data_(nullptr),
      state_callback_(nullptr), state_user_data_(nullptr) {}

template <typename SpiType>
MAX22200<SpiType>::MAX22200(SpiType &spi_interface, const BoardConfig &board_config)
    : spi_interface_(spi_interface), initialized_(false), statistics_(),
      last_fault_byte_(0xFF), cached_status_(), board_config_(board_config),
      fault_callback_(nullptr), fault_user_data_(nullptr),
      state_callback_(nullptr), state_user_data_(nullptr) {}

template <typename SpiType>
MAX22200<SpiType>::~MAX22200() {
  if (initialized_) {
    Deinitialize();
  }
}

// ============================================================================
// Initialization (per datasheet Figure 6 flowchart)
// ============================================================================

template <typename SpiType>
DriverStatus MAX22200<SpiType>::Initialize() {
  if (initialized_) {
    return DriverStatus::OK;
  }

  // Step 0: Initialize SPI interface
  if (!spi_interface_.Initialize()) {
    updateStatistics(false);
    return DriverStatus::INITIALIZATION_ERROR;
  }

  // Configure SPI: Mode 0 (CPOL=0, CPHA=0), MSB first
  if (!spi_interface_.Configure(MAX_SPI_FREQ_STANDALONE_, 0, true)) {
    updateStatistics(false);
    return DriverStatus::INITIALIZATION_ERROR;
  }

  // Step 1: ENABLE pin HIGH, power-up delay
  spi_interface_.GpioSetActive(CtrlPin::ENABLE);
  spi_interface_.DelayUs(500); // 0.5ms power-up delay

  constexpr int kMaxInitRetries = 3;
  StatusConfig status;
  DriverStatus result;

  for (int attempt = 0; attempt < kMaxInitRetries; ++attempt) {
    // Step 2: Read STATUS register to clear UVM and deassert nFAULT
    result = ReadStatus(status);
    if (result != DriverStatus::OK) {
      spi_interface_.GpioSetInactive(CtrlPin::ENABLE);
      updateStatistics(false);
      return result;
    }
    if (GetLastFaultByte() == StatusReg::FAULT_BYTE_COMER) {
      continue; // Communication error — retry per datasheet Figure 6
    }

    // Step 3: Write STATUS register — set ACTIVE=1, all channels off,
    // channel-pair modes independent, mask communication fault (default)
    status.active = true;
    status.channels_on_mask = 0x00; // All channels off initially
    status.communication_error_masked = true; // Mask communication fault (reset default)

    result = WriteStatus(status);
    if (result != DriverStatus::OK) {
      spi_interface_.GpioSetInactive(CtrlPin::ENABLE);
      updateStatistics(false);
      return result;
    }
    if (GetLastFaultByte() == StatusReg::FAULT_BYTE_COMER) {
      continue; // Communication error — retry
    }

    // Cache the status for ONCH updates
    cached_status_ = status;

    // Step 4: Read STATUS again to verify UVM is cleared
    result = ReadStatus(status);
    if (result != DriverStatus::OK) {
      spi_interface_.GpioSetInactive(CtrlPin::ENABLE);
      updateStatistics(false);
      return result;
    }
    if (GetLastFaultByte() == StatusReg::FAULT_BYTE_COMER) {
      continue; // Communication error — retry
    }

    if (status.undervoltage) {
      // UVM still set — device may not have full power
      // Continue anyway but note the condition
    }

    cached_status_ = status;
    initialized_ = true;
    updateStatistics(true);
    return DriverStatus::OK;
  }

  // COMER persisted after all retries
  spi_interface_.GpioSetInactive(CtrlPin::ENABLE);
  updateStatistics(false);
  return DriverStatus::COMMUNICATION_ERROR;
}

template <typename SpiType>
DriverStatus MAX22200<SpiType>::Deinitialize() {
  if (!initialized_) {
    return DriverStatus::OK;
  }

  // Disable all channels
  DisableAllChannels();

  // Set ACTIVE=0
  cached_status_.active = false;
  cached_status_.channels_on_mask = 0;
  WriteStatus(cached_status_);

  // ENABLE pin LOW
  spi_interface_.GpioSetInactive(CtrlPin::ENABLE);

  initialized_ = false;
  updateStatistics(true);
  return DriverStatus::OK;
}

template <typename SpiType>
bool MAX22200<SpiType>::IsInitialized() const {
  return initialized_;
}

// ============================================================================
// STATUS Register Operations
// ============================================================================

template <typename SpiType>
DriverStatus MAX22200<SpiType>::ReadStatus(StatusConfig &status) const {
  uint32_t raw;
  DriverStatus result = readReg32(RegBank::STATUS, raw);
  if (result == DriverStatus::OK) {
    status.fromRegister(raw);
    cached_status_ = status;  // Keep cache in sync for FREQM, ONCH, duty limits, etc.
  }
  return result;
}

template <typename SpiType>
DriverStatus MAX22200<SpiType>::WriteStatus(const StatusConfig &status) {
  uint32_t raw = status.toRegister();
  DriverStatus result = writeReg32(RegBank::STATUS, raw);
  if (result == DriverStatus::OK) {
    cached_status_ = status;  // Keep cache in sync so FREQM, ONCH, etc. are correct for calculations
  }
  return result;
}

// ============================================================================
// Channel Configuration
// ============================================================================

template <typename SpiType>
DriverStatus MAX22200<SpiType>::ConfigureChannel(uint8_t channel,
                                                  const ChannelConfig &config) {
  if (!IsValidChannel(channel)) {
    updateStatistics(false);
    return DriverStatus::INVALID_PARAMETER;
  }

  // CDR requires board IFS (from SetBoardConfig); IFS is set by RREF, not per channel
  if (config.drive_mode == DriveMode::CDR && board_config_.full_scale_current_ma == 0) {
    updateStatistics(false);
    return DriverStatus::INVALID_PARAMETER;  // Call SetBoardConfig with RREF first
  }

  // Datasheet: SRC mode only for fCHOP < 50 kHz
  if (config.slew_rate_control_enabled &&
      getChopFreqKhz(cached_status_.master_clock_80khz, config.chop_freq) >= 50) {
    updateStatistics(false);
    return DriverStatus::INVALID_PARAMETER;
  }

  uint32_t reg_val = config.toRegister(board_config_.full_scale_current_ma, cached_status_.master_clock_80khz);
  DriverStatus result = writeReg32(getChannelCfgBank(channel), reg_val);
  updateStatistics(result == DriverStatus::OK);
  return result;
}

template <typename SpiType>
DriverStatus MAX22200<SpiType>::GetChannelConfig(uint8_t channel,
                                                  ChannelConfig &config) const {
  if (!IsValidChannel(channel)) {
    updateStatistics(false);
    return DriverStatus::INVALID_PARAMETER;
  }

  uint32_t raw;
  DriverStatus result = readReg32(getChannelCfgBank(channel), raw);
  if (result == DriverStatus::OK) {
    // Pass context (IFS and FREQM) for proper conversion to user units
    config.fromRegister(raw, board_config_.full_scale_current_ma, cached_status_.master_clock_80khz);
  }
  updateStatistics(result == DriverStatus::OK);
  return result;
}

template <typename SpiType>
DriverStatus MAX22200<SpiType>::ConfigureAllChannels(
    const ChannelConfigArray &configs) {
  DriverStatus status = DriverStatus::OK;
  for (uint8_t ch = 0; ch < NUM_CHANNELS_; ++ch) {
    DriverStatus result = ConfigureChannel(ch, configs[ch]);
    if (result != DriverStatus::OK) {
      status = result;
    }
  }
  return status;
}

template <typename SpiType>
DriverStatus MAX22200<SpiType>::GetAllChannelConfigs(
    ChannelConfigArray &configs) const {
  DriverStatus status = DriverStatus::OK;
  for (uint8_t ch = 0; ch < NUM_CHANNELS_; ++ch) {
    DriverStatus result = GetChannelConfig(ch, configs[ch]);
    if (result != DriverStatus::OK) {
      status = result;
    }
  }
  return status;
}

// ============================================================================
// Channel Enable/Disable (ONCH bits in STATUS[31:24])
// ============================================================================

template <typename SpiType>
DriverStatus MAX22200<SpiType>::EnableChannel(uint8_t channel) {
  return SetChannelEnabled(channel, true);
}

template <typename SpiType>
DriverStatus MAX22200<SpiType>::DisableChannel(uint8_t channel) {
  return SetChannelEnabled(channel, false);
}

template <typename SpiType>
DriverStatus MAX22200<SpiType>::SetChannelEnabled(uint8_t channel, bool enable) {
  if (!IsValidChannel(channel)) {
    updateStatistics(false);
    return DriverStatus::INVALID_PARAMETER;
  }
  if (enable) {
    cached_status_.channels_on_mask |= (1u << channel);
  } else {
    cached_status_.channels_on_mask &= ~(1u << channel);
  }
  return SetChannelsOn(cached_status_.channels_on_mask);
}

template <typename SpiType>
DriverStatus MAX22200<SpiType>::EnableAllChannels() {
  return SetAllChannelsEnabled(true);
}

template <typename SpiType>
DriverStatus MAX22200<SpiType>::DisableAllChannels() {
  return SetAllChannelsEnabled(false);
}

template <typename SpiType>
DriverStatus MAX22200<SpiType>::SetAllChannelsEnabled(bool enable) {
  cached_status_.channels_on_mask = enable ? 0xFFu : 0x00u;
  return SetChannelsOn(cached_status_.channels_on_mask);
}

template <typename SpiType>
DriverStatus MAX22200<SpiType>::SetChannelsOn(uint8_t channel_mask) {
  cached_status_.channels_on_mask = channel_mask;
  DriverStatus result = writeReg8(RegBank::STATUS, channel_mask);
  updateStatistics(result == DriverStatus::OK);
  return result;
}

template <typename SpiType>
DriverStatus MAX22200<SpiType>::SetFullBridgeState(uint8_t pair_index,
                                                  FullBridgeState state) {
  if (pair_index > 3) {
    updateStatistics(false);
    return DriverStatus::INVALID_PARAMETER;
  }
  const uint8_t shift = pair_index * 2;
  const uint8_t pair_mask = 3u << shift;
  uint8_t bits;
  switch (state) {
    case FullBridgeState::HiZ:
      bits = 0;
      break;
    case FullBridgeState::Forward:
      bits = 1u << shift;  // ONCHx = 1, ONCHy = 0
      break;
    case FullBridgeState::Reverse:
      bits = 2u << shift;  // ONCHx = 0, ONCHy = 1
      break;
    case FullBridgeState::Brake:
      bits = 3u << shift;
      break;
    default:
      bits = 0;
      break;
  }
  cached_status_.channels_on_mask = (cached_status_.channels_on_mask & ~pair_mask) | bits;
  return SetChannelsOn(cached_status_.channels_on_mask);
}

// ============================================================================
// Fault Operations
// ============================================================================

template <typename SpiType>
DriverStatus MAX22200<SpiType>::ReadFaultRegister(FaultStatus &faults) const {
  uint32_t raw;
  DriverStatus result = readReg32(RegBank::FAULT, raw);
  if (result == DriverStatus::OK) {
    faults.fromRegister(raw);
  }
  updateStatistics(result == DriverStatus::OK);
  return result;
}

template <typename SpiType>
DriverStatus MAX22200<SpiType>::ClearAllFaults() {
  FaultStatus f;
  return ReadFaultRegister(f);
}

template <typename SpiType>
DriverStatus MAX22200<SpiType>::ClearChannelFaults(uint8_t channel_mask,
                                                    FaultStatus *out_faults) const {
  FaultStatus temp;
  DriverStatus result = ReadFaultRegisterSelectiveClear(
      channel_mask, channel_mask, channel_mask, channel_mask, temp);
  if (result == DriverStatus::OK && out_faults) {
    *out_faults = temp;
  }
  return result;
}

template <typename SpiType>
DriverStatus MAX22200<SpiType>::ReadFaultRegisterSelectiveClear(
    uint8_t ocp_mask, uint8_t hhf_mask, uint8_t olf_mask, uint8_t dpm_mask,
    FaultStatus &faults) const {
  DriverStatus result = writeCommandRegister(RegBank::FAULT, false, false);
  if (result != DriverStatus::OK) {
    updateStatistics(false);
    return result;
  }
  const uint8_t tx[4] = {ocp_mask, hhf_mask, olf_mask, dpm_mask};
  uint32_t raw;
  result = readData32WithTx(tx, raw);
  if (result == DriverStatus::OK) {
    faults.fromRegister(raw);
  }
  updateStatistics(result == DriverStatus::OK);
  return result;
}

template <typename SpiType>
DriverStatus MAX22200<SpiType>::ReadFaultFlags(StatusConfig &status) const {
  return ReadStatus(status);
}

template <typename SpiType>
DriverStatus MAX22200<SpiType>::ClearFaultFlags() {
  // Reading the STATUS register clears fault flags
  StatusConfig status;
  DriverStatus result = ReadStatus(status);
  if (result == DriverStatus::OK) {
    cached_status_ = status;
  }
  return result;
}

template <typename SpiType>
DriverStatus MAX22200<SpiType>::ReadDpmConfig(DpmConfig &config) const {
  uint32_t raw;
  DriverStatus result = readReg32(RegBank::CFG_DPM, raw);
  if (result == DriverStatus::OK) {
    config.fromRegister(raw);
  }
  updateStatistics(result == DriverStatus::OK);
  return result;
}

template <typename SpiType>
DriverStatus MAX22200<SpiType>::WriteDpmConfig(const DpmConfig &config) {
  DriverStatus result = writeReg32(RegBank::CFG_DPM, config.toRegister());
  updateStatistics(result == DriverStatus::OK);
  return result;
}

template <typename SpiType>
DriverStatus MAX22200<SpiType>::SetDpmEnabledChannels(uint8_t channel_mask) {
  for (uint8_t ch = 0; ch < NUM_CHANNELS_; ++ch) {
    ChannelConfig config;
    DriverStatus result = GetChannelConfig(ch, config);
    if (result != DriverStatus::OK) {
      return result;
    }
    bool enable = (channel_mask & (1u << ch)) != 0;
    if (config.plunger_movement_detection_enabled != enable) {
      config.plunger_movement_detection_enabled = enable;
      result = ConfigureChannel(ch, config);
      if (result != DriverStatus::OK) {
        return result;
      }
    }
  }
  return DriverStatus::OK;
}

template <typename SpiType>
DriverStatus MAX22200<SpiType>::ConfigureDpm(float start_current_ma,
                                              float dip_threshold_ma,
                                              float debounce_ms) {
  if (board_config_.full_scale_current_ma == 0) {
    updateStatistics(false);
    return DriverStatus::INVALID_PARAMETER;
  }
  DpmConfig config;
  DriverStatus result = ReadDpmConfig(config);
  if (result != DriverStatus::OK) {
    return result;
  }
  // ISTART = plunger_movement_start_current × (IFS/127)  =>  value = round(start_ma / IFS_ma * 127)
  float ratio = start_current_ma / static_cast<float>(board_config_.full_scale_current_ma);
  config.plunger_movement_start_current = static_cast<uint8_t>(ratio * 127.0f + 0.5f);
  if (config.plunger_movement_start_current > 127) config.plunger_movement_start_current = 127;
  ratio = dip_threshold_ma / static_cast<float>(board_config_.full_scale_current_ma);
  uint32_t ipth = static_cast<uint32_t>(ratio * 127.0f + 0.5f);
  config.plunger_movement_current_threshold = (ipth > 15) ? 15 : static_cast<uint8_t>(ipth);
  // TDEB = plunger_movement_debounce_time / fCHOP; use actual fCHOP from cached STATUS (FREQM + FMAIN_DIV4)
  uint32_t fchop_khz = getChopFreqKhz(cached_status_.master_clock_80khz, ChopFreq::FMAIN_DIV4);
  float periods_f = debounce_ms * static_cast<float>(fchop_khz) + 0.5f;
  uint32_t periods = static_cast<uint32_t>(periods_f);
  config.plunger_movement_debounce_time = (periods > 15) ? 15 : static_cast<uint8_t>(periods);
  return WriteDpmConfig(config);
}

// ============================================================================
// Device Control
// ============================================================================

template <typename SpiType>
DriverStatus MAX22200<SpiType>::EnableDevice() {
  return SetDeviceEnable(true);
}

template <typename SpiType>
DriverStatus MAX22200<SpiType>::DisableDevice() {
  return SetDeviceEnable(false);
}

template <typename SpiType>
DriverStatus MAX22200<SpiType>::SetDeviceEnable(bool enable) {
  if (enable) {
    spi_interface_.GpioSetActive(CtrlPin::ENABLE);
  } else {
    spi_interface_.GpioSetInactive(CtrlPin::ENABLE);
  }
  updateStatistics(true);
  return DriverStatus::OK;
}

template <typename SpiType>
DriverStatus MAX22200<SpiType>::GetFaultPinState(bool &fault_active) const {
  GpioSignal signal;
  if (!spi_interface_.GpioRead(CtrlPin::FAULT, signal)) {
    return DriverStatus::COMMUNICATION_ERROR;
  }
  fault_active = (signal == GpioSignal::ACTIVE);
  return DriverStatus::OK;
}

// ============================================================================
// Raw Register Access (for debug)
// ============================================================================

template <typename SpiType>
DriverStatus MAX22200<SpiType>::ReadRegister32(uint8_t bank,
                                               uint32_t &value) const {
  return readReg32(bank, value);
}

template <typename SpiType>
DriverStatus MAX22200<SpiType>::WriteRegister32(uint8_t bank, uint32_t value) {
  return writeReg32(bank, value);
}

template <typename SpiType>
DriverStatus MAX22200<SpiType>::ReadRegister8(uint8_t bank,
                                              uint8_t &value) const {
  return readReg8(bank, value);
}

template <typename SpiType>
DriverStatus MAX22200<SpiType>::WriteRegister8(uint8_t bank, uint8_t value) {
  return writeReg8(bank, value);
}

template <typename SpiType>
uint8_t MAX22200<SpiType>::GetLastFaultByte() const {
  return last_fault_byte_;
}

// ============================================================================
// Statistics
// ============================================================================

template <typename SpiType>
DriverStatistics MAX22200<SpiType>::GetStatistics() const {
  return statistics_;
}

template <typename SpiType>
void MAX22200<SpiType>::ResetStatistics() {
  statistics_ = DriverStatistics{};
}

// ============================================================================
// Callbacks
// ============================================================================

template <typename SpiType>
void MAX22200<SpiType>::SetFaultCallback(FaultCallback callback,
                                         void *user_data) {
  fault_callback_ = callback;
  fault_user_data_ = user_data;
}

template <typename SpiType>
void MAX22200<SpiType>::SetStateChangeCallback(StateChangeCallback callback,
                                               void *user_data) {
  state_callback_ = callback;
  state_user_data_ = user_data;
}

// ============================================================================
// Private: Two-Phase SPI Protocol Implementation
// ============================================================================

/**
 * Phase 1: Write the 8-bit Command Register
 *
 * Per datasheet:
 * - CMD pin must be HIGH during the entire transfer
 * - CMD must be HIGH at the rising edge of CSB (CS going HIGH)
 * - Device returns STATUS[7:0] on SDO
 */
template <typename SpiType>
DriverStatus MAX22200<SpiType>::writeCommandRegister(uint8_t bank,
                                                      bool is_write,
                                                      bool mode8) const {
  uint8_t cmd_byte = CommandReg::build(bank, is_write, mode8);
  uint8_t rx_byte = 0xFF;

  // CMD pin HIGH for Command Register write
  spi_interface_.GpioSetActive(CtrlPin::CMD);

  // CS LOW → transfer 1 byte → CS HIGH
  // Note: ESP-IDF handles CS automatically in the Transfer call
  bool success = spi_interface_.Transfer(&cmd_byte, &rx_byte, 1);

  // CMD pin LOW (prepare for data phase)
  spi_interface_.GpioSetInactive(CtrlPin::CMD);

  if (!success) {
    return DriverStatus::COMMUNICATION_ERROR;
  }

  // Store the fault flags byte returned by the device
  last_fault_byte_ = rx_byte;

  return DriverStatus::OK;
}

/**
 * Phase 2: Write 32-bit data (CMD pin must already be LOW)
 *
 * Per datasheet Figure 10: SDI is LSB first (first byte = DATAIN[7:0])
 * SDI: DATAIN[7:0] DATAIN[15:8] DATAIN[23:16] DATAIN[31:24]
 * SDO: STATUS[7:0] REGCMD        0x00          0x00
 */
template <typename SpiType>
DriverStatus MAX22200<SpiType>::writeData32(uint32_t value) const {
  uint8_t tx[4] = {
      static_cast<uint8_t>(value & 0xFF),
      static_cast<uint8_t>((value >> 8) & 0xFF),
      static_cast<uint8_t>((value >> 16) & 0xFF),
      static_cast<uint8_t>((value >> 24) & 0xFF)};
  uint8_t rx[4];

  bool success = spi_interface_.Transfer(tx, rx, 4);
  if (!success) {
    return DriverStatus::COMMUNICATION_ERROR;
  }
  return DriverStatus::OK;
}

/**
 * Phase 2: Read 32-bit data (CMD pin must already be LOW)
 *
 * Per datasheet Figure 11: SDO is MSB first (first byte = DATA[31:24])
 * SDO: DATA[31:24] DATA[23:16] DATA[15:8] DATA[7:0]
 */
template <typename SpiType>
DriverStatus MAX22200<SpiType>::readData32(uint32_t &value) const {
  uint8_t tx[4] = {0, 0, 0, 0};
  uint8_t rx[4];

  bool success = spi_interface_.Transfer(tx, rx, 4);
  if (!success) {
    return DriverStatus::COMMUNICATION_ERROR;
  }

  value = (static_cast<uint32_t>(rx[0]) << 24) |
          (static_cast<uint32_t>(rx[1]) << 16) |
          (static_cast<uint32_t>(rx[2]) << 8) |
          static_cast<uint32_t>(rx[3]);
  return DriverStatus::OK;
}

template <typename SpiType>
DriverStatus MAX22200<SpiType>::readData32WithTx(const uint8_t tx[4],
                                                uint32_t &value) const {
  uint8_t rx[4];
  bool success = spi_interface_.Transfer(tx, rx, 4);
  if (!success) {
    return DriverStatus::COMMUNICATION_ERROR;
  }
  value = (static_cast<uint32_t>(rx[0]) << 24) |
          (static_cast<uint32_t>(rx[1]) << 16) |
          (static_cast<uint32_t>(rx[2]) << 8) |
          static_cast<uint32_t>(rx[3]);
  return DriverStatus::OK;
}

/**
 * Phase 2: Write 8-bit data (MSB of register, CMD pin must be LOW)
 *
 * Per datasheet Figure 12: single byte
 * SDI: DATAIN[31:24]
 * SDO: STATUS[7:0]
 */
template <typename SpiType>
DriverStatus MAX22200<SpiType>::writeData8(uint8_t value) const {
  uint8_t rx;
  bool success = spi_interface_.Transfer(&value, &rx, 1);
  if (!success) {
    return DriverStatus::COMMUNICATION_ERROR;
  }
  return DriverStatus::OK;
}

/**
 * Phase 2: Read 8-bit data (MSB of register, CMD pin must be LOW)
 *
 * Per datasheet Figure 13: single byte
 * SDO: DATA[31:24]
 */
template <typename SpiType>
DriverStatus MAX22200<SpiType>::readData8(uint8_t &value) const {
  uint8_t tx = 0;
  bool success = spi_interface_.Transfer(&tx, &value, 1);
  if (!success) {
    return DriverStatus::COMMUNICATION_ERROR;
  }
  return DriverStatus::OK;
}

// ============================================================================
// Private: Convenience wrappers (Command Register + Data)
// ============================================================================

template <typename SpiType>
DriverStatus MAX22200<SpiType>::writeReg32(uint8_t bank,
                                            uint32_t value) const {
  DriverStatus result = writeCommandRegister(bank, true, false);
  if (result != DriverStatus::OK) return result;
  return writeData32(value);
}

template <typename SpiType>
DriverStatus MAX22200<SpiType>::readReg32(uint8_t bank,
                                           uint32_t &value) const {
  DriverStatus result = writeCommandRegister(bank, false, false);
  if (result != DriverStatus::OK) return result;
  return readData32(value);
}

template <typename SpiType>
DriverStatus MAX22200<SpiType>::writeReg8(uint8_t bank, uint8_t value) const {
  DriverStatus result = writeCommandRegister(bank, true, true);
  if (result != DriverStatus::OK) return result;
  return writeData8(value);
}

template <typename SpiType>
DriverStatus MAX22200<SpiType>::readReg8(uint8_t bank, uint8_t &value) const {
  DriverStatus result = writeCommandRegister(bank, false, true);
  if (result != DriverStatus::OK) return result;
  return readData8(value);
}

// ============================================================================
// Board/Scale Configuration
// ============================================================================

template <typename SpiType>
void MAX22200<SpiType>::SetBoardConfig(const BoardConfig &config) {
  board_config_ = config;
}

template <typename SpiType>
BoardConfig MAX22200<SpiType>::GetBoardConfig() const {
  return board_config_;
}

// ============================================================================
// Convenience APIs: Current in Real Units (CDR Mode)
// ============================================================================

template <typename SpiType>
DriverStatus MAX22200<SpiType>::SetHitCurrentMa(uint8_t channel, uint32_t ma) {
  if (!IsValidChannel(channel) || board_config_.full_scale_current_ma == 0) {
    updateStatistics(false);
    return DriverStatus::INVALID_PARAMETER;
  }

  // Clamp to max_current_ma if set
  if (board_config_.max_current_ma > 0 && ma > board_config_.max_current_ma) {
    ma = board_config_.max_current_ma;
  }

  ChannelConfig config;
  DriverStatus result = GetChannelConfig(channel, config);
  if (result != DriverStatus::OK) {
    return result;
  }
  config.drive_mode = DriveMode::CDR;
  config.hit_setpoint = static_cast<float>(ma);
  return ConfigureChannel(channel, config);
}

template <typename SpiType>
DriverStatus MAX22200<SpiType>::SetHoldCurrentMa(uint8_t channel, uint32_t ma) {
  if (!IsValidChannel(channel) || board_config_.full_scale_current_ma == 0) {
    updateStatistics(false);
    return DriverStatus::INVALID_PARAMETER;
  }

  if (board_config_.max_current_ma > 0 && ma > board_config_.max_current_ma) {
    ma = board_config_.max_current_ma;
  }

  ChannelConfig config;
  DriverStatus result = GetChannelConfig(channel, config);
  if (result != DriverStatus::OK) {
    return result;
  }
  config.drive_mode = DriveMode::CDR;
  config.hold_setpoint = static_cast<float>(ma);
  return ConfigureChannel(channel, config);
}

template <typename SpiType>
DriverStatus MAX22200<SpiType>::SetHitCurrentA(uint8_t channel, float amps) {
  return SetHitCurrentMa(channel, static_cast<uint32_t>(amps * 1000.0f + 0.5f));
}

template <typename SpiType>
DriverStatus MAX22200<SpiType>::SetHoldCurrentA(uint8_t channel, float amps) {
  return SetHoldCurrentMa(channel, static_cast<uint32_t>(amps * 1000.0f + 0.5f));
}

template <typename SpiType>
DriverStatus MAX22200<SpiType>::SetHitCurrentPercent(uint8_t channel, float percent) {
  if (!IsValidChannel(channel) || board_config_.full_scale_current_ma == 0) {
    updateStatistics(false);
    return DriverStatus::INVALID_PARAMETER;
  }

  // Clamp percent to 0-100
  if (percent < 0.0f) percent = 0.0f;
  if (percent > 100.0f) percent = 100.0f;

  // Convert percent to mA, then use SetHitCurrentMa
  uint32_t ma = static_cast<uint32_t>((percent / 100.0f) * board_config_.full_scale_current_ma + 0.5f);
  return SetHitCurrentMa(channel, ma);
}

template <typename SpiType>
DriverStatus MAX22200<SpiType>::SetHoldCurrentPercent(uint8_t channel, float percent) {
  if (!IsValidChannel(channel) || board_config_.full_scale_current_ma == 0) {
    updateStatistics(false);
    return DriverStatus::INVALID_PARAMETER;
  }

  if (percent < 0.0f) percent = 0.0f;
  if (percent > 100.0f) percent = 100.0f;

  uint32_t ma = static_cast<uint32_t>((percent / 100.0f) * board_config_.full_scale_current_ma + 0.5f);
  return SetHoldCurrentMa(channel, ma);
}

template <typename SpiType>
DriverStatus MAX22200<SpiType>::GetHitCurrentMa(uint8_t channel, uint32_t &ma) const {
  if (!IsValidChannel(channel) || board_config_.full_scale_current_ma == 0) {
    updateStatistics(false);
    return DriverStatus::INVALID_PARAMETER;
  }

  ChannelConfig config;
  DriverStatus result = GetChannelConfig(channel, config);
  if (result != DriverStatus::OK) {
    return result;
  }

  // Return user unit directly (CDR mode stores mA)
  ma = static_cast<uint32_t>(config.hit_setpoint + 0.5f);
  return DriverStatus::OK;
}

template <typename SpiType>
DriverStatus MAX22200<SpiType>::GetHoldCurrentMa(uint8_t channel, uint32_t &ma) const {
  if (!IsValidChannel(channel) || board_config_.full_scale_current_ma == 0) {
    updateStatistics(false);
    return DriverStatus::INVALID_PARAMETER;
  }

  ChannelConfig config;
  DriverStatus result = GetChannelConfig(channel, config);
  if (result != DriverStatus::OK) {
    return result;
  }

  // Return user unit directly (CDR mode stores mA)
  ma = static_cast<uint32_t>(config.hold_setpoint + 0.5f);
  return DriverStatus::OK;
}

template <typename SpiType>
DriverStatus MAX22200<SpiType>::GetHitCurrentPercent(uint8_t channel,
                                                      float &percent) const {
  if (!IsValidChannel(channel)) {
    updateStatistics(false);
    return DriverStatus::INVALID_PARAMETER;
  }

  ChannelConfig config;
  DriverStatus result = GetChannelConfig(channel, config);
  if (result != DriverStatus::OK) {
    return result;
  }

  if (board_config_.full_scale_current_ma > 0) {
    percent = (config.hit_setpoint / static_cast<float>(board_config_.full_scale_current_ma)) * 100.0f;
  } else {
    percent = 0.0f;
  }
  return DriverStatus::OK;
}

template <typename SpiType>
DriverStatus MAX22200<SpiType>::GetHoldCurrentPercent(uint8_t channel,
                                                      float &percent) const {
  if (!IsValidChannel(channel)) {
    updateStatistics(false);
    return DriverStatus::INVALID_PARAMETER;
  }

  ChannelConfig config;
  DriverStatus result = GetChannelConfig(channel, config);
  if (result != DriverStatus::OK) {
    return result;
  }

  if (board_config_.full_scale_current_ma > 0) {
    percent = (config.hold_setpoint / static_cast<float>(board_config_.full_scale_current_ma)) * 100.0f;
  } else {
    percent = 0.0f;
  }
  return DriverStatus::OK;
}

// ============================================================================
// Convenience APIs: Duty Cycle in Percent (VDR Mode)
// ============================================================================

template <typename SpiType>
DriverStatus MAX22200<SpiType>::SetHitDutyPercent(uint8_t channel, float percent) {
  if (!IsValidChannel(channel)) {
    updateStatistics(false);
    return DriverStatus::INVALID_PARAMETER;
  }

  // Clamp to max_duty_percent if set
  if (board_config_.max_duty_percent > 0 && percent > board_config_.max_duty_percent) {
    percent = board_config_.max_duty_percent;
  }

  // Get current config to read FREQM, FREQ_CFG, SRC for duty limits
  ChannelConfig config;
  DriverStatus result = GetChannelConfig(channel, config);
  if (result != DriverStatus::OK) {
    return result;
  }

  // Get duty limits based on FREQM + FREQ_CFG + SRC
  DutyLimits limits;
  result = GetDutyLimits(cached_status_.master_clock_80khz, config.chop_freq, config.slew_rate_control_enabled, limits);
  if (result != DriverStatus::OK) {
    return result;
  }

  // Clamp to [δMIN, δMAX]
  if (percent < limits.min_percent) percent = limits.min_percent;
  if (percent > limits.max_percent) percent = limits.max_percent;

  // Set user unit directly (VDR mode)
  config.drive_mode = DriveMode::VDR;
  config.hit_setpoint = percent;
  return ConfigureChannel(channel, config);
}

template <typename SpiType>
DriverStatus MAX22200<SpiType>::SetHoldDutyPercent(uint8_t channel, float percent) {
  if (!IsValidChannel(channel)) {
    updateStatistics(false);
    return DriverStatus::INVALID_PARAMETER;
  }

  if (board_config_.max_duty_percent > 0 && percent > board_config_.max_duty_percent) {
    percent = board_config_.max_duty_percent;
  }

  ChannelConfig config;
  DriverStatus result = GetChannelConfig(channel, config);
  if (result != DriverStatus::OK) {
    return result;
  }

  DutyLimits limits;
  result = GetDutyLimits(cached_status_.master_clock_80khz, config.chop_freq, config.slew_rate_control_enabled, limits);
  if (result != DriverStatus::OK) {
    return result;
  }

  if (percent < limits.min_percent) percent = limits.min_percent;
  if (percent > limits.max_percent) percent = limits.max_percent;

  // Set user unit directly (VDR mode)
  config.drive_mode = DriveMode::VDR;
  config.hold_setpoint = percent;
  return ConfigureChannel(channel, config);
}

template <typename SpiType>
DriverStatus MAX22200<SpiType>::GetHitDutyPercent(uint8_t channel,
                                                  float &percent) const {
  if (!IsValidChannel(channel)) {
    updateStatistics(false);
    return DriverStatus::INVALID_PARAMETER;
  }

  ChannelConfig config;
  DriverStatus result = GetChannelConfig(channel, config);
  if (result != DriverStatus::OK) {
    return result;
  }

  // Return user unit directly (VDR mode stores duty %)
  percent = config.hit_setpoint;
  return DriverStatus::OK;
}

template <typename SpiType>
DriverStatus MAX22200<SpiType>::GetHoldDutyPercent(uint8_t channel,
                                                   float &percent) const {
  if (!IsValidChannel(channel)) {
    updateStatistics(false);
    return DriverStatus::INVALID_PARAMETER;
  }

  ChannelConfig config;
  DriverStatus result = GetChannelConfig(channel, config);
  if (result != DriverStatus::OK) {
    return result;
  }

  // Return user unit directly (VDR mode stores duty %)
  percent = config.hold_setpoint;
  return DriverStatus::OK;
}

template <typename SpiType>
DriverStatus MAX22200<SpiType>::GetDutyLimits(uint8_t channel, DutyLimits &limits) const {
  if (!IsValidChannel(channel)) {
    return DriverStatus::INVALID_PARAMETER;
  }
  ChannelConfig config;
  DriverStatus result = GetChannelConfig(channel, config);
  if (result != DriverStatus::OK) {
    return result;
  }
  return GetDutyLimits(cached_status_.master_clock_80khz, config.chop_freq,
                       config.slew_rate_control_enabled, limits);
}

template <typename SpiType>
DriverStatus MAX22200<SpiType>::GetDutyLimits(bool master_clock_80khz, ChopFreq chop_freq,
                                               bool slew_rate_control_enabled, DutyLimits &limits) {
  // Compute fCHOP from FREQM and FREQ_CFG (datasheet Table 1)
  uint32_t fchop_khz;
  switch (chop_freq) {
    case ChopFreq::FMAIN_DIV4:
      fchop_khz = master_clock_80khz ? 20 : 25;
      break;
    case ChopFreq::FMAIN_DIV3:
      fchop_khz = master_clock_80khz ? 26 : 33;  // 26.66 -> 26, 33.33 -> 33
      break;
    case ChopFreq::FMAIN_DIV2:
      fchop_khz = master_clock_80khz ? 40 : 50;
      break;
    case ChopFreq::FMAIN:
      fchop_khz = master_clock_80khz ? 80 : 100;
      break;
    default:
      limits = DutyLimits();
      return DriverStatus::INVALID_PARAMETER;
  }

  // Datasheet Table 2: Min and Max Duty Cycle
  if (slew_rate_control_enabled) {
    // SRC = 1 (Slew-Rate Controlled)
    if (fchop_khz == 100 || fchop_khz == 80) {
      // FREQ_CFG = 11 with SRC=1: not valid per datasheet (SRC only for fCHOP < 50kHz)
      limits = DutyLimits();
      return DriverStatus::INVALID_PARAMETER;
    }
    // SRC=1: δMIN = 7%, δMAX = 93% for valid frequencies
    limits.min_percent = 7;
    limits.max_percent = 93;
  } else {
    // SRC = 0 (Fast Mode)
    if (fchop_khz == 100 || fchop_khz == 80) {
      // FREQ_CFG = 11: δMIN = 8%, δMAX = 92%
      limits.min_percent = 8;
      limits.max_percent = 92;
    } else {
      // Other FREQ_CFG: δMIN = 4%, δMAX = 96%
      limits.min_percent = 4;
      limits.max_percent = 96;
    }
  }

  return DriverStatus::OK;
}

// ============================================================================
// Convenience APIs: HIT Time in Milliseconds
// ============================================================================

template <typename SpiType>
DriverStatus MAX22200<SpiType>::SetHitTimeMs(uint8_t channel, float ms) {
  if (!IsValidChannel(channel)) {
    updateStatistics(false);
    return DriverStatus::INVALID_PARAMETER;
  }

  ChannelConfig config;
  DriverStatus result = GetChannelConfig(channel, config);
  if (result != DriverStatus::OK) {
    return result;
  }

  // Reject NaN/Inf
  if (!std::isfinite(ms)) {
    updateStatistics(false);
    return DriverStatus::INVALID_PARAMETER;
  }
  // Reject positive finite ms beyond 8-bit representable range (raw 1–254) for this channel's chop freq
  if (ms > 0.0f && ms > getMaxHitTimeMs(cached_status_.master_clock_80khz, config.chop_freq)) {
    updateStatistics(false);
    return DriverStatus::INVALID_PARAMETER;
  }

  config.hit_time_ms = ms;
  return ConfigureChannel(channel, config);
}

template <typename SpiType>
DriverStatus MAX22200<SpiType>::GetHitTimeMs(uint8_t channel, float &ms) const {
  if (!IsValidChannel(channel)) {
    updateStatistics(false);
    return DriverStatus::INVALID_PARAMETER;
  }

  ChannelConfig config;
  DriverStatus result = GetChannelConfig(channel, config);
  if (result != DriverStatus::OK) {
    return result;
  }

  // Return user unit directly
  ms = config.hit_time_ms;
  return DriverStatus::OK;
}

// ============================================================================
// Convenience APIs: One-Shot Channel Configuration
// ============================================================================

template <typename SpiType>
DriverStatus MAX22200<SpiType>::ConfigureChannelCdr(
    uint8_t channel, uint32_t hit_ma, uint32_t hold_ma, float hit_time_ms,
    SideMode side_mode, ChopFreq chop_freq, bool slew_rate_control_enabled,
    bool open_load_detection_enabled, bool plunger_movement_detection_enabled,
    bool hit_current_check_enabled) {
  if (!IsValidChannel(channel) || board_config_.full_scale_current_ma == 0) {
    updateStatistics(false);
    return DriverStatus::INVALID_PARAMETER;
  }

  ChannelConfig config;
  config.drive_mode = DriveMode::CDR;
  config.side_mode = side_mode;
  config.chop_freq = chop_freq;
  config.slew_rate_control_enabled = slew_rate_control_enabled;
  config.open_load_detection_enabled = open_load_detection_enabled;
  config.plunger_movement_detection_enabled = plunger_movement_detection_enabled;
  config.hit_current_check_enabled = hit_current_check_enabled;

  // Clamp to max_current_ma if set
  if (board_config_.max_current_ma > 0 && hit_ma > board_config_.max_current_ma) {
    hit_ma = board_config_.max_current_ma;
  }
  if (board_config_.max_current_ma > 0 && hold_ma > board_config_.max_current_ma) {
    hold_ma = board_config_.max_current_ma;
  }

  // Set user units directly (CDR mode: mA)
  config.hit_setpoint = static_cast<float>(hit_ma);
  config.hold_setpoint = static_cast<float>(hold_ma);
  config.hit_time_ms = hit_time_ms;

  return ConfigureChannel(channel, config);
}

template <typename SpiType>
DriverStatus MAX22200<SpiType>::ConfigureChannelVdr(
    uint8_t channel, float hit_duty_percent, float hold_duty_percent,
    float hit_time_ms, SideMode side_mode, ChopFreq chop_freq,
    bool slew_rate_control_enabled, bool open_load_detection_enabled,
    bool plunger_movement_detection_enabled, bool hit_current_check_enabled) {
  if (!IsValidChannel(channel)) {
    updateStatistics(false);
    return DriverStatus::INVALID_PARAMETER;
  }

  ChannelConfig config;
  config.drive_mode = DriveMode::VDR;
  config.side_mode = side_mode;
  config.chop_freq = chop_freq;
  config.slew_rate_control_enabled = slew_rate_control_enabled;
  config.open_load_detection_enabled = open_load_detection_enabled;
  config.plunger_movement_detection_enabled = plunger_movement_detection_enabled;
  config.hit_current_check_enabled = hit_current_check_enabled;

  // Clamp to max_duty_percent if set
  if (board_config_.max_duty_percent > 0 &&
      hit_duty_percent > board_config_.max_duty_percent) {
    hit_duty_percent = board_config_.max_duty_percent;
  }
  if (board_config_.max_duty_percent > 0 &&
      hold_duty_percent > board_config_.max_duty_percent) {
    hold_duty_percent = board_config_.max_duty_percent;
  }

  // Clamp to duty limits
  DutyLimits limits;
  DriverStatus result = GetDutyLimits(cached_status_.master_clock_80khz, chop_freq,
                                      slew_rate_control_enabled, limits);
  if (result != DriverStatus::OK) {
    return result;
  }
  if (hit_duty_percent < limits.min_percent) hit_duty_percent = limits.min_percent;
  if (hit_duty_percent > limits.max_percent) hit_duty_percent = limits.max_percent;
  if (hold_duty_percent < limits.min_percent) hold_duty_percent = limits.min_percent;
  if (hold_duty_percent > limits.max_percent) hold_duty_percent = limits.max_percent;

  // Set user units directly (VDR mode: duty %)
  config.hit_setpoint = hit_duty_percent;
  config.hold_setpoint = hold_duty_percent;
  config.hit_time_ms = hit_time_ms;

  return ConfigureChannel(channel, config);
}

// ============================================================================
// Private: Statistics
// ============================================================================

template <typename SpiType>
void MAX22200<SpiType>::updateStatistics(bool success) const {
  statistics_.total_transfers++;
  if (!success) {
    statistics_.failed_transfers++;
  }
}

} // namespace max22200

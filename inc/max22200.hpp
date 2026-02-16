/**
 * @file max22200.hpp
 * @brief Main driver class for MAX22200 octal solenoid and motor driver
 *
 * This file provides the main MAX22200 driver class template that implements
 * the two-phase SPI protocol per MAX22200 datasheet (Rev 1, 3/25, Document 19-100531).
 *
 * ## Two-Phase SPI Protocol
 *
 * The MAX22200 uses a **two-phase SPI protocol** for all register access:
 *
 * ### Phase 1: Command Register Write
 * - **CMD pin HIGH** (must be held HIGH during entire transfer)
 * - Write 8-bit Command Register (1 byte SPI transfer)
 * - Device responds with STATUS[7:0] (Fault Flag Byte) on SDO
 * - Check for communication error: STATUS[7:0] = 0x04 (COMER flag)
 *
 * ### Phase 2: Data Transfer
 * - **CMD pin LOW** (must be LOW for data transfer)
 * - Read or write data register:
 *   - **8-bit mode**: Transfer 1 byte (MSB only, for fast updates)
 *   - **32-bit mode**: Transfer 4 bytes (full register)
 *
 * ## Initialization Sequence
 *
 * Per datasheet Figure 6 (Programming Flow Chart):
 *
 * 1. **Power-up**: Set ENABLE pin HIGH, wait 0.5ms (tEN)
 * 2. **Read STATUS**: Clear UVM flag and deassert nFAULT pin
 * 3. **Write STATUS**: Set ACTIVE=1, configure HW (CMxy bits), set fault masks
 * 4. **Configure Channels**: Write CFG_CHx for each channel (HIT/HOLD currents, timing, etc.)
 * 5. **Read STATUS**: Verify UVM cleared and no faults
 * 6. **Ready**: Channels can now be activated via ONCH bits
 *
 * ## Timing Specifications
 *
 * - **Enable time (tEN)**: 0.5ms from ENABLE rising edge to SPI ready
 * - **Wake-up time (tWU)**: 2.5ms from ACTIVE=1 to normal operation (OUT_ active)
 * - **Disable time (tDIS)**: 2.5ms from ENABLE falling edge to OUT_ tristate
 * - **Dead time (tDEAD)**: 200ns dead zone to prevent current feedthrough
 * - **SPI clock period (tCLK)**: Minimum 100ns (max 10MHz standalone, 5MHz daisy-chain)
 *
 * ## Restrictions and Limitations
 *
 * - **VDRnCDR and HSnLS bits** can only be modified when:
 *   - All channels are OFF (ONCHx = 0 for all channels)
 *   - Both TRIGA and TRIGB inputs are logic-low
 *
 * - **CDR mode** is only supported in **low-side** operation (HSnLS = 0)
 *
 * - **High-side mode** (HSnLS = 1) only supports **VDR mode**
 *
 * - **Channel-pair mode (CMxy)** can only be changed when both channels in the pair are OFF
 *
 * - **HFS, SRC, and DPM** are only available for **low-side** applications
 *
 * ## Fast 8-Bit Updates
 *
 * For low-latency control, certain operations can use 8-bit MSB-only transfers:
 *
 * - **Channel ON/OFF**: Write STATUS[31:24] (ONCH byte) — fast channel activation
 * - **HOLD current update**: Write CFG_CHx[31:24] (HOLD + HFS) — dynamic current control
 * - **OCP status**: Read FAULT[31:24] (OCP byte) — quick fault check
 *
 * @note All SPI transfers must use **Mode 0** (CPOL=0, CPHA=0).
 * @note The CMD pin must be held HIGH during the rising edge of CSB for Command Register writes.
 * @note Communication errors (COMER) are detected by checking STATUS[7:0] = 0x04 after Command Register write.
 *
 * @copyright Copyright (c) 2024-2025 HardFOC. All rights reserved.
 */
#pragma once
#include "max22200_registers.hpp"
#include "max22200_types.hpp"
#include "max22200_spi_interface.hpp"
#include <array>
#include <cstdint>

namespace max22200 {

/**
 * @brief Main driver class for MAX22200 IC
 *
 * This class provides a high-level API for controlling the MAX22200 octal
 * solenoid and motor driver. It handles the two-phase SPI protocol internally
 * and provides convenient methods for channel configuration, control, and fault monitoring.
 *
 * @tparam SpiType The SPI interface implementation type that inherits from
 *                 max22200::SpiInterface<SpiType>. Must implement:
 *                 - Transfer() for SPI data transfer
 *                 - GpioSet() for CMD/ENABLE/FAULT pin control
 *                 - GpioGet() for FAULT pin reading
 *
 * @example Basic usage (CDR mode):
 * @code
 * MySPI spi;
 * MAX22200<MySPI> driver(spi);
 *
 * if (driver.Initialize() == DriverStatus::OK) {
 *     ChannelConfig config;
 *     config.drive_mode = DriveMode::CDR;      // Current Drive Regulation
 *     config.side_mode = SideMode::LOW_SIDE;    // Low-side only for CDR
 *     config.hit_current = 80;                  // 7-bit (0-127), IHIT = 80/127 × IFS
 *     config.hold_current = 40;                 // 7-bit (0-127), IHOLD = 40/127 × IFS
 *     config.hit_time = 100;                    // 8-bit (0-255), tHIT = 100 × 40 / fCHOP
 *     config.chop_freq = ChopFreq::FMAIN_DIV2; // 50kHz (if FREQM=0)
 *     config.hit_current_check_enabled = true;  // Enable HIT current check
 *
 *     driver.ConfigureChannel(0, config);
 *     driver.EnableChannel(0);
 * }
 * @endcode
 *
 * @example Fast HOLD current update (8-bit mode):
 * @code
 * // Update HOLD current on-the-fly while channel is operating (CFG_CH MSB = HFS | HOLD[6:0])
 * ChannelConfig config;
 * driver.GetChannelConfig(0, config);
 * config.hold_current = 60;  // New HOLD current
 * driver.WriteRegister8(RegBank::CFG_CH0, (config.half_full_scale ? 0x80u : 0u) | (config.hold_current & 0x7Fu));
 * @endcode
 *
 * @example Channel-pair configuration (parallel mode):
 * @code
 * StatusConfig status;
 * driver.ReadStatus(status);
 * status.channel_pair_mode_10 = ChannelMode::PARALLEL;  // Channels 0-1 in parallel
 * status.channels_on_mask = 0;  // Both channels must be OFF
 * driver.WriteStatus(status);
 * // Now configure channel 0 (channel 1 config is ignored)
 * @endcode
 */
template <typename SpiType> class MAX22200 {
public:
  /**
   * @brief Constructor (SPI only; set board config later with SetBoardConfig())
   *
   * Use when board config is unknown at construction (e.g. loaded from NVS or
   * another source after the driver is created). Unit APIs (SetHitCurrentMa,
   * ConfigureChannelCdr, etc.) require IFS to be set and will return
   * INVALID_PARAMETER if full_scale_current_ma is 0.
   *
   * @param spi_interface Reference to SPI interface implementation
   */
  explicit MAX22200(SpiType &spi_interface);

  /**
   * @brief Constructor with board config (SPI + IFS/limits)
   *
   * Use when the board is known at construction (e.g. fixed RREF). The driver
   * is immediately valid for unit-based APIs without calling SetBoardConfig().
   *
   * @param spi_interface Reference to SPI interface implementation
   * @param board_config   Board configuration (IFS from RREF, optional limits)
   */
  MAX22200(SpiType &spi_interface, const BoardConfig &board_config);

  /**
   * @brief Destructor — calls Deinitialize() if initialized
   */
  ~MAX22200();

  // Disable copy
  MAX22200(const MAX22200 &) = delete;
  MAX22200 &operator=(const MAX22200 &) = delete;
  MAX22200(MAX22200 &&) = default;
  MAX22200 &operator=(MAX22200 &&) = delete;

  // =========================================================================
  // Initialization
  // =========================================================================

  /**
   * @brief Initialize the driver per datasheet flowchart (Figure 6)
   *
   * Performs the complete initialization sequence as specified in the MAX22200
   * datasheet Programming Flow Chart:
   *
   * 1. **Initialize SPI**: Configure SPI interface to Mode 0 (CPOL=0, CPHA=0)
   * 2. **Power-up**: Set ENABLE pin HIGH, wait 0.5ms (tEN) for SPI ready
   * 3. **Read STATUS**: Clear UVM flag and deassert nFAULT pin
   *    - Checks for communication error (STATUS[7:0] = 0x04)
   *    - If COMER detected, initialization fails
   * 4. **Write STATUS**: Set ACTIVE=1, configure default HW settings
   *    - All channels OFF (ONCH = 0)
   *    - All channel pairs in INDEPENDENT mode
   *    - Fault masks set to defaults (M_COMF = 1, others = 0)
   * 5. **Cache STATUS**: Store STATUS for fast ONCH updates
   *
   * @return DriverStatus::OK on success
   * @return DriverStatus::COMMUNICATION_ERROR if COMER flag detected
   * @return DriverStatus::INITIALIZATION_ERROR if SPI or GPIO operations fail
   *
   * @note After Initialize(), channels must be configured with ConfigureChannel()
   *       before they can be enabled.
   * @note The device enters normal operation after ACTIVE=1 with a wake-up time
   *       of 2.5ms (tWU). Channels can be enabled immediately after Initialize().
   *
   * @see MAX22200 datasheet Figure 6: Programming Flow Chart
   */
  DriverStatus Initialize();

  /**
   * @brief Deinitialize — disable channels, ACTIVE=0, ENABLE low
   *
   * Performs a clean shutdown sequence:
   * 1. Disable all channels (ONCH = 0)
   * 2. Set ACTIVE=0 (device enters low-power mode)
   * 3. Set ENABLE pin LOW (device enters sleep mode)
   *
   * After Deinitialize(), the device consumes < 11μA from VM.
   *
   * @return DriverStatus::OK on success
   *
   * @note All channels are three-stated when ACTIVE=0.
   * @note It takes 2.5ms (tDIS) from ENABLE falling edge to OUT_ tristate.
   */
  DriverStatus Deinitialize();

  /**
   * @brief Check if driver is initialized
   */
  bool IsInitialized() const;

  // =========================================================================
  // STATUS Register Operations
  // =========================================================================

  /**
   * @brief Read the full 32-bit STATUS register
   */
  DriverStatus ReadStatus(StatusConfig &status) const;

  /**
   * @brief Write the STATUS register (writable bits only)
   */
  DriverStatus WriteStatus(const StatusConfig &status);

  // =========================================================================
  // Channel Configuration (CFG_CHx)
  // =========================================================================

  /**
   * @brief Configure a channel (write full 32-bit CFG_CHx register)
   *
   * Writes the complete 32-bit configuration register for the specified channel.
   * This includes all drive parameters: HIT/HOLD currents, timing, drive mode, etc.
   *
   * @param channel Channel number (0-7)
   * @param config Channel configuration structure
   * @return DriverStatus::OK on success
   * @return DriverStatus::INVALID_PARAMETER if channel >= 8
   *
   * @note For channels in PARALLEL mode, only the lower channel number's
   *       configuration is used (e.g., configure channel 0 for pair 0-1).
   * @note For channels in HBRIDGE mode, both channels' configurations are used
   *       (CFG_CHy for forward, CFG_CHx for reverse).
   * @note VDRnCDR and HSnLS bits can only be changed when all channels are OFF
   *       and both TRIGA and TRIGB are logic-low.
   * @note FREQ_CFG and SRC can be changed "on-the-fly" while channel is operating.
   *
   * @see ChannelConfig for detailed field descriptions
   */
  DriverStatus ConfigureChannel(uint8_t channel, const ChannelConfig &config);

  /**
   * @brief Read a channel's configuration
   * @param channel Channel number (0-7)
   * @param config Reference to store configuration
   */
  DriverStatus GetChannelConfig(uint8_t channel, ChannelConfig &config) const;

  /**
   * @brief Configure all channels
   */
  DriverStatus ConfigureAllChannels(const ChannelConfigArray &configs);

  /**
   * @brief Get all channel configurations
   */
  DriverStatus GetAllChannelConfigs(ChannelConfigArray &configs) const;

  // =========================================================================
  // Channel Enable/Disable (ONCH bits in STATUS register)
  // =========================================================================

  /**
   * @brief Turn on a channel (set ONCHx = 1)
   *
   * Sets the ONCHx bit so the channel is active (delivering current).
   *
   * @param channel Channel number (0-7)
   * @return DriverStatus::OK on success
   * @return DriverStatus::INVALID_PARAMETER if channel >= 8
   *
   * @note Use DisableChannel() to turn off. For multiple channels use SetChannelsOn().
   * @note Channel must be configured with ConfigureChannel() before enabling.
   */
  DriverStatus EnableChannel(uint8_t channel);

  /**
   * @brief Turn off a channel (set ONCHx = 0)
   * @param channel Channel number (0-7)
   * @return DriverStatus::OK on success
   * @return DriverStatus::INVALID_PARAMETER if channel >= 8
   */
  DriverStatus DisableChannel(uint8_t channel);

  /**
   * @brief Set a channel on or off (convenience when toggling from a variable)
   * @param channel Channel number (0-7)
   * @param enable  true = on, false = off
   * @return DriverStatus::OK on success
   */
  DriverStatus SetChannelEnabled(uint8_t channel, bool enable);

  /**
   * @brief Turn on all channels
   * @return DriverStatus::OK on success
   */
  DriverStatus EnableAllChannels();

  /**
   * @brief Turn off all channels
   * @return DriverStatus::OK on success
   */
  DriverStatus DisableAllChannels();

  /**
   * @brief Set all channels on or off at once (convenience when toggling from a variable)
   * @param enable true = all on, false = all off
   * @return DriverStatus::OK on success
   */
  DriverStatus SetAllChannelsEnabled(bool enable);

  /**
   * @brief Set which channels are on (fast multi-channel update)
   *
   * Updates all eight ONCH bits in one 8-bit write. Bit N = 1 means channel N
   * is on; 0 means off. Use this instead of multiple EnableChannel() calls when
   * updating several channels at once.
   *
   * @param channel_mask Bitmask: bit 0 = channel 0, bit 1 = channel 1, ... bit 7 = channel 7
   * @return DriverStatus::OK on success
   *
   * @example Turn on channels 0, 2, and 5:
   * @code
   * driver.SetChannelsOn((1u << 0) | (1u << 2) | (1u << 5));
   * @endcode
   */
  DriverStatus SetChannelsOn(uint8_t channel_mask);

  /**
   * @brief Set full-bridge state for a channel pair (datasheet Table 7)
   *
   * Updates ONCHx and ONCHy for the given pair to achieve HiZ, Forward,
   * Reverse, or Brake. Other channels' ONCH bits are preserved.
   *
   * @param pair_index Pair index 0–3 (0 = ch0–ch1, 1 = ch2–ch3, 2 = ch4–ch5, 3 = ch6–ch7)
   * @param state     FullBridgeState::HiZ, Forward, Reverse, or Brake
   * @return DriverStatus::OK on success
   * @return DriverStatus::INVALID_PARAMETER if pair_index > 3
   *
   * @note The pair must be configured as HBRIDGE in STATUS (CMxy = 10).
   * @see FullBridgeState, StatusConfig::cm10, ChannelMode::HBRIDGE
   */
  DriverStatus SetFullBridgeState(uint8_t pair_index, FullBridgeState state);

  // =========================================================================
  // Fault Operations
  // =========================================================================

  /**
   * @brief Read per-channel fault register (FAULT)
   *
   * Reads the FAULT register (OCP, HHF, OLF, DPM per channel). Reading also
   * clears the flags and deasserts nFAULT when no other faults remain.
   *
   * @param faults Reference to FaultStatus to populate
   * @return DriverStatus::OK on success
   *
   * @see ClearAllFaults() to clear without keeping the result
   * @see ClearChannelFaults() to clear only specific channels (MAX22200A)
   */
  DriverStatus ReadFaultRegister(FaultStatus &faults) const;

  /**
   * @brief Clear all fault flags (read FAULT register and discard)
   *
   * Clears OCP, HHF, OLF, and DPM for all channels and deasserts nFAULT.
   * Use when you only need to clear and do not need the fault snapshot.
   *
   * @return DriverStatus::OK on success
   */
  DriverStatus ClearAllFaults();

  /**
   * @brief Clear fault flags for selected channels only (MAX22200A)
   *
   * On MAX22200A, clears OCP/HHF/OLF/DPM only for channels whose bit is set
   * in channel_mask. On MAX22200, the device clears all flags on any read
   * (same as ClearAllFaults).
   *
   * @param channel_mask Bit N = 1 to clear faults for channel N (0–7)
   * @param out_faults  Optional: if non-null, filled with FAULT state after clear
   * @return DriverStatus::OK on success
   */
  DriverStatus ClearChannelFaults(uint8_t channel_mask, FaultStatus *out_faults = nullptr) const;

  /**
   * @brief [Advanced] Read FAULT register with per-type selective clear (MAX22200A)
   *
   * For MAX22200A only: separate masks for OCP, HHF, OLF, DPM let you clear
   * only certain fault types per channel. On MAX22200, SDI is ignored and
   * all flags are cleared. Prefer ClearAllFaults() or ClearChannelFaults()
   * for normal use.
   *
   * @param ocp_mask  Channel mask for OCP bits to clear (bit N = channel N)
   * @param hhf_mask  Channel mask for HHF bits to clear
   * @param olf_mask  Channel mask for OLF bits to clear
   * @param dpm_mask  Channel mask for DPM bits to clear
   * @param faults    Populated with FAULT register value after the read
   * @return DriverStatus::OK on success
   */
  DriverStatus ReadFaultRegisterSelectiveClear(uint8_t ocp_mask,
                                              uint8_t hhf_mask,
                                              uint8_t olf_mask,
                                              uint8_t dpm_mask,
                                              FaultStatus &faults) const;

  /**
   * @brief Read fault flags from STATUS register
   */
  DriverStatus ReadFaultFlags(StatusConfig &status) const;

  /**
   * @brief Clear fault flags by reading STATUS register
   */
  DriverStatus ClearFaultFlags();

  // =========================================================================
  // DPM Configuration (CFG_DPM, 0x0A)
  // =========================================================================

  /**
   * @brief Configure DPM in real units (easy API)
   *
   * Sets start current, dip threshold, and debounce time. Uses board IFS for
   * current conversion and a default 25 kHz chopping frequency for debounce.
   * Call SetBoardConfig() first so IFS is set.
   *
   * @param start_current_ma  Current (mA) above which DPM monitors for dip
   * @param dip_threshold_ma Minimum dip amplitude (mA) to count as valid
   * @param debounce_ms      Min dip duration (ms); converted to chopping periods at 25 kHz
   * @return DriverStatus::OK on success
   * @return DriverStatus::INVALID_PARAMETER if IFS not set or values out of range
   *
   * @note For custom fCHOP or raw register control use ReadDpmConfig/WriteDpmConfig.
   */
  DriverStatus ConfigureDpm(float start_current_ma, float dip_threshold_ma,
                           float debounce_ms);

  /**
   * @brief Read DPM algorithm configuration (CFG_DPM register)
   *
   * DPM settings are global and apply to all channels that have DPM_EN set.
   *
   * @param config Reference to DpmConfig to populate
   * @return DriverStatus::OK on success
   *
   * @see DpmConfig, WriteDpmConfig, ConfigureDpm
   */
  DriverStatus ReadDpmConfig(DpmConfig &config) const;

  /**
   * @brief Write DPM algorithm configuration (CFG_DPM register)
   *
   * @param config DPM configuration (plunger_movement_start_current, plunger_movement_debounce_time, plunger_movement_current_threshold)
   * @return DriverStatus::OK on success
   *
   * @see DpmConfig, ReadDpmConfig, ConfigureDpm
   */
  DriverStatus WriteDpmConfig(const DpmConfig &config);

  // =========================================================================
  // Device Control
  // =========================================================================

  /**
   * @brief Enable device (ENABLE pin high); SPI and channels can be used
   */
  DriverStatus EnableDevice();

  /**
   * @brief Disable device (ENABLE pin low); low-power state
   */
  DriverStatus DisableDevice();

  /**
   * @brief Set ENABLE pin state (true = on, false = off)
   *
   * Prefer EnableDevice() / DisableDevice() for clarity.
   */
  DriverStatus SetDeviceEnable(bool enable);

  /**
   * @brief Read nFAULT pin state (true = fault active, false = no fault)
   */
  DriverStatus GetFaultPinState(bool &fault_active) const;

  // =========================================================================
  // Raw Register Access (for debug)
  // =========================================================================

  /**
   * @brief Read a 32-bit register by bank address
   */
  DriverStatus ReadRegister32(uint8_t bank, uint32_t &value) const;

  /**
   * @brief Write a 32-bit register by bank address
   */
  DriverStatus WriteRegister32(uint8_t bank, uint32_t value);

  /**
   * @brief Read 8-bit MSB of a register (fast 8-bit mode)
   */
  DriverStatus ReadRegister8(uint8_t bank, uint8_t &value) const;

  /**
   * @brief Write 8-bit MSB of a register (fast 8-bit mode)
   */
  DriverStatus WriteRegister8(uint8_t bank, uint8_t value);

  /**
   * @brief Get last fault flags byte received from Command Register write
   *
   * Every time the Command Register is written (Phase 1 of SPI protocol),
   * the device responds with STATUS[7:0] (Fault Flag Byte) on SDO. This method
   * returns the most recent value received.
   *
   * @return STATUS[7:0] byte from last Command Register write
   *
   * @note Check for communication error: return value = 0x04 (COMER flag)
   * @note This byte contains: OVT, OCP, OLF, HHF, DPM, COMER, UVM, ACTIVE bits
   *
   * @example Check for communication error:
   * @code
   * uint8_t fault_byte = driver.GetLastFaultByte();
   * if (fault_byte == 0x04) {
   *     // Communication error detected
   * }
   * @endcode
   */
  uint8_t GetLastFaultByte() const;

  // =========================================================================
  // Statistics
  // =========================================================================

  DriverStatistics GetStatistics() const;
  void ResetStatistics();

  // =========================================================================
  // Callbacks
  // =========================================================================

  void SetFaultCallback(FaultCallback callback, void *user_data);
  void SetStateChangeCallback(StateChangeCallback callback, void *user_data);

  // =========================================================================
  // Board/Scale Configuration (for unit-based APIs)
  // =========================================================================

  /**
   * @brief Set board configuration (IFS, max current, max duty)
   *
   * Configures the full-scale current and optional safety limits used by
   * convenience APIs (SetHitCurrentMa, SetHitDutyPercent, etc.). Can be
   * called at any time to change limits; IFS is required for unit-based APIs.
   *
   * @param config Board configuration (IFS in mA, optional max limits)
   *
   * @note Alternatively pass BoardConfig at construction:
   *       MAX22200 driver(spi, BoardConfig(30.0f, false));
   */
  void SetBoardConfig(const BoardConfig &config);

  /**
   * @brief Get current board configuration
   */
  BoardConfig GetBoardConfig() const;

  // =========================================================================
  // Convenience APIs: Current in Real Units (CDR Mode)
  // =========================================================================

  /**
   * @brief Set HIT current in milliamps (CDR mode)
   *
   * Converts mA to 7-bit register value: raw = round((ma / IFS_ma) × 127).
   * Clamps to max_current_ma if set, then to 0-127.
   *
   * @param channel Channel number (0-7)
   * @param ma     Current in milliamps
   * @return DriverStatus::OK on success
   * @return DriverStatus::INVALID_PARAMETER if channel >= 8 or IFS not configured
   */
  DriverStatus SetHitCurrentMa(uint8_t channel, uint32_t ma);

  /**
   * @brief Set HOLD current in milliamps (CDR mode)
   */
  DriverStatus SetHoldCurrentMa(uint8_t channel, uint32_t ma);

  /**
   * @brief Set HIT current in Amps (CDR mode)
   *
   * Convenience wrapper: converts A to mA and calls SetHitCurrentMa.
   */
  DriverStatus SetHitCurrentA(uint8_t channel, float amps);

  /**
   * @brief Set HOLD current in Amps (CDR mode)
   */
  DriverStatus SetHoldCurrentA(uint8_t channel, float amps);

  /**
   * @brief Set HIT current as percentage of IFS (CDR mode)
   *
   * @param channel Channel number (0-7)
   * @param percent Percentage (0-100)
   * @return DriverStatus::OK on success
   */
  DriverStatus SetHitCurrentPercent(uint8_t channel, float percent);

  /**
   * @brief Set HOLD current as percentage of IFS (CDR mode)
   */
  DriverStatus SetHoldCurrentPercent(uint8_t channel, float percent);

  /**
   * @brief Get HIT current in milliamps (CDR mode)
   *
   * Reads channel config and converts: ma = (hit_current / 127.0) × IFS_ma
   */
  DriverStatus GetHitCurrentMa(uint8_t channel, uint32_t &ma) const;

  /**
   * @brief Get HOLD current in milliamps (CDR mode)
   */
  DriverStatus GetHoldCurrentMa(uint8_t channel, uint32_t &ma) const;

  /**
   * @brief Get HIT current as percentage of IFS (CDR mode)
   */
  DriverStatus GetHitCurrentPercent(uint8_t channel, float &percent) const;

  /**
   * @brief Get HOLD current as percentage of IFS (CDR mode)
   */
  DriverStatus GetHoldCurrentPercent(uint8_t channel, float &percent) const;

  // =========================================================================
  // Convenience APIs: Duty Cycle in Percent (VDR Mode)
  // =========================================================================

  /**
   * @brief Set HIT duty cycle in percent (VDR mode)
   *
   * Converts percent to 7-bit: raw = round((pct / 100) × 127).
   * Clamps to max_duty_percent if set, then to [δMIN, δMAX] based on FREQM,
   * FREQ_CFG, and SRC settings.
   *
   * @param channel Channel number (0-7)
   * @param percent Duty cycle in percent (0-100)
   * @return DriverStatus::OK on success
   */
  DriverStatus SetHitDutyPercent(uint8_t channel, float percent);

  /**
   * @brief Set HOLD duty cycle in percent (VDR mode)
   */
  DriverStatus SetHoldDutyPercent(uint8_t channel, float percent);

  /**
   * @brief Get HIT duty cycle in percent (VDR mode)
   *
   * Reads channel config: percent = (hit_current / 127.0) × 100
   */
  DriverStatus GetHitDutyPercent(uint8_t channel, float &percent) const;

  /**
   * @brief Get HOLD duty cycle in percent (VDR mode)
   */
  DriverStatus GetHoldDutyPercent(uint8_t channel, float &percent) const;

  /**
   * @brief Get duty cycle limits (δMIN, δMAX) for a configuration
   *
   * Returns the minimum and maximum duty cycle percentages based on FREQM,
   * FREQ_CFG, and SRC settings (datasheet Table 2).
   *
   * @param master_clock_80khz Master clock 80 kHz base (from STATUS register)
   * @param chop_freq Chopping frequency divider
   * @param slew_rate_control_enabled Slew rate control enabled
   * @param limits    Output: min_percent and max_percent
   * @return DriverStatus::OK on success
   */
  static DriverStatus GetDutyLimits(bool master_clock_80khz, ChopFreq chop_freq,
                                    bool slew_rate_control_enabled, DutyLimits &limits);

  // =========================================================================
  // Convenience APIs: HIT Time in Milliseconds
  // =========================================================================

  /**
   * @brief Set HIT time in milliseconds
   *
   * Converts ms to 8-bit register value: HIT_T = round((ms / 1000) × fCHOP / 40).
   * Needs FREQM (from STATUS) and channel's chop_freq to compute fCHOP.
   *
   * @param channel Channel number (0-7)
   * @param ms     HIT time in milliseconds (0 = no HIT, -1 or 0xFFFF = continuous)
   * @return DriverStatus::OK on success
   */
  DriverStatus SetHitTimeMs(uint8_t channel, float ms);

  /**
   * @brief Get HIT time in milliseconds
   *
   * Reads channel config and STATUS (FREQM): ms = (HIT_T × 40 / fCHOP) × 1000
   */
  DriverStatus GetHitTimeMs(uint8_t channel, float &ms) const;

  // =========================================================================
  // Convenience APIs: One-Shot Channel Configuration
  // =========================================================================

  /**
   * @brief Configure channel in real units (CDR mode)
   *
   * Convenience method that sets all channel parameters using real units
   * (mA, ms) instead of raw register values.
   * Units: hit_ma (mA), hold_ma (mA), hit_time_ms (ms).
   *
   * @param channel      Channel number (0-7)
   * @param hit_ma       HIT current in milliamps
   * @param hold_ma      HOLD current in milliamps
   * @param hit_time_ms  HIT time in milliseconds (-1 = continuous)
   * @param side_mode    Low-side or high-side (default: LOW_SIDE)
   * @param chop_freq    Chopping frequency (default: FMAIN_DIV4)
   * @param slew_rate_control_enabled Slew rate control (default: false)
   * @param open_load_detection_enabled Open load detection (default: false)
   * @param plunger_movement_detection_enabled DPM detection (default: false)
   * @param hit_current_check_enabled HIT current check (default: false)
   * @return DriverStatus::OK on success
   */
  DriverStatus ConfigureChannelCdr(uint8_t channel, uint32_t hit_ma,
                                   uint32_t hold_ma, float hit_time_ms,
                                   SideMode side_mode = SideMode::LOW_SIDE,
                                   ChopFreq chop_freq = ChopFreq::FMAIN_DIV4,
                                   bool slew_rate_control_enabled = false,
                                   bool open_load_detection_enabled = false,
                                   bool plunger_movement_detection_enabled = false,
                                   bool hit_current_check_enabled = false);

  /**
   * @brief Configure channel in real units (VDR mode)
   *
   * Convenience method for VDR mode using duty cycle percentages.
   * Units: hit_duty_percent (%), hold_duty_percent (%), hit_time_ms (ms).
   *
   * @param channel         Channel number (0-7)
   * @param hit_duty_percent HIT duty cycle in percent (0-100)
   * @param hold_duty_percent HOLD duty cycle in percent (0-100)
   * @param hit_time_ms      HIT time in milliseconds (-1 = continuous)
   * @param side_mode        Low-side or high-side
   * @param chop_freq        Chopping frequency
   * @param slew_rate_control_enabled Slew rate control
   * @param open_load_detection_enabled Open load detection
   * @param plunger_movement_detection_enabled DPM detection (low-side only)
   * @param hit_current_check_enabled HIT current check
   * @return DriverStatus::OK on success
   */
  DriverStatus ConfigureChannelVdr(uint8_t channel, float hit_duty_percent,
                                   float hold_duty_percent, float hit_time_ms,
                                   SideMode side_mode = SideMode::LOW_SIDE,
                                   ChopFreq chop_freq = ChopFreq::FMAIN_DIV4,
                                   bool slew_rate_control_enabled = false,
                                   bool open_load_detection_enabled = false,
                                   bool plunger_movement_detection_enabled = false,
                                   bool hit_current_check_enabled = false);

  // =========================================================================
  // Validation
  // =========================================================================

  static bool IsValidChannel(uint8_t channel) { return channel < NUM_CHANNELS_; }

private:
  SpiType &spi_interface_;
  bool initialized_;
  mutable DriverStatistics statistics_;
  mutable uint8_t last_fault_byte_;  ///< STATUS[7:0] from last Command Reg write
  StatusConfig cached_status_;       ///< Cached STATUS for ONCH updates
  BoardConfig board_config_;         ///< Board configuration (IFS, max limits)

  FaultCallback fault_callback_;
  void *fault_user_data_;
  StateChangeCallback state_callback_;
  void *state_user_data_;

  // ── Core SPI protocol (two-phase) ──────────────────────────────────────

  /**
   * @brief Write the 8-bit Command Register (Phase 1 of two-phase SPI protocol)
   *
   * Performs Phase 1 of the MAX22200 SPI protocol:
   * 1. Set CMD pin HIGH (must be HIGH during entire transfer)
  2. Transfer 1 byte (Command Register)
   * 3. Device responds with STATUS[7:0] on SDO (Fault Flag Byte)
   * 4. Store STATUS[7:0] in last_fault_byte_ for diagnostic purposes
   * 5. Set CMD pin LOW (ready for Phase 2)
   *
   * @param bank      Register bank address (A_BNK, 0x00-0x0A)
   *                  - 0x00: STATUS
   *                  - 0x01-0x08: CFG_CH0 through CFG_CH7
   *                  - 0x09: FAULT
   *                  - 0x0A: CFG_DPM
   * @param is_write  true for write operation (RB/W = 1), false for read (RB/W = 0)
   * @param mode8     true for 8-bit MSB only access, false for 32-bit full register
   * @return DriverStatus::OK on success
   * @return DriverStatus::COMMUNICATION_ERROR if SPI transfer fails
   *
   * @note The CMD pin must be held HIGH during the rising edge of CSB.
   * @note Check GetLastFaultByte() after this call to detect communication errors
   *       (STATUS[7:0] = 0x04 indicates COMER).
   * @note After calling this, call writeData32/readData32 or writeData8/readData8
   *       to complete Phase 2 of the protocol.
   *
   * @see MAX22200 datasheet section "Command Register Description (COMMAND)"
   */
  DriverStatus writeCommandRegister(uint8_t bank, bool is_write,
                                    bool mode8 = false) const;

  /**
   * @brief Write 32-bit data register (Phase 2, after Command Register)
   */
  DriverStatus writeData32(uint32_t value) const;

  /**
   * @brief Read 32-bit data register (Phase 2, after Command Register)
   */
  DriverStatus readData32(uint32_t &value) const;

  /**
   * @brief Read 32-bit data register with custom TX (Phase 2). Used for
   *        MAX22200A selective fault clear (SDI bits select which bits to clear).
   */
  DriverStatus readData32WithTx(const uint8_t tx[4], uint32_t &value) const;

  /**
   * @brief Write 8-bit data register (Phase 2, after Command Register)
   */
  DriverStatus writeData8(uint8_t value) const;

  /**
   * @brief Read 8-bit data register (Phase 2, after Command Register)
   */
  DriverStatus readData8(uint8_t &value) const;

  // ── Convenience wrappers ───────────────────────────────────────────────

  /**
   * @brief Full 32-bit register write (Command Register + data)
   */
  DriverStatus writeReg32(uint8_t bank, uint32_t value) const;

  /**
   * @brief Full 32-bit register read (Command Register + data)
   */
  DriverStatus readReg32(uint8_t bank, uint32_t &value) const;

  /**
   * @brief Full 8-bit MSB register write (Command Register + data)
   */
  DriverStatus writeReg8(uint8_t bank, uint8_t value) const;

  /**
   * @brief Full 8-bit MSB register read (Command Register + data)
   */
  DriverStatus readReg8(uint8_t bank, uint8_t &value) const;

  void updateStatistics(bool success) const;
};

} // namespace max22200

// Include implementation
#include "../src/max22200.ipp"

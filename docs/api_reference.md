# API Reference

Reference for the MAX22200 driver public API. For register-level detail see `max22200_registers.hpp` and the datasheet.

## Source Code

- **Main header**: [`inc/max22200.hpp`](../inc/max22200.hpp)
- **SPI interface**: [`inc/max22200_spi_interface.hpp`](../inc/max22200_spi_interface.hpp)
- **Types**: [`inc/max22200_types.hpp`](../inc/max22200_types.hpp)

## Core Class: `MAX22200<SpiType>`

Template parameter: `SpiType` — your SPI implementation (must inherit `max22200::SpiInterface<SpiType>`).

**Constructor:** `explicit MAX22200(SpiType &spi_interface);`

---

## Methods

### Initialization

| Method | Description |
|--------|-------------|
| `Initialize()` | Full init per datasheet: ENABLE high, read STATUS (clear UVM), write STATUS (ACTIVE=1), cache STATUS |
| `Deinitialize()` | Disable all channels, ACTIVE=0, ENABLE low |
| `IsInitialized()` | Returns whether driver is initialized |

### STATUS Register

| Method | Description |
|--------|-------------|
| `ReadStatus(StatusConfig &status)` | Read 32-bit STATUS |
| `WriteStatus(const StatusConfig &status)` | Write STATUS (writable bits only) |

### Channel Configuration

| Method | Description |
|--------|-------------|
| `ConfigureChannel(uint8_t channel, const ChannelConfig &config)` | Write full CFG_CHx for channel |
| `GetChannelConfig(uint8_t channel, ChannelConfig &config)` | Read channel config |
| `ConfigureAllChannels(const ChannelConfigArray &configs)` | Configure all 8 channels |
| `GetAllChannelConfigs(ChannelConfigArray &configs)` | Read all channel configs |

### Channel Control

| Method | Description |
|--------|-------------|
| `EnableChannel(uint8_t channel)` | Set ONCH bit for channel |
| `DisableChannel(uint8_t channel)` | Clear ONCH bit |
| `SetChannelEnabled(uint8_t channel, bool enable)` | Turn channel on/off by bool |
| `EnableAllChannels()` | Set all ONCH bits |
| `DisableAllChannels()` | Clear all ONCH bits |
| `SetAllChannelsEnabled(bool enable)` | All channels on or off |
| `SetChannelsOn(uint8_t channel_mask)` | Set ONCH from bitmask (bit N = channel N) |
| `SetFullBridgeState(uint8_t pair_index, FullBridgeState state)` | Set HiZ/Forward/Reverse/Brake for pair 0–3 |

### Faults

| Method | Description |
|--------|-------------|
| `ReadFaultRegister(FaultStatus &faults)` | Read FAULT register (OCP/HHF/OLF/DPM per channel); read clears flags |
| `ClearAllFaults()` | Clear all fault flags (read FAULT, discard) |
| `ClearChannelFaults(uint8_t channel_mask, FaultStatus *out)` | Clear faults for selected channels (MAX22200A); optional snapshot |
| `ReadFaultRegisterSelectiveClear(...)` | Advanced: per-type clear masks (MAX22200A) |
| `ReadFaultFlags(StatusConfig &status)` | Read fault flags from STATUS |
| `ClearFaultFlags()` | Clear by reading STATUS |
| `GetFaultPinState(bool &fault_active)` | Read nFAULT pin |
| `GetLastFaultByte()` | STATUS[7:0] from last Command Register write (e.g. COMER = 0x04) |

### DPM

| Method | Description |
|--------|-------------|
| `ConfigureDpm(float start_current_ma, float dip_threshold_ma, float debounce_ms)` | Set DPM in mA and ms (uses board IFS) |
| `ReadDpmConfig(DpmConfig &config)` | Read CFG_DPM |
| `WriteDpmConfig(const DpmConfig &config)` | Write CFG_DPM |

### Device Control

| Method | Description |
|--------|-------------|
| `EnableDevice()` | ENABLE pin high |
| `DisableDevice()` | ENABLE pin low |
| `SetDeviceEnable(bool enable)` | Set ENABLE pin |

### Board and Convenience APIs

| Method | Description |
|--------|-------------|
| `SetBoardConfig(const BoardConfig &config)` | Set IFS and optional max current/duty limits |
| `GetBoardConfig()` | Get current board config |

**Current (CDR):**  
`SetHitCurrentMa`, `SetHoldCurrentMa`, `SetHitCurrentA`, `SetHoldCurrentA`, `SetHitCurrentPercent`, `SetHoldCurrentPercent`,  
`GetHitCurrentMa`, `GetHoldCurrentMa`, `GetHitCurrentPercent`, `GetHoldCurrentPercent`

**Duty (VDR):**  
`SetHitDutyPercent`, `SetHoldDutyPercent`, `GetHitDutyPercent`, `GetHoldDutyPercent`  
`GetDutyLimits(bool master_clock_80khz, ChopFreq chop_freq, bool slew_rate_control_enabled, DutyLimits &limits)` (static)

**HIT time:**  
`SetHitTimeMs(uint8_t channel, float ms)`, `GetHitTimeMs(uint8_t channel, float &ms)`

**One-shot config:**  
`ConfigureChannelCdr(channel, hit_ma, hold_ma, hit_time_ms, ...)`,  
`ConfigureChannelVdr(channel, hit_duty_percent, hold_duty_percent, hit_time_ms, ...)`

### Raw Registers (Debug)

| Method | Description |
|--------|-------------|
| `ReadRegister32(uint8_t bank, uint32_t &value)` | Read 32-bit register |
| `WriteRegister32(uint8_t bank, uint32_t value)` | Write 32-bit register |
| `ReadRegister8(uint8_t bank, uint8_t &value)` | Read 8-bit MSB |
| `WriteRegister8(uint8_t bank, uint8_t value)` | Write 8-bit MSB |

### Statistics and Callbacks

| Method | Description |
|--------|-------------|
| `GetStatistics()` | Return DriverStatistics (transfers, faults, uptime, etc.) |
| `ResetStatistics()` | Reset statistics |
| `SetFaultCallback(FaultCallback, void *user_data)` | Fault event callback |
| `SetStateChangeCallback(StateChangeCallback, void *user_data)` | State change callback |

### Validation

| Method | Description |
|--------|-------------|
| `IsValidChannel(uint8_t channel)` | Static: true if channel < 8 |

---

## Types (`max22200_types.hpp`)

### Enumerations

| Type | Values | Notes |
|------|--------|--------|
| `DriverStatus` | `OK`, `INITIALIZATION_ERROR`, `COMMUNICATION_ERROR`, `INVALID_PARAMETER`, `HARDWARE_FAULT`, `TIMEOUT` | Use `DriverStatusToStr(s)` for logging |
| `DriveMode` | `CDR`, `VDR` | Current vs voltage regulation |
| `SideMode` | `LOW_SIDE`, `HIGH_SIDE` | Load to VM vs GND |
| `ChannelMode` | `INDEPENDENT`, `PARALLEL`, `HBRIDGE` | Per pair (CM10, CM32, CM54, CM76) |
| `ChopFreq` | `FMAIN_DIV4`, `FMAIN_DIV3`, `FMAIN_DIV2`, `FMAIN` | Chopping frequency divider |
| `FaultType` | `OCP`, `HHF`, `OLF`, `DPM`, `OVT`, `UVM`, `COMER` | Use `FaultTypeToStr(ft)` for "Overcurrent", "HIT not reached", etc. |
| `FullBridgeState` | `HiZ`, `Forward`, `Reverse`, `Brake` | For H-bridge pairs |
| `ChannelState` | `DISABLED`, `ENABLED`, `HIT_PHASE`, `HOLD_PHASE`, `FAULT` | For state callbacks |

### Structures

| Type | Description |
|------|-------------|
| `ChannelConfig` | CFG_CHx in **user units**: hit_setpoint (mA for CDR, % for VDR), hold_setpoint, hit_time_ms; context: master_clock_80khz (IFS is not on ChannelConfig; driver uses BoardConfig IFS); register fields: drive_mode, side_mode, chop_freq, half_full_scale, trigger_from_pin, slew_rate_control_enabled, open_load_detection_enabled, plunger_movement_detection_enabled, hit_current_check_enabled. toRegister() computes raw from user units. fromRegister(val, board_ifs_ma, master_clock_80khz) fills user units (IFS not stored). Accessors: set_hit_ma/set_hold_ma, set_hit_duty_percent/set_hold_duty_percent, hit_ma()/hold_ma(), hit_duty_percent()/hold_duty_percent(). Helpers: isCdr(), isVdr(), isLowSide(), isHighSide(), hasHitTime(), isContinuousHit(), isHalfFullScale(), isSlewRateControlEnabled(), isOpenLoadDetectionEnabled(), isPlungerMovementDetectionEnabled(), isHitCurrentCheckEnabled(), getChopFreq(). |
| `StatusConfig` | STATUS: channels_on_mask, fault masks (overtemperature_masked, overcurrent_masked, …), master_clock_80khz, channel_pair_mode_10/32/54/76, active, fault flags (overtemperature, overcurrent, …). Helpers: `hasOvertemperature()`, `hasOvercurrent()`, `hasOpenLoadFault()`, `hasHitNotReached()`, `hasPlungerMovementFault()`, `hasCommunicationError()`, `hasUndervoltage()`, `isActive()`, `isChannelOn(ch)`, `channelCountOn()`, `isOvertemperatureMasked()`, … `getChannelPairMode10()` … `getChannelPairMode76()`, `is100KHzBase()`, `is80KHzBase()`, `getChannelsOnMask()`. |
| `FaultStatus` | FAULT: overcurrent_channel_mask, hit_not_reached_channel_mask, open_load_fault_channel_mask, plunger_movement_fault_channel_mask (per-channel masks). Helpers: `hasFault()`, `getFaultCount()`, `hasOvercurrent()`, `hasHitNotReached()`, `hasOpenLoadFault()`, `hasPlungerMovementFault()`, `hasFaultOnChannel(ch)`, `hasOvercurrentOnChannel(ch)`, … `channelsWithAnyFault()`. |
| `DpmConfig` | CFG_DPM: plunger_movement_start_current, plunger_movement_debounce_time, plunger_movement_current_threshold. Helpers: `getPlungerMovementStartCurrent()`, `getPlungerMovementDebounceTime()`, `getPlungerMovementCurrentThreshold()`. |
| `BoardConfig` | full_scale_current_ma, max_current_ma, max_duty_percent. Constructor `BoardConfig(rref_kohm, half_full_scale)` for IFS from RREF. Helpers: `hasMaxCurrentLimit()`, `hasMaxDutyLimit()`, `hasIfsConfigured()`, `getFullScaleCurrentMa()`, `getMaxCurrentLimitMa()`, `getMaxDutyLimitPercent()`. |
| `DutyLimits` | min_percent, max_percent. Helpers: `getMinPercent()`, `getMaxPercent()`, `inRange(percent)`, `clamp(percent)`. |
| `DriverStatistics` | total_transfers, failed_transfers, fault_events, state_changes, uptime_ms. Helpers: `getSuccessRate()`, `hasFailures()`, `isHealthy()`, `getTotalTransfers()`, … |

### Type Aliases

| Type | Definition |
|------|------------|
| `ChannelConfigArray` | `std::array<ChannelConfig, 8>` |
| `FaultCallback` | `void (*)(uint8_t channel, FaultType fault_type, void *user_data)` |
| `StateChangeCallback` | `void (*)(uint8_t channel, ChannelState old_state, ChannelState new_state, void *user_data)` |

### Helper Functions

| Function | Description |
|----------|-------------|
| `DriverStatusToStr(DriverStatus s)` | Human-readable status string |
| `FaultTypeToStr(FaultType ft)` | Human-readable fault name (e.g. "Overcurrent", "HIT not reached") |

---

**Navigation**
⬅️ [Configuration](configuration.md) | [Next: Examples ➡️](examples.md) | [Back to Index](index.md)

# API Reference

Complete reference documentation for all public methods and types in the MAX22200 driver.

## Source Code

- **Main Header**: [`inc/max22200.hpp`](../inc/max22200.hpp)
- **SPI Interface**: [`inc/max22200_spi_interface.hpp`](../inc/max22200_spi_interface.hpp)
- **Types**: [`inc/max22200_types.hpp`](../inc/max22200_types.hpp)
- **Implementation**: [`src/max22200.cpp`](../src/max22200.cpp)

## Core Class

### `MAX22200<SpiType>`

Main driver class for interfacing with the MAX22200 octal solenoid and motor driver.

**Template Parameter**: `SpiType` - Your SPI interface implementation (must inherit from `max22200::SpiInterface<SpiType>`)

**Location**: [`inc/max22200.hpp#L64`](../inc/max22200.hpp#L64)

**Constructor:**

```cpp
explicit MAX22200(SpiType &spi_interface, bool enable_diagnostics = true);
```

**Location**: [`inc/max22200.hpp#L73`](../inc/max22200.hpp#L73)

## Methods

### Initialization

| Method | Signature | Location |
|--------|-----------|----------|
| `Initialize()` | `DriverStatus Initialize()` | [`inc/max22200.hpp#L100`](../inc/max22200.hpp#L100) |
| `Deinitialize()` | `DriverStatus Deinitialize()` | [`inc/max22200.hpp#L110`](../inc/max22200.hpp#L110) |
| `Reset()` | `DriverStatus Reset()` | [`inc/max22200.hpp#L120`](../inc/max22200.hpp#L120) |

### Global Configuration

| Method | Signature | Location |
|--------|-----------|----------|
| `ConfigureGlobal()` | `DriverStatus ConfigureGlobal(const GlobalConfig &config)` | [`inc/max22200.hpp#L130`](../inc/max22200.hpp#L130) |
| `GetGlobalConfig()` | `DriverStatus GetGlobalConfig(GlobalConfig &config) const` | [`inc/max22200.hpp#L138`](../inc/max22200.hpp#L138) |
| `SetSleepMode()` | `DriverStatus SetSleepMode(bool enable)` | [`inc/max22200.hpp#L146`](../inc/max22200.hpp#L146) |
| `SetDiagnosticMode()` | `DriverStatus SetDiagnosticMode(bool enable)` | [`inc/max22200.hpp#L154`](../inc/max22200.hpp#L154) |
| `SetIntegratedCurrentSensing()` | `DriverStatus SetIntegratedCurrentSensing(bool enable)` | [`inc/max22200.hpp#L162`](../inc/max22200.hpp#L162) |

### Channel Configuration

| Method | Signature | Location |
|--------|-----------|----------|
| `ConfigureChannel()` | `DriverStatus ConfigureChannel(uint8_t channel, const ChannelConfig &config)` | [`inc/max22200.hpp#L173`](../inc/max22200.hpp#L173) |
| `GetChannelConfig()` | `DriverStatus GetChannelConfig(uint8_t channel, ChannelConfig &config) const` | [`inc/max22200.hpp#L182`](../inc/max22200.hpp#L182) |
| `ConfigureAllChannels()` | `DriverStatus ConfigureAllChannels(const ChannelConfigArray &configs)` | [`inc/max22200.hpp#L190`](../inc/max22200.hpp#L190) |
| `GetAllChannelConfigs()` | `DriverStatus GetAllChannelConfigs(ChannelConfigArray &configs) const` | [`inc/max22200.hpp#L198`](../inc/max22200.hpp#L198) |

### Channel Control

| Method | Signature | Location |
|--------|-----------|----------|
| `EnableChannel()` | `DriverStatus EnableChannel(uint8_t channel, bool enable)` | [`inc/max22200.hpp#L209`](../inc/max22200.hpp#L209) |
| `EnableAllChannels()` | `DriverStatus EnableAllChannels(bool enable)` | [`inc/max22200.hpp#L217`](../inc/max22200.hpp#L217) |
| `SetChannelDriveMode()` | `DriverStatus SetChannelDriveMode(uint8_t channel, DriveMode mode)` | [`inc/max22200.hpp#L226`](../inc/max22200.hpp#L226) |
| `SetChannelBridgeMode()` | `DriverStatus SetChannelBridgeMode(uint8_t channel, BridgeMode mode)` | [`inc/max22200.hpp#L235`](../inc/max22200.hpp#L235) |
| `SetChannelPolarity()` | `DriverStatus SetChannelPolarity(uint8_t channel, OutputPolarity polarity)` | [`inc/max22200.hpp#L244`](../inc/max22200.hpp#L244) |

### Current Control

| Method | Signature | Location |
|--------|-----------|----------|
| `SetHitCurrent()` | `DriverStatus SetHitCurrent(uint8_t channel, uint16_t current)` | [`inc/max22200.hpp#L255`](../inc/max22200.hpp#L255) |
| `SetHoldCurrent()` | `DriverStatus SetHoldCurrent(uint8_t channel, uint16_t current)` | [`inc/max22200.hpp#L264`](../inc/max22200.hpp#L264) |
| `SetCurrents()` | `DriverStatus SetCurrents(uint8_t channel, uint16_t hit_current, uint16_t hold_current)` | [`inc/max22200.hpp#L274`](../inc/max22200.hpp#L274) |
| `GetCurrents()` | `DriverStatus GetCurrents(uint8_t channel, uint16_t &hit_current, uint16_t &hold_current) const` | [`inc/max22200.hpp#L285`](../inc/max22200.hpp#L285) |

### Timing Control

| Method | Signature | Location |
|--------|-----------|----------|
| `SetHitTime()` | `DriverStatus SetHitTime(uint8_t channel, uint16_t time)` | [`inc/max22200.hpp#L297`](../inc/max22200.hpp#L297) |
| `GetHitTime()` | `DriverStatus GetHitTime(uint8_t channel, uint16_t &time) const` | [`inc/max22200.hpp#L306`](../inc/max22200.hpp#L306) |

### Status and Diagnostics

| Method | Signature | Location |
|--------|-----------|----------|
| `ReadFaultStatus()` | `DriverStatus ReadFaultStatus(FaultStatus &status) const` | [`inc/max22200.hpp#L316`](../inc/max22200.hpp#L316) |
| `ClearFaultStatus()` | `DriverStatus ClearFaultStatus()` | [`inc/max22200.hpp#L325`](../inc/max22200.hpp#L325) |
| `ReadChannelStatus()` | `DriverStatus ReadChannelStatus(uint8_t channel, ChannelStatus &status) const` | [`inc/max22200.hpp#L334`](../inc/max22200.hpp#L334) |
| `ReadAllChannelStatuses()` | `DriverStatus ReadAllChannelStatuses(ChannelStatusArray &statuses) const` | [`inc/max22200.hpp#L342`](../inc/max22200.hpp#L342) |
| `GetStatistics()` | `DriverStatus GetStatistics(DriverStatistics &stats) const` | [`inc/max22200.hpp#L350`](../inc/max22200.hpp#L350) |
| `ResetStatistics()` | `DriverStatus ResetStatistics()` | [`inc/max22200.hpp#L359`](../inc/max22200.hpp#L359) |

### Callbacks

| Method | Signature | Location |
|--------|-----------|----------|
| `SetFaultCallback()` | `void SetFaultCallback(FaultCallback callback, void *user_data = nullptr)` | [`inc/max22200.hpp#L369`](../inc/max22200.hpp#L369) |
| `SetStateChangeCallback()` | `void SetStateChangeCallback(StateChangeCallback callback, void *user_data = nullptr)` | [`inc/max22200.hpp#L377`](../inc/max22200.hpp#L377) |

### Utility

| Method | Signature | Location |
|--------|-----------|----------|
| `IsInitialized()` | `bool IsInitialized() const` | [`inc/max22200.hpp#L387`](../inc/max22200.hpp#L387) |
| `IsValidChannel()` | `static constexpr bool IsValidChannel(uint8_t channel)` | [`inc/max22200.hpp#L395`](../inc/max22200.hpp#L395) |
| `GetVersion()` | `static constexpr const char *GetVersion()` | [`inc/max22200.hpp#L404`](../inc/max22200.hpp#L404) |

## Types

### Enumerations

| Type | Values | Location |
|------|--------|----------|
| `DriverStatus` | `OK`, `INITIALIZATION_ERROR`, `COMMUNICATION_ERROR`, `INVALID_PARAMETER`, `HARDWARE_FAULT`, `TIMEOUT` | [`inc/max22200_types.hpp#L201`](../inc/max22200_types.hpp#L201) |
| `DriveMode` | `CDR`, `VDR` | [`inc/max22200_types.hpp#L23`](../inc/max22200_types.hpp#L23) |
| `BridgeMode` | `HALF_BRIDGE`, `FULL_BRIDGE` | [`inc/max22200_types.hpp#L31`](../inc/max22200_types.hpp#L31) |
| `OutputPolarity` | `NORMAL`, `INVERTED` | [`inc/max22200_types.hpp#L39`](../inc/max22200_types.hpp#L39) |
| `FaultType` | `OCP`, `OL`, `DPM`, `UVLO`, `HHF`, `TSD` | [`inc/max22200_types.hpp#L47`](../inc/max22200_types.hpp#L47) |
| `ChannelState` | `DISABLED`, `ENABLED`, `HIT_PHASE`, `HOLD_PHASE`, `FAULT` | [`inc/max22200_types.hpp#L213`](../inc/max22200_types.hpp#L213) |

### Structures

| Type | Description | Location |
|------|-------------|----------|
| `ChannelConfig` | Channel configuration structure | [`inc/max22200_types.hpp#L62`](../inc/max22200_types.hpp#L62) |
| `GlobalConfig` | Global configuration structure | [`inc/max22200_types.hpp#L106`](../inc/max22200_types.hpp#L106) |
| `FaultStatus` | Fault status structure | [`inc/max22200_types.hpp#L128`](../inc/max22200_types.hpp#L128) |
| `ChannelStatus` | Channel status structure | [`inc/max22200_types.hpp#L182`](../inc/max22200_types.hpp#L182) |
| `DriverStatistics` | Driver statistics structure | [`inc/max22200_types.hpp#L262`](../inc/max22200_types.hpp#L262) |

### Type Aliases

| Type | Definition | Location |
|------|------------|----------|
| `ChannelConfigArray` | `std::array<ChannelConfig, 8>` | [`inc/max22200_types.hpp#L224`](../inc/max22200_types.hpp#L224) |
| `ChannelStatusArray` | `std::array<ChannelStatus, 8>` | [`inc/max22200_types.hpp#L229`](../inc/max22200_types.hpp#L229) |
| `FaultCallback` | `void (*)(uint8_t channel, FaultType fault_type, void *user_data)` | [`inc/max22200_types.hpp#L243`](../inc/max22200_types.hpp#L243) |
| `StateChangeCallback` | `void (*)(uint8_t channel, ChannelState old_state, ChannelState new_state, void *user_data)` | [`inc/max22200_types.hpp#L254`](../inc/max22200_types.hpp#L254) |

---

**Navigation**
⬅️ [Configuration](configuration.md) | [Next: Examples ➡️](examples.md) | [Back to Index](index.md)

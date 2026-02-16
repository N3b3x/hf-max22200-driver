---
layout: default
title: "⚙️ Configuration"
description: "Configuration options for the MAX22200 driver"
nav_order: 5
parent: "📚 Documentation"
permalink: /docs/configuration/
---

# Configuration

This guide covers configuration options for the MAX22200 driver, aligned with the device registers (STATUS, CFG_CHx, CFG_DPM) and types in `max22200_types.hpp`.

## Channel Configuration (CFG_CHx)

### Basic Channel Setup

Channels are configured via `ChannelConfig` and written with `ConfigureChannel()`. **ChannelConfig stores user units** (mA for CDR, % for VDR, ms for hit time). For CDR, **IFS comes from the board** (SetBoardConfig with RREF)—you do not set it per channel. Call `SetBoardConfig(BoardConfig(rref_kohm, hfs))` first; the driver uses that IFS when writing. The driver gets the master clock (FREQM) from STATUS for hit_time conversion—you do not set it on ChannelConfig. Channels are turned on/off via the STATUS register channels_on_mask (ONCH bits) (see [Channel Control](#channel-control)).

```cpp
driver.SetBoardConfig(max22200::BoardConfig(30.0f, false));  // IFS from RREF (required for CDR)

max22200::ChannelConfig config;
config.drive_mode = max22200::DriveMode::CDR;
config.side_mode = max22200::SideMode::LOW_SIDE;
config.hit_setpoint = 500.0f;   // desired current, mA
config.hold_setpoint = 200.0f;  // mA
config.hit_time_ms = 10.0f;     // 10 ms
config.chop_freq = max22200::ChopFreq::FMAIN_DIV2;
config.hit_current_check_enabled = true;
config.open_load_detection_enabled = false;
config.plunger_movement_detection_enabled = false;

driver.ConfigureChannel(0, config);
```

### Drive Modes

- **CDR (Current Drive Regulation)**  
  `config.drive_mode = max22200::DriveMode::CDR;`  
  Regulates current to hit/hold levels. Only supported in low-side mode.

- **VDR (Voltage Drive Regulation)**  
  `config.drive_mode = max22200::DriveMode::VDR;`  
  Duty-cycle control. Supported in both low-side and high-side.

### Side Mode (Low-Side vs High-Side)

- **Low-side**  
  `config.side_mode = max22200::SideMode::LOW_SIDE;`  
  Load between OUT and VM. Supports CDR and VDR, HFS, SRC, DPM. When `half_full_scale` is true, effective IFS for that channel is half of board IFS (driver uses it for mA ↔ raw conversion).

- **High-side**  
  `config.side_mode = max22200::SideMode::HIGH_SIDE;`  
  Load between OUT and GND. VDR only; no CDR, HFS, SRC, or DPM.

### Current and Timing in Real Units

**Option A — Driver APIs (recommended):** Set IFS with `BoardConfig`, then use mA/ms APIs:

```cpp
max22200::BoardConfig board(30.0f, false);
board.max_current_ma = 800;      // Optional safety limit (0 = no limit)
board.max_duty_percent = 90;    // Optional for VDR (0 = no limit)
driver.SetBoardConfig(board);

driver.SetHitCurrentMa(0, 500);   // Channel 0, 500 mA
driver.SetHoldCurrentMa(0, 200);
driver.SetHitTimeMs(0, 10.0f);   // 10 ms (converted from fCHOP)
```

**Option B — Set user units on ChannelConfig directly:** Call `SetBoardConfig()` first (IFS from RREF). Set `hit_setpoint`, `hold_setpoint`, `hit_time_ms`, and `chop_freq`. The driver uses board IFS (and STATUS FREQM for hit time) when writing; ChannelConfig does not store IFS or master clock.

```cpp
driver.SetBoardConfig(max22200::BoardConfig(30.0f, false));  // IFS from RREF

max22200::ChannelConfig config;
config.drive_mode = max22200::DriveMode::CDR;
config.side_mode = max22200::SideMode::LOW_SIDE;
config.hit_setpoint = 500.0f;   // mA
config.hold_setpoint = 200.0f;  // mA
config.hit_time_ms = 10.0f;
config.chop_freq = max22200::ChopFreq::FMAIN_DIV2;
driver.ConfigureChannel(0, config);
```

For VDR, set `hit_setpoint` and `hold_setpoint` as duty percent (0–100). Helpers `currentMaToRaw()`, `hitTimeMsToRaw()`, and `getChopFreqKhz()` are in `max22200_types.hpp` for custom conversion.

### Human-Readable Probes on ChannelConfig

You can query config with inline helpers (see `max22200_types.hpp`):

- `config.isCdr()` / `config.isVdr()`, `config.isLowSide()` / `config.isHighSide()`
- `config.hasHitTime()`, `config.isContinuousHit()` (hit_time_ms > 0 or continuous)
- `config.isHalfFullScale()`, `config.isSlewRateControlEnabled()`, `config.isOpenLoadDetectionEnabled()`, `config.isPlungerMovementDetectionEnabled()`, `config.isHitCurrentCheckEnabled()`

## STATUS Register (StatusConfig)

The STATUS register holds channel on/off bits (ONCH), fault masks, channel-pair mode (CMxy), and the ACTIVE bit. Read/write with `ReadStatus()` and `WriteStatus()`. The driver keeps an internal cache of STATUS (updated on ReadStatus, WriteStatus, and Initialize) for hit-time and duty-limit conversions. If you write STATUS via `WriteRegister32(RegBank::STATUS, ...)`, that cache is not updated—prefer `WriteStatus(StatusConfig)` when changing STATUS.

### Channel On/Off (ONCH)

- `status.channels_on_mask` — bitmask: bit N = channel N on (1) or off (0).
- Helpers: `status.isChannelOn(ch)`, `status.channelCountOn()`, `status.getChannelsOnMask()`.

### Fault Masks (M_OVT, M_OCP, …)

When a mask bit is 1, that fault does not assert the nFAULT pin. Use the human-readable helpers:

- `status.isOvertemperatureMasked()`, `status.isOvercurrentMasked()`, `status.isOpenLoadFaultMasked()`, `status.isHitNotReachedMasked()`, `status.isPlungerMovementFaultMasked()`, `status.isCommunicationErrorMasked()`, `status.isUndervoltageMasked()`.

### Fault Flags (Read-Only)

After `ReadStatus(status)` or `ReadFaultFlags(status)`:

- `status.hasOvertemperature()`, `status.hasOvercurrent()`, `status.hasOpenLoadFault()`, `status.hasHitNotReached()`, `status.hasPlungerMovementFault()`, `status.hasCommunicationError()`, `status.hasUndervoltage()`, `status.hasFault()`, `status.isActive()`.

### Channel-Pair Mode (CM10, CM32, CM54, CM76)

Pairs (0–1, 2–3, 4–5, 6–7) can be INDEPENDENT, PARALLEL, or HBRIDGE. Change only when both channels in the pair are off.

- `status.getChannelPairMode10()`, `getChannelPairMode32()`, `getChannelPairMode54()`, `getChannelPairMode76()` return `ChannelMode`.

### Master Frequency (FREQM)

- `status.is100KHzBase()` / `status.is80KHzBase()` — base clock for chopping frequency.

## FAULT Register (FaultStatus)

Per-channel fault bits (OCP, HHF, OLF, DPM) are read with `ReadFaultRegister(faults)`. Reading clears flags (and deasserts nFAULT when no other faults remain). Use human-readable probes:

- `faults.hasOvercurrent()`, `faults.hasHitNotReached()`, `faults.hasOpenLoadFault()`, `faults.hasPlungerMovementFault()`
- `faults.hasFaultOnChannel(ch)`, `faults.hasOvercurrentOnChannel(ch)`, etc.
- `faults.channelsWithAnyFault()` — bitmask of channels with any fault.
- `FaultTypeToStr(fault_type)` for logging (e.g. "Overcurrent", "HIT not reached").

Clearing:

- `ClearAllFaults()` — clear all and discard.
- `ClearChannelFaults(channel_mask, &faults)` — clear selected channels (MAX22200A); optional snapshot in `faults`.

## DPM Configuration (CFG_DPM)

Plunger movement detection is global; enable per channel with `config.plunger_movement_detection_enabled = true` (low-side only), or in one call with `SetDpmEnabledChannels(channel_mask)` (bit N = 1 enables DPM on channel N). Configure algorithm in real units:

```cpp
driver.ConfigureDpm(200.0f, 50.0f, 2.0f);
// start_current_ma, dip_threshold_ma, debounce_ms (debounce uses cached STATUS FREQM + FMAIN_DIV4 fCHOP)
```

Or read/write raw `DpmConfig` (plunger_movement_start_current, plunger_movement_debounce_time, plunger_movement_current_threshold) with `ReadDpmConfig()` / `WriteDpmConfig()`. Use `getPlungerMovementStartCurrent()`, `getPlungerMovementDebounceTime()`, `getPlungerMovementCurrentThreshold()` for readable access.

## Channel Control

### Enable/Disable Channels

```cpp
driver.EnableChannel(0);
driver.DisableChannel(0);
driver.SetChannelEnabled(0, true);   // or false

driver.EnableAllChannels();
driver.DisableAllChannels();
driver.SetAllChannelsEnabled(true); // or false
driver.SetChannelsOn((1u << 0) | (1u << 2));  // Channels 0 and 2 on
```

### Full-Bridge Pairs

When a pair is configured as HBRIDGE in STATUS (e.g. `status.channel_pair_mode_10 = ChannelMode::HBRIDGE`), use:

```cpp
driver.SetFullBridgeState(0, max22200::FullBridgeState::Forward);  // pair 0 = ch0–ch1
```

## Device Enable (ENABLE Pin)

```cpp
driver.EnableDevice();   // ENABLE pin high — SPI and channels active
driver.DisableDevice();  // ENABLE pin low — low power
driver.SetDeviceEnable(true);  // or false
```

Initialization sets ACTIVE=1 and ENABLE high; `Deinitialize()` sets ACTIVE=0 and ENABLE low.

## Recommended Setups

### Solenoid (CDR, low-side)

```cpp
max22200::ChannelConfig config;
config.drive_mode = max22200::DriveMode::CDR;
config.side_mode = max22200::SideMode::LOW_SIDE;
config.hit_setpoint = 800.0f;  // mA (IFS from SetBoardConfig)
config.hold_setpoint = 200.0f;
config.hit_time_ms = 50.0f;
config.chop_freq = max22200::ChopFreq::FMAIN_DIV2;
config.hit_current_check_enabled = true;
driver.ConfigureChannel(0, config);
driver.EnableChannel(0);
```

Or use the preset: `auto config = max22200::ChannelConfig::makeSolenoidCdr(800.0f, 200.0f, 50.0f); config.hit_current_check_enabled = true; driver.ConfigureChannel(0, config);`

### Motor (H-bridge pair)

Set channel pair to `ChannelMode::HBRIDGE` in STATUS (both channels off), configure both channels, then use `SetFullBridgeState(pair_index, Forward)` or `Reverse`, etc.

### Low-Power (VDR)

```cpp
config.drive_mode = max22200::DriveMode::VDR;
config.side_mode = max22200::SideMode::LOW_SIDE;
config.hit_setpoint = 60.0f;   // duty %
config.hold_setpoint = 20.0f;
config.hit_time_ms = 100.0f;
config.chop_freq = max22200::ChopFreq::FMAIN_DIV2;
```

Or: `auto config = max22200::ChannelConfig::makeSolenoidVdr(60.0f, 20.0f, 100.0f); driver.ConfigureChannel(0, config);`

## Next Steps

- See [Examples](examples.md) for full code.
- [API Reference](api_reference.md) for all methods and types.

---

**Navigation**
⬅️ [Platform Integration](platform_integration.md) | [Next: API Reference ➡️](api_reference.md) | [Back to Index](index.md)

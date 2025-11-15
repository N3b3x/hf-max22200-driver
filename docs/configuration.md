---
layout: default
title: "⚙️ Configuration"
description: "Configuration options for the MAX22200 driver"
nav_order: 5
parent: "📚 Documentation"
permalink: /docs/configuration/
---

# Configuration

This guide covers all configuration options available for the MAX22200 driver.

## Channel Configuration

### Basic Channel Setup

```cpp
max22200::ChannelConfig config;
config.enabled = true;
config.drive_mode = max22200::DriveMode::CDR;  // Current Drive Regulation
config.bridge_mode = max22200::BridgeMode::HALF_BRIDGE;
config.parallel_mode = false;
config.polarity = max22200::OutputPolarity::NORMAL;
config.hit_current = 500;   // Hit current (0-1023)
config.hold_current = 200;  // Hold current (0-1023)
config.hit_time = 1000;     // Hit time in ms (0-65535)

driver.ConfigureChannel(0, config);
```

### Drive Modes

#### CDR (Current Drive Regulation)

```cpp
config.drive_mode = max22200::DriveMode::CDR;
```

- Regulates current to hit_current during hit phase
- Regulates current to hold_current during hold phase
- Best for solenoids and motors requiring constant current

#### VDR (Voltage Drive Regulation)

```cpp
config.drive_mode = max22200::DriveMode::VDR;
```

- Applies constant voltage during hit phase
- Regulates current to hold_current during hold phase
- Best for applications requiring fast response

### Bridge Modes

#### Half-Bridge Mode

```cpp
config.bridge_mode = max22200::BridgeMode::HALF_BRIDGE;
```

- Load connected between OUTA and GND
- OUTB not used
- Single-ended drive

#### Full-Bridge Mode

```cpp
config.bridge_mode = max22200::BridgeMode::FULL_BRIDGE;
```

- Load connected between OUTA and OUTB
- Bidirectional drive
- Can reverse current direction

### Current Settings

```cpp
// Set hit and hold currents separately
driver.SetHitCurrent(0, 500);   // Channel 0, 500 mA
driver.SetHoldCurrent(0, 200);  // Channel 0, 200 mA

// Or set both at once
driver.SetCurrents(0, 500, 200);  // Channel 0, hit=500mA, hold=200mA
```

**Current Range**: 0-1023 (10-bit resolution)

### Hit Time

```cpp
driver.SetHitTime(0, 1000);  // Channel 0, 1000 ms
```

**Time Range**: 0-65535 ms

## Global Configuration

### Global Settings

```cpp
max22200::GlobalConfig global_config;
global_config.reset = false;
global_config.sleep_mode = false;
global_config.diagnostic_enable = true;
global_config.ics_enable = true;  // Integrated Current Sensing
global_config.daisy_chain_mode = false;

driver.ConfigureGlobal(global_config);
```

### Sleep Mode

```cpp
driver.SetSleepMode(true);   // Enable sleep mode
driver.SetSleepMode(false);  // Disable sleep mode
```

### Diagnostic Mode

```cpp
driver.SetDiagnosticMode(true);  // Enable diagnostics
```

### Integrated Current Sensing (ICS)

```cpp
driver.SetIntegratedCurrentSensing(true);  // Enable ICS
```

## Channel Control

### Enable/Disable Channels

```cpp
// Enable single channel
driver.EnableChannel(0, true);

// Disable single channel
driver.EnableChannel(0, false);

// Enable all channels
driver.EnableAllChannels(true);

// Disable all channels
driver.EnableAllChannels(false);
```

### Individual Channel Settings

```cpp
// Set drive mode
driver.SetChannelDriveMode(0, max22200::DriveMode::CDR);

// Set bridge mode
driver.SetChannelBridgeMode(0, max22200::BridgeMode::FULL_BRIDGE);

// Set polarity
driver.SetChannelPolarity(0, max22200::OutputPolarity::INVERTED);
```

## Recommended Settings

### For Solenoid Control

```cpp
max22200::ChannelConfig config;
config.enabled = true;
config.drive_mode = max22200::DriveMode::CDR;
config.bridge_mode = max22200::BridgeMode::HALF_BRIDGE;
config.hit_current = 800;   // High current for pull-in
config.hold_current = 200;  // Lower current for holding
config.hit_time = 50;       // Short hit time
```

### For Motor Control

```cpp
max22200::ChannelConfig config;
config.enabled = true;
config.drive_mode = max22200::DriveMode::CDR;
config.bridge_mode = max22200::BridgeMode::FULL_BRIDGE;
config.hit_current = 500;
config.hold_current = 500;
config.hit_time = 0;  // Continuous operation
```

### For Low-Power Applications

```cpp
max22200::ChannelConfig config;
config.enabled = true;
config.drive_mode = max22200::DriveMode::VDR;
config.bridge_mode = max22200::BridgeMode::HALF_BRIDGE;
config.hit_current = 300;
config.hold_current = 100;
config.hit_time = 100;
```

## Next Steps

- See [Examples](examples.md) for configuration examples
- Review [API Reference](api_reference.md) for all configuration methods

---

**Navigation**
⬅️ [Platform Integration](platform_integration.md) | [Next: API Reference ➡️](api_reference.md) | [Back to Index](index.md)


---
layout: default
title: "⚡ Quick Start"
description: "Get up and running with the MAX22200 driver in minutes"
nav_order: 2
parent: "📚 Documentation"
permalink: /docs/quickstart/
---

# Quick Start

This guide will get you up and running with the MAX22200 driver in just a few steps.

## Prerequisites

- [Driver installed](installation.md)
- [Hardware wired](hardware_setup.md)
- [SPI interface implemented](platform_integration.md)

## Minimal Example

Here's a complete working example:

```cpp
#include "max22200.hpp"

// 1. Implement the SPI interface
class MySpi : public max22200::SpiInterface<MySpi> {
public:
    void transfer(const uint8_t* tx, uint8_t* rx, size_t len) {
        // Your SPI transfer implementation
        // Assert CS, transfer data, deassert CS
    }
};

// 2. Create instances
MySpi spi;
max22200::MAX22200 driver(spi);

// 3. Initialize
if (driver.Initialize() == max22200::DriverStatus::OK) {
    // 4. Configure channel 0 (CDR, low-side; user units: mA and ms)
    max22200::ChannelConfig config;
    config.drive_mode = max22200::DriveMode::CDR;
    config.side_mode = max22200::SideMode::LOW_SIDE;
    config.hit_setpoint = 500.0f;   // 500 mA
    config.hold_setpoint = 200.0f;  // 200 mA
    config.hit_time_ms = 10.0f;          // 10 ms
    // IFS from SetBoardConfig; driver uses STATUS FREQM for hit time
    config.chop_freq = max22200::ChopFreq::FMAIN_DIV2;
    config.hit_current_check_enabled = true;                // Enable HIT current check

    driver.ConfigureChannel(0, config);
    driver.EnableChannel(0);
}
```

## Step-by-Step Explanation

### Step 1: Include the Header

```cpp
#include "max22200.hpp"
```

This includes the main driver class and all necessary types.

### Step 2: Implement the SPI Interface

You need to implement the `SpiInterface` for your platform. See [Platform Integration](platform_integration.md) for detailed examples.

```cpp
class MySpi : public max22200::SpiInterface<MySpi> {
public:
    void transfer(const uint8_t* tx, uint8_t* rx, size_t len) {
        // Assert chip select
        // Perform SPI transfer
        // Deassert chip select
    }
};
```

### Step 3: Create Driver Instance

```cpp
MySpi spi;
max22200::MAX22200 driver(spi);
```

The constructor takes a reference to your SPI interface implementation.

### Step 4: Initialize

```cpp
if (driver.Initialize() == max22200::DriverStatus::OK) {
    // Driver is ready
}
```

### Step 5: Configure Channel

```cpp
max22200::ChannelConfig config;
config.drive_mode = max22200::DriveMode::CDR;
config.side_mode = max22200::SideMode::LOW_SIDE;
config.hit_setpoint = 500.0f;   // 500 mA (CDR)
config.hold_setpoint = 200.0f; // 200 mA
config.hit_time_ms = 10.0f;         // 10 ms
// IFS from SetBoardConfig; driver uses cached STATUS for hit time
config.chop_freq = max22200::ChopFreq::FMAIN_DIV2;
config.hit_current_check_enabled = true;

driver.ConfigureChannel(0, config);
```

Alternatively, set board config first then use convenience APIs:

```cpp
max22200::BoardConfig board(30.0f, false); // IFS from RREF
driver.SetBoardConfig(board);
driver.SetHitCurrentMa(0, 500);  // Channel 0, 500 mA
driver.SetHoldCurrentMa(0, 200);
```

### Step 6: Enable Channel

```cpp
driver.EnableChannel(0);
```

## Complete Example with Error Handling

```cpp
#include "max22200.hpp"

class MySpi : public max22200::SpiInterface<MySpi> {
    // ... SPI implementation
};

void app_main() {
    MySpi spi;
    max22200::MAX22200 driver(spi);
    
    // Initialize
    max22200::DriverStatus status = driver.Initialize();
    if (status != max22200::DriverStatus::OK) {
        printf("Initialization failed\n");
        return;
    }
    
    // Configure channel (user units: mA and ms)
    max22200::ChannelConfig config;
    config.drive_mode = max22200::DriveMode::CDR;
    config.side_mode = max22200::SideMode::LOW_SIDE;
    config.hit_setpoint = 500.0f;
    config.hold_setpoint = 200.0f;
    config.hit_time_ms = 10.0f;
    config.chop_freq = max22200::ChopFreq::FMAIN_DIV2;

    status = driver.ConfigureChannel(0, config);
    if (status != max22200::DriverStatus::OK) {
        printf("Channel configuration failed\n");
        return;
    }

    // Enable channel
    driver.EnableChannel(0);

    // Read configured current in mA (requires SetBoardConfig with IFS)
    uint32_t current_ma = 0;
    if (driver.GetHitCurrentMa(0, current_ma) == max22200::DriverStatus::OK) {
        printf("Channel 0 hit current: %" PRIu32 " mA\n", current_ma);
    }
}
```

## Expected Output

When running this example (with `SetBoardConfig` set for your IFS), you should see the configured current in mA, for example:

```
Channel 0 hit current: 500 mA
```

## Troubleshooting

If you encounter issues:

- **Compilation errors**: Check that you've implemented the `transfer()` method in your SPI interface
- **Initialization fails**: Verify SPI connections and hardware setup
- **Channel not working**: Check channel configuration and enable state
- **See**: [Troubleshooting](troubleshooting.md) for common issues

## Next Steps

- Explore [Examples](examples.md) for more advanced usage
- Review the [API Reference](api_reference.md) for all available methods
- Check [Configuration](configuration.md) for customization options

---

**Navigation**
⬅️ [Installation](installation.md) | [Next: Hardware Setup ➡️](hardware_setup.md) | [Back to Index](index.md)


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
    // 4. Configure channel 0
    max22200::ChannelConfig config;
    config.enabled = true;
    config.drive_mode = max22200::DriveMode::CDR;
    config.bridge_mode = max22200::BridgeMode::HALF_BRIDGE;
    config.hit_current = 500;   // 500 mA hit current
    config.hold_current = 200;  // 200 mA hold current
    config.hit_time = 1000;     // 1000 ms hit time
    
    driver.ConfigureChannel(0, config);
    driver.EnableChannel(0, true);
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
config.enabled = true;
config.drive_mode = max22200::DriveMode::CDR;  // Current Drive Regulation
config.bridge_mode = max22200::BridgeMode::HALF_BRIDGE;
config.hit_current = 500;   // Hit current in mA (0-1023)
config.hold_current = 200;  // Hold current in mA (0-1023)
config.hit_time = 1000;     // Hit time in ms (0-65535)

driver.ConfigureChannel(0, config);
```

### Step 6: Enable Channel

```cpp
driver.EnableChannel(0, true);
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
    
    // Configure channel
    max22200::ChannelConfig config;
    config.enabled = true;
    config.drive_mode = max22200::DriveMode::CDR;
    config.bridge_mode = max22200::BridgeMode::HALF_BRIDGE;
    config.hit_current = 500;
    config.hold_current = 200;
    config.hit_time = 1000;
    
    status = driver.ConfigureChannel(0, config);
    if (status != max22200::DriverStatus::OK) {
        printf("Channel configuration failed\n");
        return;
    }
    
    // Enable channel
    driver.EnableChannel(0, true);
    
    // Read current
    uint16_t current;
    if (driver.ReadCurrent(0, current) == max22200::DriverStatus::OK) {
        printf("Channel 0 current: %u mA\n", current);
    }
}
```

## Expected Output

When running this example, you should see:

```
Channel 0 current: 500 mA
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


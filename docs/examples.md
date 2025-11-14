# Examples

This guide provides complete, working examples demonstrating various use cases for the MAX22200 driver.

## Example 1: Basic Solenoid Control

This example shows basic solenoid control with hit and hold currents.

```cpp
#include "max22200.hpp"
#include "esp32_max22200_spi.hpp"

void app_main() {
    // 1. Configure SPI
    Esp32Max22200Spi::SPIConfig spi_config;
    spi_config.host = SPI2_HOST;
    spi_config.miso_pin = GPIO_NUM_2;
    spi_config.mosi_pin = GPIO_NUM_7;
    spi_config.sclk_pin = GPIO_NUM_6;
    spi_config.cs_pin = GPIO_NUM_10;
    spi_config.frequency = 10000000;
    spi_config.mode = 0;
    
    Esp32Max22200Spi spi(spi_config);
    spi.Initialize();
    
    // 2. Create driver
    max22200::MAX22200 driver(spi);
    
    // 3. Initialize
    if (driver.Initialize() != max22200::DriverStatus::OK) {
        printf("Initialization failed\n");
        return;
    }
    
    // 4. Configure channel 0 for solenoid
    max22200::ChannelConfig config;
    config.enabled = true;
    config.drive_mode = max22200::DriveMode::CDR;
    config.bridge_mode = max22200::BridgeMode::HALF_BRIDGE;
    config.hit_current = 800;   // 800 mA hit current
    config.hold_current = 200;  // 200 mA hold current
    config.hit_time = 50;       // 50 ms hit time
    
    driver.ConfigureChannel(0, config);
    driver.EnableChannel(0, true);
    
    printf("Solenoid enabled\n");
}
```

---

## Example 2: Motor Control with Full-Bridge

This example demonstrates motor control using full-bridge mode.

```cpp
#include "max22200.hpp"

void app_main() {
    // ... SPI setup (same as Example 1)
    
    max22200::MAX22200 driver(spi);
    driver.Initialize();
    
    // Configure channel 0 for motor
    max22200::ChannelConfig config;
    config.enabled = true;
    config.drive_mode = max22200::DriveMode::CDR;
    config.bridge_mode = max22200::BridgeMode::FULL_BRIDGE;
    config.hit_current = 500;
    config.hold_current = 500;
    config.hit_time = 0;  // Continuous operation
    
    driver.ConfigureChannel(0, config);
    driver.EnableChannel(0, true);
    
    // Read current
    uint16_t current;
    if (driver.ReadCurrent(0, current) == max22200::DriverStatus::OK) {
        printf("Motor current: %u mA\n", current);
    }
}
```

---

## Example 3: Fault Handling with Callbacks

This example shows how to handle faults using callbacks.

```cpp
#include "max22200.hpp"

void fault_handler(uint8_t channel, max22200::FaultType fault_type, void *user_data) {
    const char* fault_names[] = {
        "OCP", "OL", "DPM", "UVLO", "HHF", "TSD"
    };
    printf("Fault on channel %d: %s\n", channel, fault_names[static_cast<int>(fault_type)]);
}

void app_main() {
    // ... SPI setup
    
    max22200::MAX22200 driver(spi);
    driver.Initialize();
    
    // Set fault callback
    driver.SetFaultCallback(fault_handler);
    
    // Configure and enable channel
    max22200::ChannelConfig config;
    config.enabled = true;
    config.drive_mode = max22200::DriveMode::CDR;
    config.hit_current = 500;
    config.hold_current = 200;
    
    driver.ConfigureChannel(0, config);
    driver.EnableChannel(0, true);
    
    // Monitor for faults
    while (true) {
        max22200::FaultStatus faults;
        driver.ReadFaultStatus(faults);
        
        if (faults.hasFault()) {
            printf("Fault detected: %d faults\n", faults.getFaultCount());
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

---

## Example 4: Multiple Channels

This example demonstrates controlling multiple channels.

```cpp
#include "max22200.hpp"

void app_main() {
    // ... SPI setup
    
    max22200::MAX22200 driver(spi);
    driver.Initialize();
    
    // Configure multiple channels
    for (uint8_t ch = 0; ch < 8; ch++) {
        max22200::ChannelConfig config;
        config.enabled = true;
        config.drive_mode = max22200::DriveMode::CDR;
        config.bridge_mode = max22200::BridgeMode::HALF_BRIDGE;
        config.hit_current = 500;
        config.hold_current = 200;
        config.hit_time = 100;
        
        driver.ConfigureChannel(ch, config);
    }
    
    // Enable all channels
    driver.EnableAllChannels(true);
    
    // Read status of all channels
    max22200::ChannelStatusArray statuses;
    driver.ReadAllChannelStatuses(statuses);
    
    for (uint8_t ch = 0; ch < 8; ch++) {
        printf("Channel %d: enabled=%d, current=%u\n", 
               ch, statuses[ch].enabled, statuses[ch].current_reading);
    }
}
```

---

## Example 5: Current Monitoring

This example demonstrates continuous current monitoring.

```cpp
#include "max22200.hpp"

void app_main() {
    // ... SPI setup
    
    max22200::MAX22200 driver(spi);
    driver.Initialize();
    
    // Enable ICS
    driver.SetIntegratedCurrentSensing(true);
    
    // Configure channel
    max22200::ChannelConfig config;
    config.enabled = true;
    config.drive_mode = max22200::DriveMode::CDR;
    config.hit_current = 500;
    config.hold_current = 200;
    
    driver.ConfigureChannel(0, config);
    driver.EnableChannel(0, true);
    
    // Monitor current
    while (true) {
        uint16_t current;
        if (driver.ReadCurrent(0, current) == max22200::DriverStatus::OK) {
            printf("Current: %u mA\n", current);
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

---

## Running the Examples

### ESP32

```bash
cd examples/esp32
idf.py build flash monitor
```

### Other Platforms

For other platforms, implement the SPI interface and compile with C++20 support.

## Next Steps

- Review the [API Reference](api_reference.md) for method details
- Check [Troubleshooting](troubleshooting.md) if you encounter issues
- Explore the [examples directory](../examples/) for more examples

---

**Navigation**
⬅️ [API Reference](api_reference.md) | [Next: Troubleshooting ➡️](troubleshooting.md) | [Back to Index](index.md)

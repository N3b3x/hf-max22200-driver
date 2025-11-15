---
layout: default
title: "🔧 Platform Integration"
description: "How to implement the SPI interface for your platform"
nav_order: 4
parent: "📚 Documentation"
permalink: /docs/platform_integration/
---

# Platform Integration Guide

This guide explains how to implement the hardware abstraction interface for the MAX22200 driver on your platform.

## Understanding CRTP (Curiously Recurring Template Pattern)

The MAX22200 driver uses **CRTP** (Curiously Recurring Template Pattern) for hardware
abstraction. This design choice provides several critical benefits for embedded systems:

### Why CRTP Instead of Virtual Functions?

#### 1. **Zero Runtime Overhead**

- **Virtual functions**: Require a vtable lookup (indirect call) = ~5-10 CPU cycles overhead per call
- **CRTP**: Direct function calls = 0 overhead, compiler can inline
- **Impact**: In time-critical embedded code controlling solenoids/motors, this matters significantly

#### 2. **Compile-Time Polymorphism**

- **Virtual functions**: Runtime dispatch - the compiler cannot optimize across the abstraction boundary
- **CRTP**: Compile-time dispatch - full optimization, dead code elimination, constant propagation
- **Impact**: Smaller code size, faster execution

#### 3. **Memory Efficiency**

- **Virtual functions**: Each object needs a vtable pointer (4-8 bytes)
- **CRTP**: No vtable pointer needed
- **Impact**: Critical in memory-constrained systems

#### 4. **Type Safety**

- **Virtual functions**: Runtime errors if method not implemented
- **CRTP**: Compile-time errors if method not implemented
- **Impact**: Catch bugs at compile time, not in the field

### How CRTP Works

```cpp
// Base template class (from max22200_spi_interface.hpp)
template <typename Derived>
class SpiInterface {
public:
    bool Transfer(const uint8_t* tx_data, uint8_t* rx_data, size_t length) {
        // Cast 'this' to Derived* and call the derived implementation
        return static_cast<Derived*>(this)->Transfer(tx_data, rx_data, length);
    }
};

// Your implementation
class MySPI : public max22200::SpiInterface<MySPI> {
public:
    // This method is called directly (no virtual overhead)
    bool Transfer(const uint8_t* tx_data, uint8_t* rx_data, size_t length) {
        // Your platform-specific SPI code
    }
};
```cpp

## Interface Definition

The MAX22200 driver requires you to implement the following interface:

```cpp
template <typename Derived>
class SpiInterface {
public:
    // Required methods (implement all of these)
    bool Initialize();
    bool Transfer(const uint8_t* tx_data, uint8_t* rx_data, size_t length);
    void SetChipSelect(bool state);
    bool Configure(uint32_t speed_hz, uint8_t mode, bool msb_first = true);
    bool IsReady() const;
};
```cpp

## Implementation Steps

### Step 1: Create Your Implementation Class

```cpp
#include "max22200_spi_interface.hpp"

class MyPlatformSPI : public max22200::SpiInterface<MyPlatformSPI> {
private:
    // Your platform-specific members
    spi_device_handle_t spi_device_;  // Example for ESP32
    
public:
    // Constructor
    MyPlatformSPI(spi_device_handle_t device) : spi_device_(device) {}
    
    // Implement required methods
    bool Initialize() {
        // Your initialization code
        return true;
    }
    
    bool Transfer(const uint8_t* tx_data, uint8_t* rx_data, size_t length) {
        // Your transfer code
        return true;
    }
    
    void SetChipSelect(bool state) {
        // Your CS control code
    }
    
    bool Configure(uint32_t speed_hz, uint8_t mode, bool msb_first) {
        // Your configuration code
        return true;
    }
    
    bool IsReady() const {
        // Check if SPI is ready
        return true;
    }
};
```

### Step 2: Platform-Specific Examples

#### ESP32 (ESP-IDF)

```cpp
#include "driver/spi_master.h"
#include "max22200_spi_interface.hpp"

class Esp32SPIBus : public max22200::SpiInterface<Esp32SPIBus> {
private:
    spi_device_handle_t spi_device_;
    gpio_num_t cs_pin_;
    
public:
    Esp32SPIBus(spi_host_device_t host, const spi_device_interface_config_t& config, gpio_num_t cs) {
        spi_bus_add_device(host, &config, &spi_device_);
        cs_pin_ = cs;
    }
    
    bool Initialize() {
        return spi_device_ != nullptr;
    }
    
    bool Transfer(const uint8_t* tx_data, uint8_t* rx_data, size_t length) {
        spi_transaction_t trans = {};
        trans.length = length * 8;
        trans.tx_buffer = tx_data;
        trans.rx_buffer = rx_data;
        
        esp_err_t ret = spi_device_transmit(spi_device_, &trans);
        return ret == ESP_OK;
    }
    
    void SetChipSelect(bool state) {
        gpio_set_level(cs_pin_, state ? 0 : 1);  // Active low
    }
    
    bool Configure(uint32_t speed_hz, uint8_t mode, bool msb_first) {
        // ESP-IDF handles this via device config
        return true;
    }
    
    bool IsReady() const {
        return spi_device_ != nullptr;
    }
};
```

#### STM32 (HAL)

```cpp
#include "stm32f4xx_hal.h"
#include "max22200_spi_interface.hpp"

extern SPI_HandleTypeDef hspi1;

class STM32SPIBus : public max22200::SpiInterface<STM32SPIBus> {
private:
    GPIO_TypeDef* cs_port_;
    uint16_t cs_pin_;
    
public:
    STM32SPIBus(GPIO_TypeDef* cs_port, uint16_t cs_pin) 
        : cs_port_(cs_port), cs_pin_(cs_pin) {}
    
    bool Initialize() {
        return HAL_SPI_Init(&hspi1) == HAL_OK;
    }
    
    bool Transfer(const uint8_t* tx_data, uint8_t* rx_data, size_t length) {
        SetChipSelect(true);  // Assert CS
        
        HAL_StatusTypeDef status = HAL_SPI_TransmitReceive(&hspi1, 
                                                          (uint8_t*)tx_data, 
                                                          rx_data, 
                                                          length, 
                                                          HAL_MAX_DELAY);
        
        SetChipSelect(false);  // Deassert CS
        
        return status == HAL_OK;
    }
    
    void SetChipSelect(bool state) {
        HAL_GPIO_WritePin(cs_port_, cs_pin_, state ? GPIO_PIN_RESET : GPIO_PIN_SET);
    }
    
    bool Configure(uint32_t speed_hz, uint8_t mode, bool msb_first) {
        // STM32 HAL handles this via SPI init
        return true;
    }
    
    bool IsReady() const {
        return hspi1.State == HAL_SPI_STATE_READY;
    }
};
```

#### Arduino

```cpp
#include <SPI.h>
#include "max22200_spi_interface.hpp"

class ArduinoSPIBus : public max22200::SpiInterface<ArduinoSPIBus> {
private:
    uint8_t cs_pin_;
    
public:
    ArduinoSPIBus(uint8_t cs_pin) : cs_pin_(cs_pin) {
        pinMode(cs_pin_, OUTPUT);
        digitalWrite(cs_pin_, HIGH);
    }
    
    bool Initialize() {
        SPI.begin();
        return true;
    }
    
    bool Transfer(const uint8_t* tx_data, uint8_t* rx_data, size_t length) {
        SPI.beginTransaction(SPISettings(10000000, MSBFIRST, SPI_MODE0));
        SetChipSelect(true);
        
        for (size_t i = 0; i < length; i++) {
            uint8_t byte = SPI.transfer(tx_data ? tx_data[i] : 0);
            if (rx_data) {
                rx_data[i] = byte;
            }
        }
        
        SetChipSelect(false);
        SPI.endTransaction();
        return true;
    }
    
    void SetChipSelect(bool state) {
        digitalWrite(cs_pin_, state ? LOW : HIGH);  // Active low
    }
    
    bool Configure(uint32_t speed_hz, uint8_t mode, bool msb_first) {
        // Arduino SPI handles this via beginTransaction
        return true;
    }
    
    bool IsReady() const {
        return true;
    }
};
```

## Common Pitfalls

### ❌ Don't Use Virtual Functions

```cpp
// WRONG - defeats the purpose of CRTP
class MyBus : public max22200::SpiInterface<MyBus> {
public:
    virtual bool Transfer(...) override {  // ❌ Virtual keyword not needed
        // ...
    }
};
```

### ✅ Correct CRTP Implementation

```cpp
// CORRECT - no virtual keyword
class MyBus : public max22200::SpiInterface<MyBus> {
public:
    bool Transfer(...) {  // ✅ Direct implementation
        // ...
    }
};
```

### ❌ Don't Forget the Template Parameter

```cpp
// WRONG - missing template parameter
class MyBus : public max22200::SpiInterface {  // ❌ Compiler error
    // ...
};
```

### ✅ Correct Template Parameter

```cpp
// CORRECT - pass your class as template parameter
class MyBus : public max22200::SpiInterface<MyBus> {  // ✅
    // ...
};
```

## Testing Your Implementation

After implementing the interface, test it:

```cpp
MyPlatformSPI spi(/* your config */);
max22200::MAX22200 driver(spi);

if (driver.Initialize() == max22200::DriverStatus::OK) {
    // Test basic operations
    max22200::ChannelConfig config;
    config.enabled = true;
    driver.ConfigureChannel(0, config);
}
```

## Next Steps

- See [Configuration](configuration.md) for driver configuration options
- Check [Examples](examples.md) for complete usage examples
- Review [API Reference](api_reference.md) for all available methods

---

**Navigation**
⬅️ [Hardware Setup](hardware_setup.md) | [Next: Configuration ➡️](configuration.md)

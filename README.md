---
layout: default
title: "HardFOC MAX22200 Driver"
description: "Portable C++20 driver for the MAX22200 octal solenoid and motor driver with SPI interface"
nav_order: 1
permalink: /
---

# HF-MAX22200 Driver

**Portable C++20 driver for the MAX22200 octal solenoid and motor driver with SPI interface**

[![C++](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![License](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![CI](https://github.com/N3b3x/hf-max22200-driver/actions/workflows/esp32-examples-build-ci.yml/badge.svg?branch=main)](https://github.com/N3b3x/hf-max22200-driver/actions/workflows/esp32-examples-build-ci.yml)
[![Docs](https://img.shields.io/badge/docs-GitHub%20Pages-blue)](https://n3b3x.github.io/hf-max22200-driver/)

## 📚 Table of Contents
1. [Overview](#-overview)
2. [Features](#-features)
3. [Quick Start](#-quick-start)
4. [Installation](#-installation)
5. [API Reference](#-api-reference)
6. [Examples](#-examples)
7. [Documentation](#-documentation)
8. [Contributing](#-contributing)
9. [License](#-license)

## 📦 Overview

> **📖 [📚🌐 Live Complete Documentation](https://n3b3x.github.io/hf-max22200-driver/)** - 
> Interactive guides, examples, and step-by-step tutorials

**HF-MAX22200** is a portable C++20 driver for the **MAX22200** octal (eight-channel) solenoid and motor driver IC. The MAX22200 features eight half-bridges, each capable of handling up to 36V and 1A RMS, with integrated current sensing, current/voltage regulation, and comprehensive protection features.

The driver uses a CRTP-based `SpiInterface` for hardware abstraction, allowing it to run on any platform (ESP32, STM32, Arduino, etc.) with zero runtime overhead. It implements all major features from the MAX22200 datasheet including channel configuration, current/voltage regulation modes, integrated current sensing (ICS), fault detection, and callback support for event-driven programming.

## ✨ Features

- ✅ **Eight Half-Bridges**: Each channel handles up to 36V and 1A RMS
- ✅ **Current and Voltage Regulation**: Supports both CDR (Current Drive Regulation) and VDR (Voltage Drive Regulation) modes
- ✅ **Integrated Current Sensing (ICS)**: Real-time current monitoring without external sense resistors
- ✅ **Half-Bridge and Full-Bridge Modes**: Flexible bridge configurations
- ✅ **HIT/HOLD Current Control**: Programmable hit current, hold current, and hit time
- ✅ **Comprehensive Protection**: OCP, OL, DPM, UVLO, TSD, and fault registers
- ✅ **Hardware Agnostic**: SPI interface for platform independence
- ✅ **Modern C++**: C++20 with CRTP-based design
- ✅ **Zero Overhead**: CRTP-based design for compile-time polymorphism
- ✅ **Callback Support**: Event-driven programming with fault and state callbacks
- ✅ **Statistics Tracking**: Runtime performance and error monitoring

## 🚀 Quick Start

```cpp
#include "max22200.hpp"

// 1. Implement the SPI interface (see platform_integration.md)
class MySpi : public max22200::SpiInterface<MySpi> {
public:
    void transfer(const uint8_t* tx, uint8_t* rx, size_t len) {
        // Your SPI transfer implementation
    }
};

// 2. Create driver instance
MySpi spi;
max22200::MAX22200 driver(spi);

// 3. Initialize
if (driver.Initialize() == max22200::DriverStatus::OK) {
    // 4. Configure channel 0
    max22200::ChannelConfig config;
    config.enabled = true;
    config.drive_mode = max22200::DriveMode::CDR;
    config.hit_current = 500;
    config.hold_current = 200;
    config.hit_time = 1000;
    
    driver.ConfigureChannel(0, config);
    driver.EnableChannel(0, true);
}
```

For detailed setup, see [Installation](docs/installation.md) and [Quick Start Guide](docs/quickstart.md).

## 🔧 Installation

1. **Clone or copy** the driver files into your project
2. **Implement the SPI interface** for your platform (see [Platform Integration](docs/platform_integration.md))
3. **Include the header** in your code:
   ```cpp
   #include "max22200.hpp"
   ```
4. Compile with a **C++20** or newer compiler

For detailed installation instructions, see [docs/installation.md](docs/installation.md).

## 📖 API Reference

| Method | Description |
|--------|-------------|
| `Initialize()` | Initialize the driver |
| `Deinitialize()` | Safely shut down the driver |
| `Reset()` | Perform software reset |
| `ConfigureChannel()` | Configure a channel |
| `EnableChannel()` | Enable/disable a channel |
| `GetChannelStatus()` | Get channel status |
| `GetFaultStatus()` | Get fault status |
| `ReadCurrent()` | Read current via ICS |
| `SetSleepMode()` | Enable/disable sleep mode |
| `SetFaultCallback()` | Set fault callback function |
| `GetStatistics()` | Get driver statistics |

For complete API documentation, see [docs/api_reference.md](docs/api_reference.md).

## 📊 Examples

For ESP32 examples, see the [examples/esp32](examples/esp32/) directory.
Additional examples for other platforms are available in the [examples](examples/) directory.

Detailed example walkthroughs are available in [docs/examples.md](docs/examples.md).

## 📚 Documentation

For complete documentation, see the [docs directory](docs/index.md).

## 🤝 Contributing

Pull requests and suggestions are welcome! Please follow the existing code style and include tests for new features.

## 📄 License

This project is licensed under the **GNU General Public License v3.0**.
See the [LICENSE](LICENSE) file for details.


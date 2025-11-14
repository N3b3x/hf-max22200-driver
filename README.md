---
layout: default
title: "HardFOC MAX22200 Driver"
description: "The MAX22200 is an octal (eight-channel) solenoid and motor driver featuring:"
nav_order: 1
permalink: /
---

# MAX22200 Driver Library
**A comprehensive C++20 driver library for the MAX22200 octal solenoid and motor driver IC**

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)

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

The MAX22200 is an octal (eight-channel) solenoid and motor driver featuring:
- **Eight Half-Bridges**: Each capable of handling up to 36V and 1A RMS
- **Current and Voltage Regulation**: Supports both CDR and VDR modes
- **Integrated Lossless Current Sensing (ICS)**: Real-time current monitoring
- **High-Speed SPI Interface**: Up to 10MHz communication
- **Comprehensive Protection**: OCP, OL, DPM, UVLO, TSD, and fault registers

## ✨ Features

- ✅ **Hardware Agnostic**: Abstract SPI interface for platform independence
- ✅ **Modern C++20**: Utilizes latest C++ features for efficiency and safety
- ✅ **Exception-Free**: Designed for embedded systems without exceptions
- ✅ **Comprehensive Documentation**: Doxygen comments and markdown guides
- ✅ **Callback Support**: Event-driven programming with fault and state callbacks
- ✅ **Statistics Tracking**: Runtime performance and error monitoring
- ✅ **Type Safety**: Strong typing with enums and structures
- ✅ **Memory Efficient**: No dynamic allocations, suitable for embedded systems

## 🚀 Quick Start

### 1. Clone the Repository

```bash
git clone <repository-url>
cd hf-max22200
```

### 2. Build the Library

```bash
# Using Make
make all

# Using CMake
mkdir build
cd build
cmake ..
make
```

### 3. Run the Example

```bash
make run
```

## Directory Structure

```
hf-max22200/
├── include/                 # Header files
│   ├── MAX22200.h          # Main driver class
│   ├── SpiInterface.h      # Abstract SPI interface
│   ├── MAX22200_Registers.h # Register definitions
│   └── MAX22200_Types.h    # Type definitions
├── src/                    # Source files
│   └── MAX22200.cpp       # Driver implementation
├── examples/               # Example code
│   ├── example_usage.cpp  # Usage examples
│   ├── ExampleSPI.h       # Example SPI implementation
│   └── ExampleSPI.cpp     # Example SPI implementation
├── docs/                   # Documentation
│   ├── README.md          # Main documentation
│   ├── api_reference.md   # API documentation
│   ├── hardware_guide.md  # Hardware integration guide
│   └── ascii_diagrams.md  # ASCII art diagrams
├── CMakeLists.txt         # CMake build configuration
├── Makefile               # Make build configuration
└── Datasheet/             # IC datasheet
    └── MAX22200.pdf       # MAX22200 datasheet
```

## Usage Example

```cpp
#include "MAX22200.h"
#include "MySPI.h"  // Your SPI implementation

// Create SPI interface
MySPI spi;

// Create MAX22200 driver
MAX22200 driver(spi);

// Initialize
if (driver.Initialize() == DriverStatus::OK) {
    // Configure channel 0
    ChannelConfig config;
    config.enabled = true;
    config.drive_mode = DriveMode::CDR;
    config.hit_current = 500;
    config.hold_current = 200;
    config.hit_time = 1000;
    
    driver.ConfigureChannel(0, config);
    driver.EnableChannel(0, true);
}
```

## 🔧 Installation

1. Clone the repository
2. Copy the driver files into your project
3. Implement the SPI interface for your platform
4. Include the driver header in your code

**Requirements:**
- **C++20 Compiler**: GCC 10+, Clang 12+, or MSVC 2019+
- **SPI Interface**: Platform-specific SPI implementation
- **Memory**: ~2KB RAM, ~8KB Flash (approximate)

## 📖 API Reference

For complete API documentation, see the [docs/api_reference.md](docs/api_reference.md) file.

## 📊 Examples

For ESP32 examples, see the [examples/esp32](examples/esp32/) directory. Additional examples and SPI implementations are available in the [examples](examples/) directory.

## 📚 Documentation

- [API Reference](docs/api_reference.md) - Complete API documentation
- [Hardware Guide](docs/hardware_guide.md) - Hardware integration guide
- [ASCII Diagrams](docs/ascii_diagrams.md) - Visual representations
- Generate Doxygen documentation: `doxygen _config/Doxyfile`

## 🤝 Contributing

Pull requests and suggestions are welcome! For guidelines, please see the [Contributing](#-contributing) section above or open an issue to discuss your changes.

## 📄 License

This project is licensed under the **GNU General Public License v3.0**.  
See the [LICENSE](LICENSE) file for details.

## Changelog

### Version 1.0.0
- Initial release
- Complete driver implementation
- Hardware abstraction layer
- Comprehensive documentation
- Example implementations
- ASCII diagrams

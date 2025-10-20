# MAX22200 Driver Library

A comprehensive C++20 driver library for the MAX22200 octal solenoid and motor driver IC, designed for embedded systems with hardware abstraction and modern C++ features.

## Overview

The MAX22200 is an octal (eight-channel) solenoid and motor driver featuring:
- **Eight Half-Bridges**: Each capable of handling up to 36V and 1A RMS
- **Current and Voltage Regulation**: Supports both CDR and VDR modes
- **Integrated Lossless Current Sensing (ICS)**: Real-time current monitoring
- **High-Speed SPI Interface**: Up to 10MHz communication
- **Comprehensive Protection**: OCP, OL, DPM, UVLO, TSD, and fault registers

## Features

- ✅ **Hardware Agnostic**: Abstract SPI interface for platform independence
- ✅ **Modern C++20**: Utilizes latest C++ features for efficiency and safety
- ✅ **Exception-Free**: Designed for embedded systems without exceptions
- ✅ **Comprehensive Documentation**: Doxygen comments and markdown guides
- ✅ **Callback Support**: Event-driven programming with fault and state callbacks
- ✅ **Statistics Tracking**: Runtime performance and error monitoring
- ✅ **Type Safety**: Strong typing with enums and structures
- ✅ **Memory Efficient**: No dynamic allocations, suitable for embedded systems

## Quick Start

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
│   ├── SPIInterface.h      # Abstract SPI interface
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
│   ├── API_Reference.md   # API documentation
│   ├── Hardware_Guide.md  # Hardware integration guide
│   └── ASCII_Diagrams.md  # ASCII art diagrams
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
if (driver.initialize() == DriverStatus::OK) {
    // Configure channel 0
    ChannelConfig config;
    config.enabled = true;
    config.drive_mode = DriveMode::CDR;
    config.hit_current = 500;
    config.hold_current = 200;
    config.hit_time = 1000;
    
    driver.configureChannel(0, config);
    driver.enableChannel(0, true);
}
```

## Requirements

- **C++20 Compiler**: GCC 10+, Clang 12+, or MSVC 2019+
- **SPI Interface**: Platform-specific SPI implementation
- **Memory**: ~2KB RAM, ~8KB Flash (approximate)

## Documentation

- [API Reference](docs/API_Reference.md) - Complete API documentation
- [Hardware Guide](docs/Hardware_Guide.md) - Hardware integration guide
- [ASCII Diagrams](docs/ASCII_Diagrams.md) - Visual representations
- [Examples](examples/) - Usage examples and SPI implementations

## Building

### Using Make

```bash
# Build everything
make all

# Build with debug flags
make debug

# Build with release flags
make release

# Clean build files
make clean

# Run example
make run
```

### Using CMake

```bash
mkdir build
cd build
cmake ..
make
```

## Installation

```bash
# Install library and headers
make install

# Remove installed files
make uninstall
```

## Platform Support

The library is designed to be platform-agnostic. Example implementations are provided for:

- **STM32**: HAL-based SPI implementation
- **Arduino**: Standard Arduino SPI library
- **ESP32**: ESP-IDF SPI driver
- **Generic**: Example implementation for testing

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests if applicable
5. Submit a pull request

## License

This library is provided as-is for educational and development purposes. Please refer to the MAX22200 datasheet for hardware specifications and limitations.

## Support

For questions and support:
- Check the documentation in the `docs/` folder
- Review the examples in the `examples/` folder
- Refer to the MAX22200 datasheet for hardware details

## Changelog

### Version 1.0.0
- Initial release
- Complete driver implementation
- Hardware abstraction layer
- Comprehensive documentation
- Example implementations
- ASCII diagrams

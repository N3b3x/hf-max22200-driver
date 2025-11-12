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

### 1. Include the Library

```cpp
#include "MAX22200.h"
#include "SPIInterface.h"
```

### 2. Implement SPI Interface

```cpp
class MySPI : public SPIInterface {
public:
    bool initialize() override { /* Your SPI init code */ }
    bool transfer(const uint8_t* tx, uint8_t* rx, size_t len) override { /* Your SPI transfer */ }
    void setChipSelect(bool state) override { /* Your CS control */ }
    bool configure(uint32_t speed, uint8_t mode, bool msb_first) override { /* Your SPI config */ }
    bool isReady() const override { /* Your ready check */ }
};
```

### 3. Create and Use Driver

```cpp
MySPI spi;
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

## Architecture

```
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   Application   │    │   MAX22200      │    │   SPI Interface │
│                 │    │   Driver        │    │   (Platform     │
│                 │◄──►│                 │◄──►│   Specific)     │
└─────────────────┘    └─────────────────┘    └─────────────────┘
                              │
                              ▼
                       ┌─────────────────┐
                       │   MAX22200 IC   │
                       │   (Hardware)    │
                       └─────────────────┘
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
│   ├── README.md          # This file
│   ├── API_Reference.md   # API documentation
│   ├── Hardware_Guide.md  # Hardware integration guide
│   └── ASCII_Diagrams.md  # ASCII art diagrams
├── CMakeLists.txt         # CMake build configuration
└── Datasheet/             # IC datasheet
    └── MAX22200.pdf       # MAX22200 datasheet
```

## Building

### Using CMake

```bash
mkdir build
cd build
cmake ..
make
```

### Manual Compilation

```bash
g++ -std=c++20 -I include -c src/MAX22200.cpp -o MAX22200.o
g++ -std=c++20 -I include examples/example_usage.cpp examples/ExampleSPI.cpp MAX22200.o -o example
```

## Documentation

- [API Reference](API_Reference.md) - Complete API documentation
- [Hardware Guide](Hardware_Guide.md) - Hardware integration guide
- [ASCII Diagrams](ASCII_Diagrams.md) - Visual representations
- [Examples](../examples/) - Usage examples and SPI implementations

## Requirements

- **C++20 Compiler**: GCC 10+, Clang 12+, or MSVC 2019+
- **SPI Interface**: Platform-specific SPI implementation
- **Memory**: ~2KB RAM, ~8KB Flash (approximate)

## License

This library is provided as-is for educational and development purposes. Please refer to the MAX22200 datasheet for hardware specifications and limitations.

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests if applicable
5. Submit a pull request

## Support

For questions and support:
- Check the documentation in the `docs/` folder
- Review the examples in the `examples/` folder
- Refer to the MAX22200 datasheet for hardware details

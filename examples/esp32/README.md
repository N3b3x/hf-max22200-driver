# MAX22200 ESP32-C6 Comprehensive Test Suite

This directory contains comprehensive test suites for the MAX22200 octal solenoid and motor driver using the ESP32-C6 DevKit-M-1.

## 📋 Table of Contents

- [Hardware Overview](#-hardware-overview)
- [Pin Connections](#-pin-connections)
- [Hardware Setup](#-hardware-setup)
- [Building the Tests](#-building-the-tests)
- [Running the Tests](#-running-the-tests)
- [Test Suites](#-test-suites)
- [Troubleshooting](#-troubleshooting)

---

## 🔌 Hardware Overview

### ESP32-C6 DevKit-M-1

The ESP32-C6 DevKit-M-1 serves as the host controller for communicating with the MAX22200 driver via SPI.

```
┌─────────────────────────────────────────────────┐
│        ESP32-C6 DevKit-M-1                      │
│                                                 │
│  ┌──────────────────────────────────────────┐   │
│  │        ESP32-C6 Microcontroller          │   │
│  │                                          │   │
│  │  GPIO Pins:                              │   │
│  │  • SPI: MOSI (GPIO7), MISO (GPIO2),      │   │
│  │          SCLK (GPIO6), CS (GPIO10)       │   │
│  │  • Test Indicator: GPIO14                │   │
│  └──────────────────────────────────────────┘   │
│                                                 │
│  USB-C Connector                                │
│  (Power + Serial Communication)                 │
└─────────────────────────────────────────────────┘
```

### MAX22200 Octal Solenoid and Motor Driver

The MAX22200 is an octal (eight-channel) solenoid and motor driver featuring:
- **Eight Half-Bridges**: Each capable of handling up to 36V and 1A RMS
- **Current and Voltage Regulation**: Supports both CDR and VDR modes
- **Integrated Lossless Current Sensing (ICS)**: Real-time current monitoring
- **High-Speed SPI Interface**: Up to 10MHz communication
- **Comprehensive Protection**: OCP, OL, DPM, UVLO, TSD, and fault registers

```
┌─────────────────────────────────────────────────┐
│      MAX22200 Octal Driver                     │
│                                                 │
│  ┌──────────────────────────────────────────┐   │
│  │        MAX22200 IC                       │   │
│  │                                          │   │
│  │  Features:                               │   │
│  │  • 8 half-bridge channels (OUT0-OUT7)    │   │
│  │  • SPI interface (up to 10MHz)           │   │
│  │  • Current regulation (CDR mode)         │   │
│  │  • Voltage regulation (VDR mode)         │   │
│  │  • Integrated current sensing (ICS)     │   │
│  │  • Fault detection and reporting         │   │
│  │  • Thermal shutdown protection          │   │
│  └──────────────────────────────────────────┘   │
│                                                 │
│  SPI Connections:                               │
│  • MOSI (Master Out Slave In)                   │
│  • MISO (Master In Slave Out)                   │
│  • SCLK (Serial Clock)                          │
│  • CS (Chip Select)                             │
│                                                 │
│  Power Connections:                             │
│  • VCC (3.3V/5V Logic Power)                    │
│  • VDD (12V-36V Load Power)                     │
│  • GND (Ground)                                 │
│                                                 │
│  Control Pins:                                  │
│  • RESET (Reset Input)                          │
│  • FAULT (Fault Status Output)                  │
│  • DIAG (Diagnostic Output)                     │
│  • SHDN (Shutdown Input)                        │
└─────────────────────────────────────────────────┘
```

---

## 📌 Pin Connections

### SPI Bus Connections

| MAX22200 Pin | ESP32-C6 GPIO | Function | Notes |
|--------------|---------------|----------|-------|
| MOSI | GPIO7 | SPI Data Out | Master to Slave |
| MISO | GPIO2 | SPI Data In | Slave to Master |
| SCLK | GPIO6 | SPI Clock | Clock signal |
| CS | GPIO10 | Chip Select | Active low |
| VCC | 3.3V | Logic Power | 3.3V or 5V |
| VDD | 12V-36V | Load Power | For solenoids/motors |
| GND | GND | Ground | Common ground |
| RESET | GPIO5 (optional) | Reset Input | Active low |
| FAULT | GPIO4 (optional) | Fault Status | Open-drain, requires pull-up |
| DIAG | GPIO3 (optional) | Diagnostic | Open-drain, requires pull-up |
| SHDN | GPIO1 (optional) | Shutdown | Active low |

### Test Indicator

| Signal | ESP32-C6 GPIO | Function |
|--------|---------------|----------|
| Test Progress | GPIO14 | Visual test progression indicator |

### SPI Configuration

Default SPI configuration (can be modified in test file):

- **Mode**: Mode 0 (CPOL=0, CPHA=0) or Mode 3 (CPOL=1, CPHA=1)
- **Bit Order**: MSB first
- **Clock Speed**: Up to 10MHz (5MHz with daisy chaining)
- **Data Width**: 8-bit transfers
- **Chip Select**: Active low, controlled by software

---

## 🛠️ Hardware Setup

### Basic Setup

1. **Connect SPI Bus**:
   - Connect MAX22200 MOSI to ESP32-C6 GPIO7
   - Connect MAX22200 MISO to ESP32-C6 GPIO2
   - Connect MAX22200 SCLK to ESP32-C6 GPIO6
   - Connect MAX22200 CS to ESP32-C6 GPIO10

2. **Power Connections**:
   - Connect MAX22200 VCC to ESP32-C6 3.3V (logic power)
   - Connect MAX22200 VDD to 12V-36V power supply (load power)
   - Connect MAX22200 GND to ESP32-C6 GND (common ground)

3. **Optional Control Pins**:
   - Connect MAX22200 RESET to ESP32-C6 GPIO5 (optional, can use software reset)
   - Connect MAX22200 FAULT to ESP32-C6 GPIO4 (optional, for fault monitoring)
   - Connect MAX22200 DIAG to ESP32-C6 GPIO3 (optional, for diagnostics)
   - Connect MAX22200 SHDN to ESP32-C6 GPIO1 (optional, for hardware shutdown)

4. **Load Connections**:
   - Connect solenoids or motors to OUT0-OUT7 pins
   - Ensure proper current ratings (max 1A RMS per channel)
   - Use appropriate flyback diodes for inductive loads

### Test Setup

For comprehensive testing, you can connect:
- Solenoids or motors to output channels (with appropriate current ratings)
- Current measurement equipment for ICS verification
- Logic analyzer on SPI bus for protocol verification
- Oscilloscope for timing analysis

---

## 🚀 Building the Tests

### Prerequisites

1. **Install ESP-IDF** (if not already installed):

   ```bash
   # Clone ESP-IDF
   git clone --recursive https://github.com/espressif/esp-idf.git
   cd esp-idf
   
   # Checkout release version 5.5
   git checkout release/v5.5
   git submodule update --init --recursive
   
   # Install ESP-IDF (Linux/macOS)
   ./install.sh esp32c6
   
   # Set up environment (add to ~/.bashrc or ~/.zshrc for persistence)
   . ./export.sh
   ```

2. **Navigate to ESP32 Examples**:

   ```bash
   cd examples/esp32
   ```

3. **Setup Repository** (First time only):

   ```bash
   # Make scripts executable and setup the build environment
   chmod +x scripts/*.sh
   ./scripts/setup_repo.sh
   ```

### Available Test Applications

The test suites use a centralized build system with scripts. Available applications:

| **Application Name** | **Description** | **Hardware Required** |
|----------------------|----------------|----------------------|
| `max22200_comprehensive_test` | Comprehensive MAX22200 driver testing with all features | MAX22200 board |

### List Available Applications

```bash
# List all available applications
./scripts/build_app.sh list
```

### Build an Application

```bash
# Build comprehensive test (Debug build)
./scripts/build_app.sh max22200_comprehensive_test Debug

# Build comprehensive test (Release build)
./scripts/build_app.sh max22200_comprehensive_test Release
```

---

## 📤 Running the Tests

### Flash Application

```bash
# Flash the application to ESP32-C6
./scripts/flash_app.sh max22200_comprehensive_test Debug

# Or manually:
idf.py -p /dev/ttyUSB0 flash
```

### Monitor Output

```bash
# Monitor serial output
idf.py -p /dev/ttyUSB0 monitor

# Or use the flash script which includes monitoring
./scripts/flash_app.sh max22200_comprehensive_test Debug
```

### Auto-detect Port

```bash
# The scripts can auto-detect the port
./scripts/detect_ports.sh
```

---

## 🧪 Test Suites

### Comprehensive Test Suite

**Application**: `max22200_comprehensive_test`

This comprehensive test suite validates all MAX22200 functionality:

#### Test Sections

1. **Initialization Tests**
   - SPI bus initialization
   - Driver initialization
   - Reset to default state
   - Global configuration

2. **Channel Configuration Tests**
   - Single channel configuration
   - Multiple channel configuration
   - All channels configuration
   - Channel enable/disable

3. **Current Control Tests**
   - Hit current setting
   - Hold current setting
   - Current reading (ICS)
   - Current regulation (CDR mode)

4. **Voltage Control Tests**
   - Voltage regulation (VDR mode)
   - Voltage setting and reading

5. **Drive Mode Tests**
   - CDR (Current Drive Regulation) mode
   - VDR (Voltage Drive Regulation) mode
   - Bridge mode configuration

6. **Fault Detection Tests**
   - Overcurrent protection (OCP)
   - Overload (OL) detection
   - Dynamic power management (DPM)
   - Thermal shutdown (TSD)
   - Under-voltage lockout (UVLO)

7. **Diagnostic Tests**
   - Diagnostic mode enable/disable
   - Fault status reading
   - Fault callback registration

8. **Statistics Tests**
   - Operation statistics tracking
   - Error counting
   - Performance metrics

9. **Error Handling Tests**
   - Invalid channel handling
   - Error flag management
   - Error recovery

10. **Stress Tests**
    - Rapid channel operations
    - Continuous read/write cycles
    - Multi-channel simultaneous operations

#### Test Configuration

You can enable/disable specific test sections by editing the test file:

```cpp
// In MAX22200ComprehensiveTest.cpp
static constexpr bool ENABLE_BASIC_TESTS = true;
// ... etc
```

#### Test Results

The test framework provides:
- Automatic pass/fail tracking
- Execution time measurement
- GPIO14 progress indicator (toggles on each test)
- Comprehensive test summary
- Success percentage calculation

---

## 🔧 Configuration

### SPI Bus Configuration

Default SPI configuration (can be modified in test file):

```cpp
// SPI configuration for MAX22200
// Mode: 0 (CPOL=0, CPHA=0) or 3 (CPOL=1, CPHA=1)
// Speed: Up to 10MHz
// Bit Order: MSB first
```

### MAX22200 Configuration

Default MAX22200 settings (can be modified in test file):

```cpp
// Global configuration
GlobalConfig global_config;
global_config.diagnostic_enable = true;
global_config.ics_enable = true;
global_config.daisy_chain_mode = false;
global_config.sleep_mode = false;

// Channel configuration
ChannelConfig channel_config;
channel_config.enabled = true;
channel_config.drive_mode = DriveMode::CDR;
channel_config.hit_current = 500;  // mA
channel_config.hold_current = 200; // mA
```

---

## 🐛 Troubleshooting

### SPI Communication Failures

**Symptoms**: Tests fail with SPI errors

**Solutions**:
1. **Check SPI connections**:
   - Verify MOSI/MISO/SCLK/CS connections
   - Ensure proper power connections
   - Check for loose connections

2. **Verify SPI configuration**:
   - Check SPI mode (0 or 3)
   - Verify clock speed (reduce if using long wires)
   - Ensure CS is properly controlled

3. **Verify power supply**:
   - Ensure 3.3V logic power is stable
   - Check 12V-36V load power is present
   - Check for voltage drops

### Build Errors

**Symptoms**: CMake or compilation errors

**Solutions**:
1. **Verify ESP-IDF version**:
   ```bash
   idf.py --version
   # Should show ESP-IDF v5.5 or compatible
   ```

2. **Clean and rebuild**:
   ```bash
   idf.py fullclean
   ./scripts/build_app.sh max22200_comprehensive_test Debug
   ```

3. **Check component paths**:
   - Verify component CMakeLists.txt paths
   - Ensure source files are accessible

### Test Failures

**Symptoms**: Specific tests fail

**Solutions**:
1. **Check hardware connections**:
   - Verify all pins are properly connected
   - Check for loose connections
   - Verify load connections (solenoids/motors)

2. **Review test logs**:
   - Check which specific test failed
   - Review error messages in serial output

3. **Verify device state**:
   - Reset MAX22200 (power cycle or software reset)
   - Run reset test first

4. **Check current/voltage settings**:
   - Verify current settings are within limits (max 1A RMS)
   - Check voltage supply is within range (12V-36V)

---

## 📚 Additional Resources

- [MAX22200 Datasheet](../../datasheet/MAX22200.pdf)
- [Driver API Documentation](../../docs/api_reference.md)
- [Hardware Integration Guide](../../docs/hardware_guide.md)
- [ASCII Diagrams](../../docs/ascii_diagrams.md)

---

## 🎯 Quick Reference

### Build Commands

```bash
# List available apps
./scripts/build_app.sh list

# Build comprehensive test
./scripts/build_app.sh max22200_comprehensive_test Debug

# Flash and monitor
./scripts/flash_app.sh max22200_comprehensive_test Debug
```

### Test Execution

The comprehensive test suite runs automatically on boot and provides:
- Real-time test progress via GPIO14 indicator
- Serial output with detailed test results
- Automatic test summary at completion

### GPIO14 Test Indicator

GPIO14 toggles between HIGH/LOW for each completed test, providing visual feedback:
- Use oscilloscope or logic analyzer to monitor
- Useful for automated test verification
- Blinks 5 times at section start/end

---

## 📝 Notes

- **SPI Configuration**: Use Mode 0 or Mode 3, MSB first, up to 10MHz clock
- **Power Requirements**: 3.3V/5V for logic, 12V-36V for load power
- **Current Limits**: Maximum 1A RMS per channel
- **Test Duration**: Comprehensive test suite takes approximately 2-5 minutes to complete
- **Hardware Requirements**: Basic tests work without external hardware; some tests require solenoids/motors connected

---

<div style="text-align: center; margin: 2em 0; padding: 1em; background: #f8f9fa; border-radius: 8px;">
  <strong>🎯 Ready to test the MAX22200?</strong><br>
  Start with: <code>./scripts/build_app.sh max22200_comprehensive_test Debug</code>
</div>

# MAX22200 Driver API Reference

This document provides comprehensive API documentation for the MAX22200 driver library.

## Table of Contents

- [Core Classes](#core-classes)
- [Data Types](#data-types)
- [Enumerations](#enumerations)
- [Functions](#functions)
- [Error Handling](#error-handling)
- [Usage Examples](#usage-examples)

## Core Classes

### MAX22200

The main driver class for controlling the MAX22200 IC.

```cpp
class MAX22200 {
public:
    explicit MAX22200(SPIInterface& spi_interface, bool enable_diagnostics = true);
    ~MAX22200();
    
    // Initialization
    DriverStatus initialize();
    DriverStatus deinitialize();
    DriverStatus reset();
    
    // Global Configuration
    DriverStatus configureGlobal(const GlobalConfig& config);
    DriverStatus getGlobalConfig(GlobalConfig& config) const;
    DriverStatus setSleepMode(bool enable);
    DriverStatus setDiagnosticMode(bool enable);
    DriverStatus setIntegratedCurrentSensing(bool enable);
    
    // Channel Configuration
    DriverStatus configureChannel(uint8_t channel, const ChannelConfig& config);
    DriverStatus getChannelConfig(uint8_t channel, ChannelConfig& config) const;
    DriverStatus configureAllChannels(const ChannelConfigArray& configs);
    DriverStatus getAllChannelConfigs(ChannelConfigArray& configs) const;
    
    // Channel Control
    DriverStatus enableChannel(uint8_t channel, bool enable);
    DriverStatus enableAllChannels(bool enable);
    DriverStatus setChannelDriveMode(uint8_t channel, DriveMode mode);
    DriverStatus setChannelBridgeMode(uint8_t channel, BridgeMode mode);
    DriverStatus setChannelPolarity(uint8_t channel, OutputPolarity polarity);
    
    // Current Control
    DriverStatus setHitCurrent(uint8_t channel, uint16_t current);
    DriverStatus setHoldCurrent(uint8_t channel, uint16_t current);
    DriverStatus setCurrents(uint8_t channel, uint16_t hit_current, uint16_t hold_current);
    DriverStatus getCurrents(uint8_t channel, uint16_t& hit_current, uint16_t& hold_current) const;
    
    // Timing Control
    DriverStatus setHitTime(uint8_t channel, uint16_t time);
    DriverStatus getHitTime(uint8_t channel, uint16_t& time) const;
    
    // Status and Diagnostics
    DriverStatus readFaultStatus(FaultStatus& status) const;
    DriverStatus clearFaultStatus();
    DriverStatus readChannelStatus(uint8_t channel, ChannelStatus& status) const;
    DriverStatus readAllChannelStatuses(ChannelStatusArray& statuses) const;
    DriverStatus getStatistics(DriverStatistics& stats) const;
    DriverStatus resetStatistics();
    
    // Callbacks
    void setFaultCallback(FaultCallback callback, void* user_data = nullptr);
    void setStateChangeCallback(StateChangeCallback callback, void* user_data = nullptr);
    
    // Utility
    bool isInitialized() const;
    static constexpr bool isValidChannel(uint8_t channel);
    static constexpr const char* getVersion();
};
```

### SPIInterface

Abstract base class for SPI communication.

```cpp
class SPIInterface {
public:
    virtual ~SPIInterface() = default;
    virtual bool initialize() = 0;
    virtual bool transfer(const uint8_t* tx_data, uint8_t* rx_data, size_t length) = 0;
    virtual void setChipSelect(bool state) = 0;
    virtual bool configure(uint32_t speed_hz, uint8_t mode, bool msb_first = true) = 0;
    virtual bool isReady() const = 0;
};
```

## Data Types

### ChannelConfig

Configuration structure for a single channel.

```cpp
struct ChannelConfig {
    bool enabled;                    ///< Channel enable state
    DriveMode drive_mode;           ///< Drive mode (CDR or VDR)
    BridgeMode bridge_mode;         ///< Bridge mode (half or full)
    bool parallel_mode;             ///< Parallel mode enable
    OutputPolarity polarity;        ///< Output polarity
    uint16_t hit_current;           ///< HIT current setting (0-1023)
    uint16_t hold_current;          ///< HOLD current setting (0-1023)
    uint16_t hit_time;              ///< HIT time setting (0-65535)
    
    ChannelConfig();  // Default constructor
    ChannelConfig(bool en, DriveMode dm, BridgeMode bm, bool pm, 
                  OutputPolarity pol, uint16_t hc, uint16_t hdc, uint16_t ht);
};
```

### GlobalConfig

Global configuration structure.

```cpp
struct GlobalConfig {
    bool reset;                     ///< Reset state
    bool sleep_mode;                ///< Sleep mode enable
    bool diagnostic_enable;         ///< Diagnostic enable
    bool ics_enable;                ///< Integrated Current Sensing enable
    bool daisy_chain_mode;          ///< Daisy chain mode enable
    
    GlobalConfig();  // Default constructor
};
```

### FaultStatus

Fault status information.

```cpp
struct FaultStatus {
    bool overcurrent_protection;    ///< Overcurrent protection fault
    bool open_load;                 ///< Open load detection fault
    bool plunger_movement;          ///< Detection of plunger movement
    bool undervoltage_lockout;      ///< Undervoltage lockout fault
    bool hit_current_not_reached;   ///< HIT current not reached fault
    bool thermal_shutdown;          ///< Thermal shutdown fault
    
    FaultStatus();  // Default constructor
    bool hasFault() const;          ///< Check if any fault is active
    uint8_t getFaultCount() const;  ///< Get fault count
};
```

### ChannelStatus

Current status of a single channel.

```cpp
struct ChannelStatus {
    bool enabled;                   ///< Channel enable state
    bool fault_active;              ///< Channel fault state
    uint16_t current_reading;       ///< Current reading from ICS
    bool hit_phase_active;          ///< HIT phase active state
    
    ChannelStatus();  // Default constructor
};
```

### DriverStatistics

Runtime statistics for the driver.

```cpp
struct DriverStatistics {
    uint32_t total_transfers;       ///< Total number of SPI transfers
    uint32_t failed_transfers;      ///< Number of failed SPI transfers
    uint32_t fault_events;          ///< Number of fault events
    uint32_t state_changes;         ///< Number of state changes
    uint32_t uptime_ms;             ///< Driver uptime in milliseconds
    
    DriverStatistics();  // Default constructor
    float getSuccessRate() const;   ///< Get transfer success rate
};
```

## Enumerations

### DriverStatus

Driver operation status codes.

```cpp
enum class DriverStatus : uint8_t {
    OK = 0,                 ///< Operation successful
    INITIALIZATION_ERROR,   ///< Initialization failed
    COMMUNICATION_ERROR,    ///< SPI communication error
    INVALID_PARAMETER,      ///< Invalid parameter provided
    HARDWARE_FAULT,         ///< Hardware fault detected
    TIMEOUT                 ///< Operation timeout
};
```

### DriveMode

Channel drive mode.

```cpp
enum class DriveMode : uint8_t {
    CDR = 0,    ///< Current Drive Regulation
    VDR = 1     ///< Voltage Drive Regulation
};
```

### BridgeMode

Bridge configuration mode.

```cpp
enum class BridgeMode : uint8_t {
    HALF_BRIDGE = 0,    ///< Half-bridge mode
    FULL_BRIDGE = 1     ///< Full-bridge mode
};
```

### OutputPolarity

Output polarity setting.

```cpp
enum class OutputPolarity : uint8_t {
    NORMAL = 0,     ///< Normal polarity
    INVERTED = 1    ///< Inverted polarity
};
```

### FaultType

Types of faults that can occur.

```cpp
enum class FaultType : uint8_t {
    OCP = 0,    ///< Overcurrent protection
    OL = 1,     ///< Open load detection
    DPM = 2,    ///< Detection of plunger movement
    UVLO = 3,   ///< Undervoltage lockout
    HHF = 4,    ///< HIT current not reached
    TSD = 5     ///< Thermal shutdown
};
```

### ChannelState

Channel operational state.

```cpp
enum class ChannelState : uint8_t {
    DISABLED = 0,       ///< Channel is disabled
    ENABLED,            ///< Channel is enabled
    HIT_PHASE,          ///< Channel is in HIT phase
    HOLD_PHASE,         ///< Channel is in HOLD phase
    FAULT               ///< Channel has a fault
};
```

## Functions

### Callback Functions

#### FaultCallback

Function type for fault event callbacks.

```cpp
using FaultCallback = void(*)(uint8_t channel, FaultType fault_type, void* user_data);
```

**Parameters:**
- `channel`: Channel number where fault occurred (0-7)
- `fault_type`: Type of fault that occurred
- `user_data`: User-provided data pointer

#### StateChangeCallback

Function type for channel state change callbacks.

```cpp
using StateChangeCallback = void(*)(uint8_t channel, ChannelState old_state, 
                                   ChannelState new_state, void* user_data);
```

**Parameters:**
- `channel`: Channel number (0-7)
- `old_state`: Previous channel state
- `new_state`: New channel state
- `user_data`: User-provided data pointer

## Error Handling

The driver uses return codes instead of exceptions for error handling, making it suitable for embedded systems.

### Return Code Usage

```cpp
DriverStatus status = driver.initialize();
if (status != DriverStatus::OK) {
    // Handle error
    switch (status) {
        case DriverStatus::INITIALIZATION_ERROR:
            // Handle initialization error
            break;
        case DriverStatus::COMMUNICATION_ERROR:
            // Handle SPI communication error
            break;
        case DriverStatus::INVALID_PARAMETER:
            // Handle invalid parameter
            break;
        // ... other cases
    }
}
```

### Error Recovery

```cpp
// Clear faults and retry
driver.clearFaultStatus();
DriverStatus status = driver.initialize();
if (status == DriverStatus::OK) {
    // Success
} else {
    // Still failed, take appropriate action
}
```

## Usage Examples

### Basic Initialization

```cpp
#include "MAX22200.h"
#include "MySPI.h"  // Your SPI implementation

MySPI spi;
MAX22200 driver(spi);

if (driver.initialize() == DriverStatus::OK) {
    // Driver ready for use
}
```

### Channel Configuration

```cpp
// Configure channel 0 for solenoid control
ChannelConfig config;
config.enabled = true;
config.drive_mode = DriveMode::CDR;
config.bridge_mode = BridgeMode::HALF_BRIDGE;
config.hit_current = 800;
config.hold_current = 200;
config.hit_time = 1000;

DriverStatus status = driver.configureChannel(0, config);
if (status == DriverStatus::OK) {
    driver.enableChannel(0, true);
}
```

### Fault Handling

```cpp
// Set up fault callback
driver.setFaultCallback([](uint8_t channel, FaultType fault_type, void* user_data) {
    printf("Fault on channel %d: %d\n", channel, static_cast<int>(fault_type));
}, nullptr);

// Read fault status
FaultStatus status;
if (driver.readFaultStatus(status) == DriverStatus::OK) {
    if (status.hasFault()) {
        printf("Active faults: %d\n", status.getFaultCount());
    }
}
```

### Statistics Monitoring

```cpp
DriverStatistics stats;
if (driver.getStatistics(stats) == DriverStatus::OK) {
    printf("Success rate: %.2f%%\n", stats.getSuccessRate());
    printf("Total transfers: %u\n", stats.total_transfers);
}
```

## Constants

### Current Ranges

```cpp
namespace CurrentRange {
    constexpr uint16_t MIN_HIT_CURRENT  = 0;     ///< Minimum HIT current
    constexpr uint16_t MAX_HIT_CURRENT  = 1023;  ///< Maximum HIT current
    constexpr uint16_t MIN_HOLD_CURRENT = 0;     ///< Minimum HOLD current
    constexpr uint16_t MAX_HOLD_CURRENT = 1023;  ///< Maximum HOLD current
}
```

### Timing Ranges

```cpp
namespace TimingRange {
    constexpr uint16_t MIN_HIT_TIME     = 0;     ///< Minimum HIT time
    constexpr uint16_t MAX_HIT_TIME     = 65535; ///< Maximum HIT time
}
```

### SPI Frequencies

```cpp
constexpr uint32_t MAX_SPI_FREQ_STANDALONE = 10000000; // 10 MHz
constexpr uint32_t MAX_SPI_FREQ_DAISY_CHAIN = 5000000;  // 5 MHz
```

## Thread Safety

The driver is not thread-safe by default. If multiple threads need to access the driver, external synchronization must be provided.

## Memory Usage

- **RAM**: Approximately 2KB for driver instance and buffers
- **Flash**: Approximately 8KB for code and constants
- **Stack**: Minimal stack usage, suitable for embedded systems

## Performance

- **SPI Transfer**: ~1-2ms per register read/write
- **Channel Configuration**: ~5-10ms for full channel setup
- **Fault Detection**: ~1ms for fault status read
- **Current Sensing**: Real-time when ICS enabled

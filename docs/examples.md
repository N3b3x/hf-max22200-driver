---
layout: default
title: "💡 Examples"
description: "Example walkthroughs for the MAX22200 driver"
nav_order: 7
parent: "📚 Documentation"
permalink: /docs/examples/
---

# Examples

Working examples using the current driver API (STATUS, CFG_CHx, FAULT, CFG_DPM, and types in `max22200_types.hpp`).

## Example 1: Basic Solenoid Control (CDR)

CDR mode, low-side, with hit/hold current and HIT time.

```cpp
#include "max22200.hpp"

void app_main() {
    // SPI setup (implement SpiInterface for your platform)
    MySpi spi;
    max22200::MAX22200 driver(spi);

    if (driver.Initialize() != max22200::DriverStatus::OK) {
        printf("Initialization failed\n");
        return;
    }

    max22200::ChannelConfig config;
    config.drive_mode = max22200::DriveMode::CDR;
    config.side_mode = max22200::SideMode::LOW_SIDE;
    config.hit_setpoint = 800.0f;  // mA
    config.hold_setpoint = 200.0f;
    config.hit_time_ms = 50.0f;
    // IFS from SetBoardConfig; not set on ChannelConfig
    config.master_clock_80khz = false;
    config.chop_freq = max22200::ChopFreq::FMAIN_DIV2;
    config.hit_current_check_enabled = true;

    driver.ConfigureChannel(0, config);
    driver.EnableChannel(0);

    printf("Solenoid enabled\n");
}
```

## Example 2: Motor with H-Bridge Pair

Use channel-pair mode HBRIDGE and set full-bridge state.

```cpp
#include "max22200.hpp"

void app_main() {
    MySpi spi;
    max22200::MAX22200 driver(spi);
    driver.Initialize();

    // Set pair 0 (ch0–ch1) to H-bridge; both channels must be off first
    max22200::StatusConfig status;
    driver.ReadStatus(status);
    status.channels_on_mask = 0;
    status.channel_pair_mode_10 = max22200::ChannelMode::HBRIDGE;
    driver.WriteStatus(status);

    // Configure both channels (forward uses ch1 config, reverse uses ch0)
    max22200::ChannelConfig cfg;
    cfg.drive_mode = max22200::DriveMode::CDR;
    cfg.side_mode = max22200::SideMode::LOW_SIDE;
    cfg.hit_setpoint = 500.0f;
    cfg.hold_setpoint = 500.0f;
    cfg.hit_time_ms = -1.0f;  // Continuous
    // IFS from SetBoardConfig
    cfg.master_clock_80khz = false;
    cfg.chop_freq = max22200::ChopFreq::FMAIN_DIV2;
    driver.ConfigureChannel(0, cfg);
    driver.ConfigureChannel(1, cfg);

    // Drive forward
    driver.SetFullBridgeState(0, max22200::FullBridgeState::Forward);
}
```

## Example 3: Fault Handling and Human-Readable Fault Names

Use `ReadFaultRegister`, probe helpers, and `FaultTypeToStr()` for logging.

```cpp
#include "max22200.hpp"

void fault_handler(uint8_t channel, max22200::FaultType fault_type, void *user_data) {
    printf("Fault on channel %u: %s\n", channel, max22200::FaultTypeToStr(fault_type));
}

void app_main() {
    MySpi spi;
    max22200::MAX22200 driver(spi);
    driver.Initialize();
    driver.SetFaultCallback(fault_handler);

    max22200::ChannelConfig config;
    config.drive_mode = max22200::DriveMode::CDR;
    config.side_mode = max22200::SideMode::LOW_SIDE;
    config.hit_setpoint = 500.0f;
    config.hold_setpoint = 200.0f;
    config.hit_time_ms = 10.0f;
    // IFS from SetBoardConfig; not set on ChannelConfig
    config.master_clock_80khz = false;
    config.chop_freq = max22200::ChopFreq::FMAIN_DIV2;
    driver.ConfigureChannel(0, config);
    driver.EnableChannel(0);

    for (;;) {
        max22200::FaultStatus faults;
        driver.ReadFaultRegister(faults);

        if (faults.hasFault()) {
            printf("Faults: %u (mask 0x%02X)\n", faults.getFaultCount(), faults.channelsWithAnyFault());
            if (faults.hasOvercurrent()) printf("  Overcurrent\n");
            if (faults.hasHitNotReached()) printf("  HIT not reached\n");
            if (faults.hasOpenLoadFault()) printf("  Open-load\n");
            if (faults.hasPlungerMovementFault()) printf("  Plunger movement\n");
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

## Example 4: Multiple Channels and Status

Use `ReadStatus()` for channel on/off and `ReadFaultRegister()` for per-channel faults.

```cpp
#include "max22200.hpp"

void app_main() {
    MySpi spi;
    max22200::MAX22200 driver(spi);
    driver.Initialize();

    for (uint8_t ch = 0; ch < 8; ch++) {
        max22200::ChannelConfig config;
        config.drive_mode = max22200::DriveMode::CDR;
        config.side_mode = max22200::SideMode::LOW_SIDE;
        config.hit_setpoint = 400.0f;
        config.hold_setpoint = 150.0f;
        config.hit_time_ms = 10.0f;
        // IFS from SetBoardConfig; not set on ChannelConfig
        config.master_clock_80khz = false;
        config.chop_freq = max22200::ChopFreq::FMAIN_DIV2;
        driver.ConfigureChannel(ch, config);
    }

    driver.SetChannelsOn(0x0F);  // Channels 0–3 on

    max22200::StatusConfig status;
    driver.ReadStatus(status);
    printf("Channels on: %u\n", status.channelCountOn());
    for (uint8_t ch = 0; ch < 8; ch++) {
        if (status.isChannelOn(ch)) printf("  Channel %u on\n", ch);
    }

    max22200::FaultStatus faults;
    driver.ReadFaultRegister(faults);
    for (uint8_t ch = 0; ch < 8; ch++) {
        if (faults.hasFaultOnChannel(ch)) {
            printf("  Channel %u fault (OCP=%d HHF=%d OLF=%d DPM=%d)\n",
                   ch, faults.hasOvercurrentOnChannel(ch), faults.hasHitNotReachedOnChannel(ch),
                   faults.hasOpenLoadFaultOnChannel(ch), faults.hasPlungerMovementFaultOnChannel(ch));
        }
    }
}
```

## Example 5: Board Config and Current in mA

Set IFS via `BoardConfig`, then use mA and ms APIs.

```cpp
#include "max22200.hpp"

void app_main() {
    MySpi spi;
    max22200::MAX22200 driver(spi);
    driver.Initialize();

    max22200::BoardConfig board(30.0f, false);
    board.max_current_ma = 800;
    driver.SetBoardConfig(board);

    driver.ConfigureChannelCdr(0, 500, 200, 10.0f);  // 500 mA hit, 200 mA hold, 10 ms
    driver.EnableChannel(0);

    uint32_t hit_ma = 0, hold_ma = 0;
    driver.GetHitCurrentMa(0, hit_ma);
    driver.GetHoldCurrentMa(0, hold_ma);
    printf("Channel 0: hit=%" PRIu32 " mA, hold=%" PRIu32 " mA\n", hit_ma, hold_ma);
}
```

## Example 6: ESP32 Solenoid / Valve Test (All 8 Channels)

The **solenoid valve test** app (`max22200_solenoid_valve_test`) is a dedicated ESP32 application that exercises the driver on valves with full diagnostics. It uses the same C21-style profile on all 8 channels and runs sequential (follow-up) and parallel clicking patterns.

**What it does:**

- Configures all 8 channels with the same valve profile (C21: 100 ms hit, 50% hold, low-side CDR or VDR per `C21ValveConfig`).
- **Sequential pattern**: Enables channel 0, holds 200 ms, disables; then ch1 … ch7 with 80 ms gap between channels.
- **Parallel pattern**: Turns all channels on for 500 ms, then all off.
- Logs **full diagnostics** after init and after each pattern:
  - STATUS (ACTIVE, fault flags, channels on mask)
  - FAULT register (OCP, HHF, OLF, DPM per channel)
  - Last fault byte from Command Register (hex + bit decode)
  - nFAULT pin state
  - Per-channel config readback (raw register + hit/hold/hit_time, CDR/VDR, LS/HS)
  - BoardConfig (IFS, max current, max duty)
  - Driver statistics (transfers, failed, faults, uptime, success %)

**Build and run (from `examples/esp32`):**

```bash
./scripts/build_app.sh max22200_solenoid_valve_test Debug
./scripts/flash_app.sh max22200_solenoid_valve_test Debug
```

Valve profile (CDR vs VDR, hit/hold, hit time) is set in `esp32_max22200_test_config.hpp` via `C21ValveConfig`. See `examples/esp32/README.md` for the full **Solenoid / Valve Test** section.

---

## Running the Examples

### ESP32

```bash
cd examples/esp32
./scripts/build_app.sh max22200_comprehensive_test Debug   # or max22200_solenoid_valve_test
./scripts/flash_app.sh max22200_comprehensive_test Debug
```

### Other Platforms

Implement `max22200::SpiInterface<YourSpi>` for your platform and build with C++20. See [Platform Integration](platform_integration.md).

## Next Steps

- [API Reference](api_reference.md) for all methods and types
- [Configuration](configuration.md) for STATUS, fault masks, and DPM
- [Troubleshooting](troubleshooting.md) for common issues

---

**Navigation**
⬅️ [API Reference](api_reference.md) | [Next: Troubleshooting ➡️](troubleshooting.md) | [Back to Index](index.md)

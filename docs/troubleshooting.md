---
layout: default
title: "🐛 Troubleshooting"
description: "Common issues and solutions for the MAX22200 driver"
nav_order: 8
parent: "📚 Documentation"
permalink: /docs/troubleshooting/
---

# Troubleshooting

This guide helps you diagnose and resolve common issues when using the MAX22200 driver. For a detailed analysis of SPI command bytes, write/read byte order, and decoding (and why raw register HEX comparison is used for pass/fail), see [SPI Protocol Analysis](spi_protocol_analysis.md).

## Common Error Messages

### Error: Initialization Failed

**Symptoms:**

- `Initialize()` returns `INITIALIZATION_ERROR` or `COMMUNICATION_ERROR`
- Driver not responding

**Causes:**

- SPI interface not properly initialized
- Hardware connections incorrect
- Power supply issues

**Solutions:**

1. **Verify SPI Interface**: Ensure SPI interface is initialized before creating driver
2. **Check Connections**: Verify all SPI connections (SCLK, SDI, SDO, CS)
3. **Verify Power**: Check power supply voltage (VM = 5.5V – 36V; VL via internal LDO)
4. **Check CS Line**: Verify chip select is properly controlled
5. **Run the diagnostic**: Flash `examples/esp32/main/max22200_diagnostic.cpp` — it dumps every SPI byte and walks through the datasheet init flow with annotated output. Compare against the "expected healthy output" listed in the file header.

---

### Error: ACTIVE bit reads back as 0 even though `Initialize()` returned OK

**Symptoms:**

- `Initialize()` returns OK
- `IsInitialized()` reports `true`
- But `ReadStatus()` shows `status.active == false` and the chip won't drive any channel
- `nFAULT` may still be asserted (LED on)

**Cause:**

The driver's `Initialize()` returns OK as soon as its STATUS write transaction completes, but the chip's specified `tWU` (wake-up time) is **2.5 ms** from the ACTIVE=1 write to "OUT_ active". On rigs with a slow VL LDO ramp (slow / under-spec V18 bypass cap), ACTIVE may take dozens of ms to physically latch. Until then, channel writes are ignored.

**Solution:**

After `Initialize()`, poll STATUS in a loop, re-issuing a bare ACTIVE=1 write each iteration, until ACTIVE actually reads back as 1. The proven pattern from `c21_cycle_test`:

```cpp
driver->Initialize();                           // datasheet init flow
vTaskDelay(pdMS_TO_TICKS(50));                  // give VL a head-start
StatusConfig st{};
for (uint32_t waited = 0; waited < 2000; waited += 25) {
    driver->WriteRegister32(RegBank::STATUS, 0x00000001U);  // bare ACTIVE
    driver->ReadStatus(st);
    if (st.active && !st.undervoltage) break;
    vTaskDelay(pdMS_TO_TICKS(25));
}
```

If this still doesn't bring ACTIVE up, check:

1. **VL pin bypass capacitor** — the chip's V18 LDO requires a **2.2 µF ceramic** between the V18 pin and GND. Missing or under-spec cap will keep V18 from settling.
2. **VM rail under SPI load** — scope right at the chip's VM pin. Brief brown-outs can collapse V18 and re-set UVM.
3. **Series elements on +VM** — fuse / sense resistor / reverse-polarity diode that drops VM at the chip pin under transient current draw.

---

### Chip stuck in a corrupted state across MCU reboots

**Symptoms:**

- STATUS register reads bizarre values (`0x0180C000`, CM76 = 0b11 = RESERVED, ONCH already set at boot, etc.)
- STATUS reads return *different values on consecutive non-write reads*
- `DPM` fault asserts even though no channel is being driven
- Write-data SDO bytes 2-3 are non-zero (per datasheet Figure 10 they MUST be 0x00)
- Toggling ENABLE LOW for 50 ms does NOT clear the bad state

**Cause:**

Per datasheet §"Undervoltage Lockout (UVLO)": "The content of the logic registers is preserved until the V18 regulator… falls below the digital power-on reset (POR) threshold (typically 1.0 V). When this happens, all registers are reset to the default values."

**Driving ENABLE LOW only enters low-power mode — the register state is preserved.** The only way to truly reset the chip is a full **VM power cycle**.

**Solution:**

1. Disconnect +24V from the MAX22200's VM pin completely.
2. Wait 10 seconds for all decoupling caps to discharge V18 below ~1 V.
3. Reconnect +24V, re-flash, re-run the diagnostic.

If the chip still misbehaves after a clean POR, the chip itself may be damaged — try a fresh known-good MAX22200 in the same socket.

---

### Error: Communication Error

**Symptoms:**

- `COMMUNICATION_ERROR` returned from methods
- No response from device

**Causes:**

- SPI configuration incorrect
- Signal integrity issues
- CS timing problems

**Solutions:**

1. **Check SPI Mode**: Ensure SPI Mode 0 or Mode 3
2. **Verify Speed**: Try lower SPI speed (e.g., 1 MHz)
3. **Check CS Timing**: Verify CS assertion/deassertion timing
4. **Verify Connections**: Check all SPI connections are secure

---

### Error: Channel Not Working

**Symptoms:**

- Channel enabled but no output
- Current not flowing

**Causes:**

- Channel not properly configured
- Channel not enabled
- Load not connected correctly
- Fault condition

**Solutions:**

1. **Check Configuration**: Verify channel configuration is correct
2. **Check Enable State**: Ensure channel is enabled via `EnableChannel()`
3. **Verify Load**: Check load connections (OUTA/OUTB to load)
4. **Check Faults**: Read fault status to identify issues

---

### Error: Overcurrent Protection (OCP)

**Symptoms:**

- `FaultStatus::hasOvercurrent()` is true (or per-channel `hasOvercurrentOnChannel(ch)`)
- nFAULT pin asserted; channel may be disabled depending on mask

**Causes:**

- Load current exceeds limits
- Short circuit
- Incorrect current settings

**Solutions:**

1. **Check Load**: Verify load is within specifications (1A RMS max per channel)
2. **Reduce Current**: Lower hit_setpoint and hold_setpoint (or use set_hit_ma/set_hold_ma or SetHitCurrentMa/SetHoldCurrentMa)
3. **Check for Shorts**: Verify no short circuits in wiring
4. **Clear Fault**: Read fault status to clear, then reconfigure

---

### Error: Open Load Detection

**Symptoms:**

- `FaultStatus.open_load` is true
- No current flow

**Causes:**

- Load not connected
- Broken connection
- Load impedance too high

**Solutions:**

1. **Check Connections**: Verify load is properly connected
2. **Check Wiring**: Inspect for broken wires
3. **Verify Load**: Ensure load impedance is appropriate

---

## Hardware Issues

### Device Not Responding

**Checklist:**

- [ ] Verify power supply voltage is 2.7V - 5.5V
- [ ] Check all SPI connections are secure
- [ ] Verify CS line is properly controlled
- [ ] Check for short circuits
- [ ] Use oscilloscope/logic analyzer to verify bus activity

---

### Incorrect Current Readings

**Symptoms:**

- Current readings don't match expected values
- ICS readings are zero or incorrect

**Causes:**

- ICS not enabled
- Channel not active
- Incorrect configuration

**Solutions:**

1. **Enable ICS**: Call `SetIntegratedCurrentSensing(true)`
2. **Check Channel State**: Verify channel is enabled and active
3. **Verify Configuration**: Check current settings are correct

---

## Software Issues

### Compilation Errors

**Error: "No matching function"**

**Solution:**

- Ensure you've implemented all required SPI interface methods
- Check method signatures match the interface definition

**Error: "Undefined reference"**

**Solution:**

- Verify you're linking the driver source file
- Check include paths are correct

---

### Runtime Errors

**Initialization Fails**

**Checklist:**

- [ ] SPI bus interface is properly initialized
- [ ] Hardware connections are correct
- [ ] Configuration parameters are valid
- [ ] Device is powered and ready

**Unexpected Behavior**

**Checklist:**

- [ ] Verify configuration matches your use case
- [ ] Check for timing issues
- [ ] Review error handling code
- [ ] Check fault status

---

## Debugging Tips

### Enable Diagnostics

```cpp
max22200::MAX22200 driver(spi, true);  // Enable diagnostics
```

### Check Fault Status

```cpp
max22200::FaultStatus faults;
driver.ReadFaultRegister(faults);

if (faults.hasFault()) {
    printf("Faults detected: %d\n", faults.getFaultCount());
}
```

### Read Channel and Fault Status

Use STATUS for channel on/off and FAULT for per-channel fault flags:

```cpp
max22200::StatusConfig status;
driver.ReadStatus(status);
bool ch0_on = status.isChannelOn(0);

max22200::FaultStatus faults;
driver.ReadFaultRegister(faults);
bool ch0_fault = faults.hasFaultOnChannel(0);
if (faults.hasOvercurrent()) { /* ... */ }

// Configured current in mA (after SetBoardConfig)
uint32_t hit_ma = 0;
driver.GetHitCurrentMa(0, hit_ma);
```

### Use Callbacks

Set up fault callbacks to get notified immediately when faults occur:

```cpp
void fault_handler(uint8_t channel, max22200::FaultType fault_type, void *user_data) {
    printf("Fault on channel %d\n", channel);
}

driver.SetFaultCallback(fault_handler);
```

### DPM (Detection of Plunger Movement) polarity

Per datasheet §"Detection of Plunger Movement": "If the drop is **not revealed** a fault indication is output on FAULT pin and a fault bit is asserted in the fault register."

| `FAULT.DPM[ch]` value | Meaning                                          |
|-----------------------|--------------------------------------------------|
| `0`                   | Current dip detected → plunger MOVED (healthy)   |
| `1`                   | Drop NOT revealed → plunger STUCK (fault)        |

So a healthy free-moving valve produces **zero DPM fires** across many cycles. To confirm the DPM algorithm is actually working, hold the plunger physically still during a cycle — DPM should then fire (`= 1`) for that cycle. See `examples/esp32/main/c21_dpm_tuning_test.cpp` for a full tuning workflow with CSV-format current/fault sampling.

---

## FAQ

### Q: What is the maximum current per channel?

**A:** Each channel can handle up to 1A RMS. ChannelConfig stores currents in user units: for CDR use hit_setpoint and hold_setpoint in mA; call SetBoardConfig(BoardConfig(rref, hfs)) first so the driver uses board IFS and converts to the device’s 7-bit (0–127) range when writing.

### Q: Can I use multiple channels simultaneously?

**A:** Yes, all 8 channels can be used simultaneously. Each channel is independent.

### Q: What's the difference between CDR and VDR modes?

**A:** CDR (Current Drive Regulation) regulates current during both hit and hold phases. VDR (Voltage Drive Regulation) applies constant voltage during hit phase and regulates current during hold phase.

### Q: How do I clear a fault?

**A:** Read the fault status register to clear latched faults. Some faults (like OCP) may require reconfiguring the channel.

### Q: What is Integrated Current Sensing (ICS)?

**A:** ICS allows reading the actual current flowing through each channel without external sense resistors. Enable it with `SetIntegratedCurrentSensing(true)`.

---

## Getting More Help

If you're still experiencing issues:

1. Check the [API Reference](api_reference.md) for method details
2. Review [Examples](examples.md) for working code
3. Search existing issues on GitHub
4. Open a new issue with:
   - Description of the problem
   - Steps to reproduce
   - Hardware setup details
   - Error messages/logs

---

**Navigation**
⬅️ [Examples](examples.md) | [Back to Index](index.md)

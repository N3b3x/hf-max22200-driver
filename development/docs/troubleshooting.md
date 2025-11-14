# Troubleshooting

This guide helps you diagnose and resolve common issues when using the MAX22200 driver.

## Common Error Messages

### Error: Initialization Failed

**Symptoms:**

- `Initialize()` returns `INITIALIZATION_ERROR`
- Driver not responding

**Causes:**

- SPI interface not properly initialized
- Hardware connections incorrect
- Power supply issues

**Solutions:**

1. **Verify SPI Interface**: Ensure SPI interface is initialized before creating driver
2. **Check Connections**: Verify all SPI connections (SCLK, SDI, SDO, CS)
3. **Verify Power**: Check power supply voltage (2.7V - 5.5V)
4. **Check CS Line**: Verify chip select is properly controlled

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

- `FaultStatus.overcurrent_protection` is true
- Channel disabled automatically

**Causes:**

- Load current exceeds limits
- Short circuit
- Incorrect current settings

**Solutions:**

1. **Check Load**: Verify load is within specifications (1A RMS max per channel)
2. **Reduce Current**: Lower hit_current and hold_current settings
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
driver.ReadFaultStatus(faults);

if (faults.hasFault()) {
    printf("Faults detected: %d\n", faults.getFaultCount());
}
```

### Read Channel Status

```cpp
max22200::ChannelStatus status;
driver.ReadChannelStatus(0, status);

printf("Channel 0: enabled=%d, fault=%d, current=%u\n",
       status.enabled, status.fault_active, status.current_reading);
```

### Use Callbacks

Set up fault callbacks to get notified immediately when faults occur:

```cpp
void fault_handler(uint8_t channel, max22200::FaultType fault_type, void *user_data) {
    printf("Fault on channel %d\n", channel);
}

driver.SetFaultCallback(fault_handler);
```

---

## FAQ

### Q: What is the maximum current per channel?

**A:** Each channel can handle up to 1A RMS. The hit_current and hold_current settings use 10-bit values (0-1023) that map to the actual current range.

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

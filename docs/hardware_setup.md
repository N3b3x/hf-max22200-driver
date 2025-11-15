---
layout: default
title: "🔌 Hardware Setup"
description: "Hardware wiring and connection guide for MAX22200 driver"
nav_order: 3
parent: "📚 Documentation"
permalink: /docs/hardware_setup/
---

# Hardware Setup

This guide covers the physical connections and hardware requirements for the MAX22200 octal solenoid and motor driver chip.

## Pin Connections

### Basic SPI Connections

```text
MCU              MAX22200
─────────────────────────
3.3V      ────── VDD
GND       ────── GND
SCK       ────── SCLK
MOSI      ────── SDI
MISO      ────── SDO
CS        ────── CS
```

### Pin Descriptions

| Pin | Name | Description | Required |
|-----|------|-------------|----------|
| VDD | Power | 3.3V power supply (2.7V - 5.5V) | Yes |
| GND | Ground | Ground reference | Yes |
| SCLK | Clock | SPI clock line | Yes |
| SDI | Data In | SPI data input (from MCU) | Yes |
| SDO | Data Out | SPI data output (to MCU) | Yes |
| CS | Chip Select | SPI chip select (active low) | Yes |
| FAULT | Fault | Fault interrupt output (optional) | No |
| RESET | Reset | Reset input (optional) | No |

### Channel Output Pins

Each of the 8 channels has two output pins:

| Channel | OUTA | OUTB |
|---------|------|------|
| 0 | OUT0A | OUT0B |
| 1 | OUT1A | OUT1B |
| 2 | OUT2A | OUT2B |
| 3 | OUT3A | OUT3B |
| 4 | OUT4A | OUT4B |
| 5 | OUT5A | OUT5B |
| 6 | OUT6A | OUT6B |
| 7 | OUT7A | OUT7B |

## Power Requirements

- **Supply Voltage**: 2.7V - 5.5V (3.3V or 5V typical)
- **Current Consumption**: Depends on load, up to 1A RMS per channel
- **Power Supply**: Clean supply with decoupling capacitors (100nF ceramic + 10µF tantalum recommended per channel)

## SPI Configuration

- **Mode**: SPI Mode 0 (CPOL=0, CPHA=0) or Mode 3 (CPOL=1, CPHA=1)
- **Speed**: Up to 10 MHz
- **Bit Order**: MSB first
- **CS Polarity**: Active low (CS)
- **Data Format**: 8-bit data words

## Load Connections

### Half-Bridge Mode

In half-bridge mode, connect the load between OUTA and GND:

```cpp
OUTA ────[Load]──── GND
OUTB ──── (not used)
```

### Full-Bridge Mode

In full-bridge mode, connect the load between OUTA and OUTB:

```cpp
OUTA ────[Load]──── OUTB
```

## Physical Layout Recommendations

- Keep SPI traces short (< 10cm recommended)
- Use ground plane for noise reduction
- Place decoupling capacitors (100nF ceramic + 10µF tantalum) close to VDD pin
- Route clock and data lines away from noise sources
- Keep high-current traces (OUTA/OUTB) away from sensitive signals
- Use appropriate trace widths for high-current paths
- Add flyback diodes for inductive loads (solenoids, motors)

## Example Wiring Diagram

```text
                    MAX22200
                    ┌─────────┐
        3.3V ───────┤ VDD     │
        GND  ───────┤ GND     │
        SCK  ───────┤ SCLK    │
        MOSI ───────┤ SDI     │
        MISO ───────┤ SDO     │
        CS   ───────┤ CS      │
                    └─────────┘
                      │
                      │ OUT0A
                      ├───[Solenoid]─── GND
                      │
                      │ OUT0B (not used in half-bridge)
```

## Protection Components

### Flyback Diodes

For inductive loads (solenoids, motors), add flyback diodes:

```cpp
OUTA ────[Flyback Diode]─── OUTB
         (cathode to OUTA)
```

### Current Limiting

The MAX22200 has built-in overcurrent protection (OCP), but you may want to add external current limiting for additional safety.

## Next Steps

- Verify connections with a multimeter
- Proceed to [Quick Start](quickstart.md) to test the connection
- Review [Platform Integration](platform_integration.md) for software setup

---

**Navigation**
⬅️ [Quick Start](quickstart.md) | [Next: Platform Integration ➡️](platform_integration.md) | [Back to Index](index.md)

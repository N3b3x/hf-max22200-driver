# SPI Protocol and Decode Analysis

This document describes how the driver maps to the MAX22200 SPI protocol (datasheet Rev 1, 3/25), how commands and data are sent/received, and how to verify behavior from logs. It also explains why **raw register comparison** is the right source of truth for write/read tests.

---

## 1. Command byte (Table 9)

Every register access starts with one **SPI(1)** transfer (CMD pin high). The byte we **send (TX)** is the Command Register:

| Bit  | Name       | Meaning |
|------|------------|--------|
| 7    | RB/W       | 0 = Read, 1 = Write |
| 6:5  | RFU        | 0 |
| 4:1  | A_BNK      | Register bank 0x00–0x0A (STATUS, CFG_CH0–7, FAULT, CFG_DPM) |
| 0    | 8bit/n32   | 0 = 32-bit transfer, 1 = 8-bit (MSB only) |

**Decoding examples from your log:**

| TX (hex) | Decode | Meaning |
|----------|--------|--------|
| 0x00 | Read, bank 0, 32-bit | Read STATUS |
| 0x80 | Write, bank 0, 32-bit | Write STATUS |
| 0x02 | Read, bank 1, 32-bit | Read CFG_CH0 |
| 0x82 | Write, bank 1, 32-bit | Write CFG_CH0 |
| 0x12 | Read, bank 9, 32-bit | Read FAULT |
| 0x14 | Read, bank 10, 32-bit | Read CFG_DPM |

**RX on that same SPI(1)** is the fault flags byte STATUS[7:0] from the device (e.g. 0x03 = no COMER; bit 0 = ACTIVE, bit 1 = UVM, etc.).

---

## 2. Data phase: write (Figure 10)

- **SDI (we send):** LSB first → first byte = DATAIN[7:0], then [15:8], [23:16], [31:24].
- Driver sends: `tx[0] = value & 0xFF`, `tx[1] = (value>>8)&0xFF`, `tx[2] = (value>>16)&0xFF`, `tx[3] = (value>>24)&0xFF`.

**Example – Write CFG_CH0 0x28500600:**

- SPI(4) TX: `00 06 50 28` → bytes [7:0]=0x00, [15:8]=0x06, [23:16]=0x50, [31:24]=0x28 → value 0x28500600. Correct.

- SDO (we receive) during write: STATUS[7:0], REGCMD, 0x00, 0x00 → e.g. `03 82 00 00` (fault byte 0x03, command echo 0x82). Correct.

---

## 3. Data phase: read (Figure 11)

- **SDO (we receive):** MSB first → first byte = DATA[31:24], then [23:16], [15:8], [7:0].
- Driver assembles: `value = (rx[0]<<24) | (rx[1]<<16) | (rx[2]<<8) | rx[3]`.

**Example – Read CFG_CH0 back:**

- SPI(4) RX: `28 50 06 00` → value = 0x28500600. Correct.

So **write LSB-first, read MSB-first** matches the datasheet and your log.

---

## 4. Decoding STATUS (0x00040003)

- RX bytes (MSB first): 00 04 00 03 → value = **0x00040003**.
- Byte 3 [31:24] = 0x00 → channels_on_mask = 0.
- Byte 2 [23:16] = 0x04 → fault masks / FREQM (e.g. one mask bit set).
- Byte 1 [15:8] = 0x00 → channel-pair modes.
- Byte 0 [7:0] = 0x03 → ACTIVE=1 (bit 0), UVM=1 (bit 1), others 0.

So: ACTIVE=1, UVM=1, channels_on=0x00. Matches log.

---

## 5. Decoding CFG_CH0 (0x28500600)

- 0x28 = byte 3: HFS=0, HOLD[6:0] = 0x28 (40 decimal) → hold current 40/127 × IFS.
- 0x50 = byte 2: TRGnSPI=0, HIT[6:0] = 0x50 (80 decimal) → hit current 80/127 × IFS.
- 0x06 = byte 1: HIT_T = 6 → hit time = 6×40/fCHOP (e.g. ~9.6 ms at 25 kHz).
- 0x00 = byte 0: VDRnCDR=0 (CDR), HSnLS=0 (low-side), etc.

So the **register value** is correct; the **decoded** hit_time_ms (e.g. 9.60 ms) differs from the **requested** 10.0 ms only because of 8-bit quantization (one LSB ≈ 40/fCHOP ms).

---

## 6. Why compare raw register value (HEX)

- **Raw register** = exactly what was written and read over SPI. If `sent_val == raw_val` (e.g. 0x28500600 == 0x28500600), the protocol and byte order are correct.
- **Decoded** values (mA, %, ms) come from the same raw value via formulas (IFS, fCHOP, etc.). Small differences (e.g. 10.0 ms vs 9.60 ms) are expected when the stored quantity is quantized.

So for **pass/fail of “did the register round-trip?”** we should compare **raw HEX** (sent vs read). Decoded comparison is still useful for debugging when raw does **not** match, but it should not fail the test when raw matches and the only difference is quantization in user units.

The channel_configuration test therefore:

1. Compares **sent_val** to **raw_val** (from `ReadRegister32`). If equal → **pass** (register round-trip OK).
2. If raw differs, it then reports decoded field mismatches and fails.

This gives a clear, unambiguous check: same HEX → SPI and decoding of the register value are correct; any remaining difference in “10.00 vs 9.60 ms” is quantization, not a protocol or decode bug.

/**
 * @file max22200_registers.hpp
 * @brief Register definitions and constants for MAX22200 IC
 *
 * This file defines the register map, bit fields, and helper functions for the
 * MAX22200 octal solenoid and motor driver IC.
 *
 * ## MAX22200 Register Architecture
 *
 * The MAX22200 uses a **two-phase SPI protocol**:
 * - **Phase 1**: Write 8-bit Command Register (CMD pin HIGH)
 * - **Phase 2**: Read/Write data register (CMD pin LOW), 8 or 32 bits
 *
 * ### Register Map
 *
 * The MAX22200 has **10x 32-bit data registers** plus one **8-bit Command Register**:
 *
 * | A_BNK | Register | Size   | Description |
 * |-------|----------|--------|-------------|
 * | 0x00  | STATUS   | 32-bit | Channel on/off, HW config, fault flags, ACTIVE |
 * | 0x01  | CFG_CH0  | 32-bit | Channel 0 configuration (all params) |
 * | 0x02  | CFG_CH1  | 32-bit | Channel 1 configuration |
 * | 0x03  | CFG_CH2  | 32-bit | Channel 2 configuration |
 * | 0x04  | CFG_CH3  | 32-bit | Channel 3 configuration |
 * | 0x05  | CFG_CH4  | 32-bit | Channel 4 configuration |
 * | 0x06  | CFG_CH5  | 32-bit | Channel 5 configuration |
 * | 0x07  | CFG_CH6  | 32-bit | Channel 6 configuration |
 * | 0x08  | CFG_CH7  | 32-bit | Channel 7 configuration |
 * | 0x09  | FAULT    | 32-bit | Per-channel fault flags (OCP/HHF/OLF/DPM) |
 * | 0x0A  | CFG_DPM  | 32-bit | DPM algorithm configuration (global) |
 *
 * ### Command Register Protocol
 *
 * The Command Register (8-bit, write-only) must be written **first** before any
 * register access. It is written with **CMD pin HIGH**. The device responds with
 * STATUS[7:0] (Fault Flag Byte) on SDO during the Command Register write.
 *
 * Command Register bit layout:
 * - Bit 7: RB/W (0=Read, 1=Write)
 * - Bits 6:5: RFU (Reserved, write 0)
 * - Bits 4:1: A_BNK (Register bank address, 0x00-0x0A)
 * - Bit 0: 8bit/n32bits (1=8-bit MSB only, 0=32-bit full)
 *
 * After writing the Command Register, set **CMD pin LOW** and perform the data
 * transfer (1 byte for 8-bit mode, 4 bytes for 32-bit mode).
 *
 * @note Per MAX22200 datasheet Rev 1 (3/25), Document 19-100531
 * @copyright Copyright (c) 2024-2025 HardFOC. All rights reserved.
 */
#pragma once
#include <cstdint>

namespace max22200 {

/**
 * @brief Number of channels on the MAX22200
 *
 * The MAX22200 is an octal (8-channel) driver.
 */
constexpr uint8_t NUM_CHANNELS_ = 8;

/**
 * @brief Maximum SPI clock frequency without daisy chaining (Hz)
 *
 * Per datasheet: "The SPI supports daisy-chain configurations and can operate
 * up to 5MHz" (with daisy chain), and "10MHz without Daisy Chain" (standalone).
 *
 * @note Use MAX_SPI_FREQ_DAISY_CHAIN_ when multiple devices are daisy-chained.
 */
constexpr uint32_t MAX_SPI_FREQ_STANDALONE_ = 10000000; // 10 MHz

/**
 * @brief Maximum SPI clock frequency with daisy chaining (Hz)
 *
 * When multiple MAX22200 devices are connected in a daisy chain, the maximum
 * SPI clock frequency is reduced to 5 MHz.
 */
constexpr uint32_t MAX_SPI_FREQ_DAISY_CHAIN_ = 5000000; // 5 MHz

// ============================================================================
// Register Bank Addresses (A_BNK field in Command Register, bits [4:1])
// ============================================================================

/**
 * @brief Register bank addresses for MAX22200
 *
 * These addresses are used in the Command Register's A_BNK field (bits 4:1)
 * to select which 32-bit register to access. Each register is addressed by
 * a 4-bit bank address (0x00-0x0A).
 *
 * @note The Command Register itself is not addressed via A_BNK — it is written
 *       directly when CMD pin is HIGH.
 */
namespace RegBank {
constexpr uint8_t STATUS   = 0x00; ///< Status register (32-bit) — channel on/off, HW config, faults, ACTIVE
constexpr uint8_t CFG_CH0  = 0x01; ///< Channel 0 configuration register (32-bit)
constexpr uint8_t CFG_CH1  = 0x02; ///< Channel 1 configuration register (32-bit)
constexpr uint8_t CFG_CH2  = 0x03; ///< Channel 2 configuration register (32-bit)
constexpr uint8_t CFG_CH3  = 0x04; ///< Channel 3 configuration register (32-bit)
constexpr uint8_t CFG_CH4  = 0x05; ///< Channel 4 configuration register (32-bit)
constexpr uint8_t CFG_CH5  = 0x06; ///< Channel 5 configuration register (32-bit)
constexpr uint8_t CFG_CH6  = 0x07; ///< Channel 6 configuration register (32-bit)
constexpr uint8_t CFG_CH7  = 0x08; ///< Channel 7 configuration register (32-bit)
constexpr uint8_t FAULT    = 0x09; ///< Fault register (32-bit, read-only) — per-channel fault flags
constexpr uint8_t CFG_DPM  = 0x0A; ///< DPM configuration register (32-bit) — global DPM algorithm settings
} // namespace RegBank

// ============================================================================
// Command Register (8-bit, Write Only)
// ============================================================================

/**
 * @brief Command Register bit field definitions and helper functions
 *
 * The Command Register is 8-bit, write-only, and **must be written with CMD pin HIGH**
 * before any register access. It determines the type and format of the follow-on SPI transfer.
 *
 * ### Bit Layout
 *
 * ```
 * Bit:  7     6     5     4     3     2     1     0
 *      RB/W  RFU   RFU   A_BNK[3] A_BNK[2] A_BNK[1] A_BNK[0]  8bit/n32
 * ```
 *
 * ### Protocol Sequence
 *
 * 1. **Set CMD pin HIGH**
 * 2. **Write Command Register** (1 byte transfer)
 *    - Device responds with STATUS[7:0] on SDO (Fault Flag Byte)
 *    - Check for communication error: STATUS[7:0] = 0x04 (COMER flag)
 * 3. **Set CMD pin LOW**
 * 4. **Perform data transfer** (1 byte for 8-bit mode, 4 bytes for 32-bit mode)
 *
 * @note Once the Command Register is written, all subsequent SPI transfers remain
 *       of the same type (8-bit or 32-bit) until the Command Register is rewritten.
 *
 * @note The CMD pin must be held HIGH during the rising edge of CSB (chip select).
 */
namespace CommandReg {
constexpr uint8_t RBW_POS       = 7;    ///< Read/Write bit position
constexpr uint8_t RBW_READ      = 0x00; ///< Read operation (bit 7 = 0)
constexpr uint8_t RBW_WRITE     = 0x80; ///< Write operation (bit 7 = 1)

constexpr uint8_t A_BNK_POS     = 1;    ///< Bank address bit position (bits 4:1)
constexpr uint8_t A_BNK_MASK    = 0x1E; ///< Bank address mask (bits 4:1)

constexpr uint8_t MODE_8BIT     = 0x01; ///< 8-bit MSB only access (bit 0 = 1)
constexpr uint8_t MODE_32BIT    = 0x00; ///< 32-bit full access (bit 0 = 0)

/**
 * @brief Build a Command Register value
 *
 * Constructs the 8-bit Command Register byte from the register bank address,
 * read/write mode, and transfer size.
 *
 * @param bank    Register bank address (0x00-0x0A, see RegBank namespace)
 * @param write   true for write operation, false for read
 * @param mode8   true for 8-bit MSB only, false for 32-bit full register
 * @return 8-bit command register value ready for SPI transfer
 *
 * @example
 * @code
 * // Write 32-bit STATUS register
 * uint8_t cmd = CommandReg::build(RegBank::STATUS, true, false);
 * // Result: 0x80 | (0x00 << 1) | 0x00 = 0x80
 *
 * // Read 8-bit MSB of CFG_CH0 (fast HOLD current update)
 * uint8_t cmd = CommandReg::build(RegBank::CFG_CH0, false, true);
 * // Result: 0x00 | (0x01 << 1) | 0x01 = 0x03
 * @endcode
 */
constexpr uint8_t build(uint8_t bank, bool write, bool mode8 = false) {
  return static_cast<uint8_t>(
      (write ? RBW_WRITE : RBW_READ) |
      ((bank & 0x0F) << A_BNK_POS) |
      (mode8 ? MODE_8BIT : MODE_32BIT));
}
} // namespace CommandReg

// ============================================================================
// STATUS Register (0x00) — 32-bit
// ============================================================================

/**
 * @brief STATUS register bit field definitions
 *
 * The STATUS register is 32-bit and contains channel activation, hardware
 * configuration, fault masks, fault flags, and the global ACTIVE bit.
 *
 * ### Register Layout
 *
 * | Byte | Bits    | Field              | Description |
 * |------|---------|--------------------|-------------|
 * | 3    | 31:24   | ONCH[7:0]          | Channel on/off bits (1=on, 0=off) |
 * | 2    | 23:16   | M_OVT, M_OCP, etc. | Fault mask bits + FREQM |
 * | 1    | 15:8    | CM76, CM54, etc.   | Channel-pair mode configuration |
 * | 0    | 7:0     | OVT, OCP, etc.    | Fault flags (read-only) + ACTIVE |
 *
 * ### Key Fields
 *
 * - **ONCH[7:0]**: Individual channel enable/disable. Bit N controls channel N.
 *   Can be updated with fast 8-bit write to STATUS[31:24].
 *
 * - **ACTIVE**: Global enable bit. Must be set to 1 for normal operation.
 *   When 0, device enters low-power mode and all channels are three-stated.
 *   Default is 0 at power-up.
 *
 * - **UVM**: Undervoltage flag. Set high at power-up and must be cleared by
 *   reading the STATUS register. Reading STATUS clears UVM and deasserts nFAULT.
 *
 * - **CMxy**: Channel-pair mode bits (CM76, CM54, CM32, CM10). Configure pairs
 *   of contiguous channels for independent, parallel, or H-bridge operation.
 *   Can only be written when both channels in the pair are OFF (ONCHx=0, ONCHy=0).
 *
 * @note Fault flags (bits 7:1) are read-only. They are cleared by reading the
 *       STATUS register (UVM) or FAULT register (OCP, HHF, OLF, DPM).
 */
namespace StatusReg {
// Byte 3 (bits 31:24) — Channel activation
constexpr uint32_t ONCH_SHIFT    = 24;   ///< ONCH bit shift (bits 31:24)
constexpr uint32_t ONCH_MASK     = 0xFF000000u; ///< ONCH bitmask

// Byte 2 (bits 23:16) — Fault masks + FREQM
constexpr uint32_t M_OVT_BIT     = (1u << 23); ///< OVT fault mask (1=masked, 0=signaled)
constexpr uint32_t M_OCP_BIT     = (1u << 22); ///< OCP fault mask
constexpr uint32_t M_OLF_BIT     = (1u << 21); ///< OLF fault mask
constexpr uint32_t M_HHF_BIT     = (1u << 20); ///< HHF fault mask
constexpr uint32_t M_DPM_BIT     = (1u << 19); ///< DPM fault mask
constexpr uint32_t M_COMF_BIT    = (1u << 18); ///< Communication fault mask (reset value = 1, masked by default)
constexpr uint32_t M_UVM_BIT     = (1u << 17); ///< UVM fault mask
constexpr uint32_t FREQM_BIT     = (1u << 16); ///< Master frequency (0=100kHz, 1=80kHz)

// Byte 1 (bits 15:8) — Channel-pair mode configuration
constexpr uint32_t CM76_SHIFT    = 14;   ///< CM76 bit shift (bits 15:14)
constexpr uint32_t CM76_MASK     = (0x03u << 14); ///< CM76 bitmask
constexpr uint32_t CM54_SHIFT    = 12;   ///< CM54 bit shift (bits 13:12)
constexpr uint32_t CM54_MASK     = (0x03u << 12); ///< CM54 bitmask
constexpr uint32_t CM32_SHIFT    = 10;   ///< CM32 bit shift (bits 11:10)
constexpr uint32_t CM32_MASK     = (0x03u << 10); ///< CM32 bitmask
constexpr uint32_t CM10_SHIFT    = 8;    ///< CM10 bit shift (bits 9:8)
constexpr uint32_t CM10_MASK     = (0x03u << 8);  ///< CM10 bitmask

// Byte 0 (bits 7:0) — Fault flags (read-only except ACTIVE)
constexpr uint32_t OVT_BIT       = (1u << 7);  ///< Overtemperature fault flag (read-only)
constexpr uint32_t OCP_BIT       = (1u << 6);  ///< Overcurrent fault flag (read-only)
constexpr uint32_t OLF_BIT       = (1u << 5);  ///< Open-load fault flag (read-only)
constexpr uint32_t HHF_BIT       = (1u << 4);  ///< HIT current not reached flag (read-only)
constexpr uint32_t DPM_BIT       = (1u << 3);  ///< Plunger movement detection flag (read-only)
constexpr uint32_t COMER_BIT     = (1u << 2);  ///< Communication error flag (read-only, STATUS[7:0]=0x04)
/// Fault byte value returned on SDO when COMER is set (per datasheet Figure 6)
constexpr uint8_t FAULT_BYTE_COMER = 0x04u;
constexpr uint32_t UVM_BIT       = (1u << 1);  ///< Undervoltage flag (read-only, set at POR, cleared by read)
constexpr uint32_t ACTIVE_BIT    = (1u << 0);  ///< Global enable bit (write: 0=low-power, 1=normal operation)

/// Fault flags byte mask (bits 7:1, read-only)
constexpr uint32_t FAULT_FLAGS_MASK = 0xFEu;

/// Channel-pair mode: independent operation
constexpr uint8_t CM_INDEPENDENT = 0x00;
/// Channel-pair mode: parallel mode (double current)
constexpr uint8_t CM_PARALLEL    = 0x01;
/// Channel-pair mode: H-bridge mode (full-bridge)
constexpr uint8_t CM_HBRIDGE     = 0x02;
/// Channel-pair mode: reserved (do not use)
constexpr uint8_t CM_RESERVED    = 0x03;
} // namespace StatusReg

// ============================================================================
// Channel Configuration Register (CFG_CHx, 0x01-0x08) — 32-bit
// ============================================================================

/**
 * @brief Channel configuration register bit field definitions
 *
 * Each channel has one 32-bit configuration register (CFG_CHx) that contains
 * all drive parameters for that channel. The register layout is:
 *
 * ### Register Layout
 *
 * | Byte | Bits    | Field         | Description |
 * |------|---------|---------------|-------------|
 * | 3    | 31      | HFS           | Half Full-Scale (0=1x, 1=0.5x) |
 * | 3    | 30:24   | HOLD[6:0]     | HOLD current (7-bit, 0-127) |
 * | 2    | 23      | TRGnSPI       | Trigger source (0=SPI, 1=TRIG pin) |
 * | 2    | 22:16   | HIT[6:0]      | HIT current (7-bit, 0-127) |
 * | 1    | 15:8    | HIT_T[7:0]    | HIT time (8-bit, 0-255) |
 * | 0    | 7       | VDRnCDR      | Drive mode (0=CDR, 1=VDR) |
 * | 0    | 6       | HSnLS        | High-side/Low-side (0=LS, 1=HS) |
 * | 0    | 5:4     | FREQ_CFG     | Chopping frequency divider |
 * | 0    | 3       | SRC          | Slew rate control enable |
 * | 0    | 2       | OL_EN        | Open load detect enable |
 * | 0    | 1       | DPM_EN       | DPM detection enable |
 * | 0    | 0       | HHF_EN       | HIT current check enable |
 *
 * ### Current Programming (CDR Mode)
 *
 * In CDR mode, HIT and HOLD currents are programmed as fractions of the
 * full-scale current (IFS):
 *
 * - **IHIT = HIT[6:0] / 127 × IFS** (for 1-126)
 * - **IHOLD = HOLD[6:0] / 127 × IFS** (for 1-126)
 * - **HIT[6:0] = 0**: HS switch ON, LS switch OFF
 * - **HIT[6:0] = 127**: HS switch OFF, LS switch ON
 *
 * The full-scale current IFS is set by an external resistor RREF on the IREF pin:
 * - **IFS = KFS × 1V / RREF**
 * - **KFS = 15k** when HFS = 0 (full-scale, IFS up to 1A)
 * - **KFS = 7.5k** when HFS = 1 (half-scale, IFS up to 0.5A)
 * - **RREF range**: 15kΩ to 100kΩ recommended
 *
 * ### Current Programming (VDR Mode)
 *
 * In VDR mode, HIT and HOLD values represent PWM duty cycle percentages:
 * - **Duty cycle = HIT[6:0]%** (for 1-126, clamped to δMIN-δMAX per Table 2)
 * - **Duty cycle = 0%** when HIT[6:0] = 0
 * - **Duty cycle = 100%** when HIT[6:0] ≥ 100
 *
 * ### HIT Time Programming
 *
 * The HIT excitation time is calculated as:
 * - **HIT_T[7:0] = 0**: No HIT time (tHIT = 0)
 * - **HIT_T[7:0] = 1-254**: tHIT = HIT_T[7:0] × 40 / fCHOP
 * - **HIT_T[7:0] = 255**: Continuous IHIT (tHIT = ∞)
 *
 * Where fCHOP is the chopping frequency (depends on FREQM and FREQ_CFG).
 *
 * ### Chopping Frequency
 *
 * The chopping frequency fCHOP depends on FREQM (STATUS[16]) and FREQ_CFG[1:0]:
 *
 * | FREQM | FREQ_CFG | fCHOP (kHz) |
 * |-------|----------|-------------|
 * | 0     | 00       | 25          |
 * | 0     | 01       | 33.33       |
 * | 0     | 10       | 50          |
 * | 0     | 11       | 100         |
 * | 1     | 00       | 20          |
 * | 1     | 01       | 26.66       |
 * | 1     | 10       | 40          |
 * | 1     | 11       | 80          |
 *
 * ### Restrictions
 *
 * - **VDRnCDR and HSnLS bits** can only be modified when:
 *   - All channels are OFF (ONCHx = 0 for all channels)
 *   - Both TRIGA and TRIGB inputs are logic-low
 *
 * - **CDR mode** is only supported in **low-side** operation (HSnLS = 0)
 *
 * - **High-side mode** (HSnLS = 1) only supports **VDR mode**
 *
 * - **HFS bit** is only available for **low-side** applications
 *
 * @note Per MAX22200 datasheet Table 11 and sections on Current Drive Regulation
 *       and Voltage Drive Regulation.
 */
namespace CfgChReg {
// Bit 31: HFS (Half Full-Scale)
constexpr uint32_t HFS_BIT       = (1u << 31); ///< HFS bit (0=1x full-scale, 1=0.5x half-scale)

// Bits 30:24: HOLD current (7-bit)
constexpr uint32_t HOLD_SHIFT    = 24;   ///< HOLD current bit shift
constexpr uint32_t HOLD_MASK     = (0x7Fu << 24); ///< HOLD current mask (7-bit, 0-127)

// Bit 23: TRGnSPI (Trigger source)
constexpr uint32_t TRGNSPI_BIT   = (1u << 23); ///< TRGnSPI bit (0=SPI ONCH, 1=TRIG pin)

// Bits 22:16: HIT current (7-bit)
constexpr uint32_t HIT_SHIFT     = 16;   ///< HIT current bit shift
constexpr uint32_t HIT_MASK      = (0x7Fu << 16); ///< HIT current mask (7-bit, 0-127)

// Bits 15:8: HIT time (8-bit)
constexpr uint32_t HITT_SHIFT    = 8;    ///< HIT time bit shift
constexpr uint32_t HITT_MASK     = (0xFFu << 8); ///< HIT time mask (8-bit, 0-255)

// Bit 7: VDRnCDR (Drive mode)
constexpr uint32_t VDRNCDR_BIT   = (1u << 7);   ///< VDRnCDR bit (0=CDR, 1=VDR)

// Bit 6: HSnLS (High-side/Low-side)
constexpr uint32_t HSNLS_BIT     = (1u << 6);   ///< HSnLS bit (0=low-side, 1=high-side)

// Bits 5:4: FREQ_CFG (Chopping frequency)
constexpr uint32_t FREQ_CFG_SHIFT = 4;   ///< FREQ_CFG bit shift
constexpr uint32_t FREQ_CFG_MASK  = (0x03u << 4); ///< FREQ_CFG mask (2-bit)

// Bit 3: SRC (Slew rate control)
constexpr uint32_t SRC_BIT       = (1u << 3);   ///< SRC bit (0=fast, 1=slew-rate controlled)

// Bit 2: OL_EN (Open load detect enable)
constexpr uint32_t OL_EN_BIT     = (1u << 2);   ///< OL_EN bit (0=disabled, 1=enabled)

// Bit 1: DPM_EN (DPM detection enable)
constexpr uint32_t DPM_EN_BIT    = (1u << 1);   ///< DPM_EN bit (0=disabled, 1=enabled)

// Bit 0: HHF_EN (HIT current check enable)
constexpr uint32_t HHF_EN_BIT    = (1u << 0);   ///< HHF_EN bit (0=disabled, 1=enabled)

/// Maximum HOLD current register value (7-bit)
constexpr uint8_t MAX_HOLD       = 127;
/// Maximum HIT current register value (7-bit)
constexpr uint8_t MAX_HIT        = 127;
/// Maximum HIT time register value (8-bit)
constexpr uint8_t MAX_HIT_TIME   = 255;
/// HIT time value for continuous IHIT (tHIT = ∞)
constexpr uint8_t CONTINUOUS_HIT = 255;
} // namespace CfgChReg

// ============================================================================
// FAULT Register (0x09) — 32-bit, Read Only
// ============================================================================

/**
 * @brief FAULT register bit field definitions
 *
 * The FAULT register is 32-bit, read-only, and contains per-channel fault flags.
 * Each fault type has an 8-bit field where bit N corresponds to channel N.
 *
 * ### Register Layout
 *
 * | Byte | Bits    | Field      | Description |
 * |------|---------|------------|-------------|
 * | 3    | 31:24   | OCP[7:0]   | Per-channel overcurrent flags |
 * | 2    | 23:16   | HHF[7:0]   | Per-channel HIT not reached flags |
 * | 1    | 15:8    | OLF[7:0]   | Per-channel open-load flags |
 * | 0    | 7:0     | DPM[7:0]   | Per-channel plunger movement flags |
 *
 * ### Clearing Fault Flags
 *
 * **MAX22200**: Reading the FAULT register clears all fault flags simultaneously.
 *
 * **MAX22200A** (recommended): Selective clear on read. To clear a specific fault
 * bit, read the FAULT register while keeping the corresponding SDI bit HIGH for
 * the channel(s) to clear. Only the bits corresponding to HIGH SDI bits are cleared.
 *
 * @note Channel 0 corresponds to the LSB (bit 0) of each fault byte.
 * @note Reading the FAULT register also deasserts the nFAULT pin (if no other
 *       faults are active).
 */
namespace FaultReg {
constexpr uint32_t OCP_SHIFT = 24;   ///< OCP bit shift (bits 31:24)
constexpr uint32_t OCP_MASK  = 0xFF000000u; ///< OCP bitmask
constexpr uint32_t HHF_SHIFT = 16;   ///< HHF bit shift (bits 23:16)
constexpr uint32_t HHF_MASK  = 0x00FF0000u; ///< HHF bitmask
constexpr uint32_t OLF_SHIFT = 8;    ///< OLF bit shift (bits 15:8)
constexpr uint32_t OLF_MASK  = 0x0000FF00u; ///< OLF bitmask
constexpr uint32_t DPM_SHIFT = 0;    ///< DPM bit shift (bits 7:0)
constexpr uint32_t DPM_MASK  = 0x000000FFu; ///< DPM bitmask
} // namespace FaultReg

// ============================================================================
// CFG_DPM Register (0x0A) — 32-bit, DPM Algorithm Configuration
// ============================================================================

/**
 * @brief CFG_DPM register bit field definitions
 *
 * The CFG_DPM register configures the Detection of Plunger Movement (DPM)
 * algorithm parameters. DPM settings are **global** and apply to all channels.
 *
 * ### Register Layout
 *
 * | Bits    | Field          | Description |
 * |---------|----------------|-------------|
 * | 31:21   | RSVD           | Reserved (write 0) |
 * | 20:16   | RSVD           | Reserved (write 0) |
 * | 15      | RSVD           | Reserved (write 0) |
 * | 14:8    | DPM_ISTART[6:0]| Starting current for DPM monitoring |
 * | 7:4     | DPM_TDEB[3:0]  | DPM debounce time |
 * | 3:0     | DPM_IPTH[3:0]  | DPM current dip threshold |
 *
 * ### DPM Algorithm Parameters
 *
 * - **ISTART**: Starting current for DPM monitoring
 *   - ISTART = DPM_ISTART[6:0] × (IFS / 127)
 *   - Set ISTART just below the minimum current where the plunger movement
 *     dip is expected
 *
 * - **TDEB**: Debounce time (minimum dip duration to be recognized)
 *   - TDEB = DPM_TDEB[3:0] / fCHOP
 *   - Lower values = more sensitive (may false-trigger on noise)
 *   - Higher values = less sensitive (may miss valid dips)
 *
 * - **IPTH**: Current dip threshold (minimum dip amplitude)
 *   - IPTH = DPM_IPTH[3:0] × (IFS / 127)
 *   - Lower values = more sensitive
 *   - Higher values = less sensitive
 *
 * ### DPM Algorithm Behavior
 *
 * The DPM algorithm monitors current above ISTART until either:
 * - The programmed IHIT level is reached, OR
 * - The HIT time (tHIT) ends
 *
 * A valid plunger movement is detected if:
 * - A current dip with amplitude > IPTH occurs, AND
 * - The dip duration > TDEB
 *
 * If no valid dip is detected, a DPM fault is signaled (if DPM_EN is set for
 * that channel and M_DPM is not masked).
 *
 * @note DPM accuracy becomes less reliable if the dip is not pronounced enough.
 * @note The current slope above ISTART must be slower than 700mA/ms for
 *       reliable detection (to allow the internal ADC to track).
 *
 * @note Per datasheet section "Detection of Plunger Movement (DPM)" and
 *       "Detection of Plunger Movement Register Description (CFG_DPM)".
 */
namespace CfgDpmReg {
constexpr uint32_t DPM_ISTART_SHIFT = 8;   ///< DPM_ISTART bit shift (bits 14:8)
constexpr uint32_t DPM_ISTART_MASK  = (0x7Fu << 8); ///< DPM_ISTART mask (7-bit)
constexpr uint32_t DPM_TDEB_SHIFT   = 4;    ///< DPM_TDEB bit shift (bits 7:4)
constexpr uint32_t DPM_TDEB_MASK    = (0x0Fu << 4); ///< DPM_TDEB mask (4-bit)
constexpr uint32_t DPM_IPTH_SHIFT   = 0;    ///< DPM_IPTH bit shift (bits 3:0)
constexpr uint32_t DPM_IPTH_MASK    = 0x0Fu; ///< DPM_IPTH mask (4-bit)
} // namespace CfgDpmReg

// ============================================================================
// Helper functions
// ============================================================================

/**
 * @brief Get channel configuration register bank address
 *
 * Returns the A_BNK address for a channel's configuration register.
 *
 * @param channel Channel number (0-7)
 * @return Register bank address (0x01-0x08) for the specified channel
 *
 * @example
 * @code
 * uint8_t bank = getChannelCfgBank(0);  // Returns 0x01 (CFG_CH0)
 * uint8_t bank = getChannelCfgBank(3);  // Returns 0x04 (CFG_CH3)
 * @endcode
 */
constexpr uint8_t getChannelCfgBank(uint8_t channel) {
  return static_cast<uint8_t>(RegBank::CFG_CH0 + channel);
}

// Forward declarations
enum class DriveMode : uint8_t;
enum class ChannelMode : uint8_t;

} // namespace max22200

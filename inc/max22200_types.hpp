/**
 * @file max22200_types.hpp
 * @brief Type definitions and structures for MAX22200 driver
 *
 * This file defines all data structures, enumerations, and type aliases used
 * by the MAX22200 driver. All types match the MAX22200 datasheet register
 * layout (Rev 1, 3/25, Document 19-100531).
 *
 * ## Current Programming Details
 *
 * ### CDR Mode (Current Drive Regulation)
 *
 * In CDR mode, currents are programmed as fractions of the full-scale current (IFS):
 *
 * - **IHIT = HIT[6:0] / 127 × IFS** (for 1-126)
 * - **IHOLD = HOLD[6:0] / 127 × IFS** (for 1-126)
 * - **HIT[6:0] = 0**: High-side switch ON, low-side switch OFF
 * - **HIT[6:0] = 127**: High-side switch OFF, low-side switch ON
 *
 * The full-scale current IFS is set by an external resistor RREF on the IREF pin:
 * - **IFS = KFS × 1V / RREF**
 * - **KFS = 15k** when HFS = 0 (full-scale, IFS up to 1A, RON = 0.2Ω)
 * - **KFS = 7.5k** when HFS = 1 (half-scale, IFS up to 0.5A, RON = 0.4Ω)
 * - **RREF range**: 15kΩ to 100kΩ recommended
 *
 * ### VDR Mode (Voltage Drive Regulation)
 *
 * In VDR mode, HIT and HOLD values represent PWM duty cycle percentages:
 * - **Duty cycle = HIT[6:0]%** (for 1-126, clamped to δMIN-δMAX per datasheet Table 2)
 * - **Duty cycle = 0%** when HIT[6:0] = 0
 * - **Duty cycle = 100%** when HIT[6:0] ≥ 100
 *
 * Duty cycle limits (δMIN, δMAX) depend on FREQM, FREQ_CFG, and SRC settings.
 *
 * ## HIT Time Calculation
 *
 * The HIT excitation time is calculated as:
 * - **HIT_T[7:0] = 0**: No HIT time (tHIT = 0)
 * - **HIT_T[7:0] = 1-254**: tHIT = HIT_T[7:0] × 40 / fCHOP
 * - **HIT_T[7:0] = 255**: Continuous IHIT (tHIT = ∞)
 *
 * Where fCHOP is the chopping frequency (depends on FREQM and FREQ_CFG).
 *
 * ## Chopping Frequency
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
 * @note CDR mode is only supported in low-side operation (HSnLS = 0).
 * @note High-side mode (HSnLS = 1) only supports VDR mode.
 * @note VDRnCDR and HSnLS bits can only be modified when all channels are OFF
 *       and both TRIGA and TRIGB inputs are logic-low.
 *
 * @copyright Copyright (c) 2024-2025 HardFOC. All rights reserved.
 */
#pragma once
#include "max22200_registers.hpp"
#include <array>
#include <cstdint>

namespace max22200 {

// ============================================================================
// Enumerations
// ============================================================================

/**
 * @brief Drive mode enumeration (VDRnCDR bit in CFG_CHx[7])
 *
 * Determines how the channel regulates output current/voltage.
 *
 * ### CDR (Current Drive Regulation)
 * - Uses integrated lossless current sensing (ICS)
 * - Regulates peak current cycle-by-cycle
 * - More efficient than VDR (no design margin needed)
 * - **Only supported in low-side mode** (HSnLS = 0)
 * - Current accuracy: ±6% for 71-100% of IFS, ±10% for 20-70%, ±15% for 10-20%, ±25% for 5-10%
 *
 * ### VDR (Voltage Drive Regulation)
 * - Outputs PWM voltage with programmable duty cycle
 * - Duty cycle resolution: 1% steps
 * - Supported in both low-side and high-side modes
 * - Output current proportional to duty cycle for given supply voltage and load resistance
 *
 * @note VDRnCDR bit can only be changed when all channels are OFF and both
 *       TRIGA and TRIGB inputs are logic-low.
 */
enum class DriveMode : uint8_t {
  CDR = 0, ///< Current Drive Regulation (VDRnCDR = 0) — low-side only
  VDR = 1  ///< Voltage Drive Regulation (VDRnCDR = 1) — low-side or high-side
};

/**
 * @brief High-side / Low-side selection (HSnLS bit in CFG_CHx[6])
 *
 * Determines whether the channel drives the load connected to the positive rail
 * (low-side) or to ground (high-side).
 *
 * ### Low-Side Mode (HSnLS = 0)
 * - Load connected between OUTx and VM (positive rail)
 * - Supports both CDR and VDR modes
 * - Supports HFS (half full-scale) bit for improved accuracy at low currents
 * - Supports SRC (slew rate control) and DPM (plunger movement detection)
 *
 * ### High-Side Mode (HSnLS = 1)
 * - Load connected between OUTx and GND
 * - **Only supports VDR mode** (CDR not available)
 * - HFS, SRC, and DPM functions are **not supported** in high-side mode
 *
 * @note HSnLS bit can only be changed when all channels are OFF and both
 *       TRIGA and TRIGB inputs are logic-low.
 */
enum class SideMode : uint8_t {
  LOW_SIDE  = 0, ///< Low-side driver (HSnLS = 0) — supports CDR and VDR
  HIGH_SIDE = 1  ///< High-side driver (HSnLS = 1) — VDR only, no CDR/HFS/SRC/DPM
};

/**
 * @brief Channel-pair mode (CMxy bits in STATUS[15:8])
 *
 * Configures pairs of contiguous channels (0-1, 2-3, 4-5, 6-7) for different
 * operating modes. These bits can only be written when both channels in the
 * pair are OFF (ONCHx = 0, ONCHy = 0).
 *
 * ### INDEPENDENT (CMxy = 00)
 * - Each channel operates independently
 * - Standard half-bridge operation
 * - Each channel uses its own configuration register (CFG_CHx)
 *
 * ### PARALLEL (CMxy = 01)
 * - Two channels connected in parallel to double current capability
 * - Output pins must be connected together externally
 * - Configuration taken from **lower channel number** (e.g., CFG_CH0 for pair 0-1)
 * - Higher channel's configuration register is ignored
 * - Activation remains independent (ONCHx and ONCHy control individually)
 * - TRGnSPI bits determine trigger source (see datasheet Table 6)
 *
 * ### HBRIDGE (CMxy = 10)
 * - Two channels form a full-bridge (H-bridge) for bidirectional drive
 * - Used for latched (bi-stable) solenoid valves or brushed DC motors
 * - Four states: Hi-Z, Forward, Reverse, Brake (see datasheet Table 7)
 * - Forward mode uses CFG_CHy settings, Reverse mode uses CFG_CHx settings
 * - HSnLS bit is ignored in full-bridge mode
 * - TRGnSPI bits determine trigger source (see datasheet Table 8)
 *
 * @note Channel pairs: CM10 (ch0-1), CM32 (ch2-3), CM54 (ch4-5), CM76 (ch6-7)
 * @note CMxy = 11 is reserved — do not use
 */
enum class ChannelMode : uint8_t {
  INDEPENDENT = 0, ///< Channels independently controlled (CMxy = 00)
  PARALLEL    = 1, ///< Channels in parallel mode (CMxy = 01) — double current
  HBRIDGE     = 2  ///< Channels in H-bridge mode (CMxy = 10) — bidirectional drive
};

/**
 * @brief Chopping frequency setting (FREQ_CFG bits in CFG_CHx[5:4])
 *
 * Selects the chopping frequency divider relative to the master frequency (FREQM).
 * The actual chopping frequency fCHOP depends on both FREQM (STATUS[16]) and FREQ_CFG:
 *
 * | FREQM | FREQ_CFG | fCHOP (kHz) | Period (μs) |
 * |-------|----------|-------------|-------------|
 * | 0     | 00       | 25          | 40          |
 * | 0     | 01       | 33.33       | 30          |
 * | 0     | 10       | 50          | 20          |
 * | 0     | 11       | 100         | 10          |
 * | 1     | 00       | 20          | 50          |
 * | 1     | 01       | 26.66       | 37.5        |
 * | 1     | 10       | 40          | 25          |
 * | 1     | 11       | 80          | 12.5        |
 *
 * ### Effects of Chopping Frequency
 *
 * - **Higher frequency**: Lower current ripple, better average current control,
 *   but higher switching losses and EMI
 * - **Lower frequency**: Higher current ripple, but lower switching losses
 *
 * ### Duty Cycle Limits
 *
 * Minimum and maximum duty cycles (δMIN, δMAX) depend on FREQM, FREQ_CFG, and SRC:
 *
 * - **SRC = 0 (Fast Mode)**:
 *   - FREQ_CFG = 11: δMIN = 8%, δMAX = 92%
 *   - Other FREQ_CFG: δMIN = 4%, δMAX = 96%
 * - **SRC = 1 (Slew-Rate Controlled)**:
 *   - FREQ_CFG = 01 or 00: δMIN = 7%, δMAX = 93%
 *   - FREQ_CFG = 10 (FREQM=1 only): δMIN = 7%, δMAX = 93%
 *
 * ### SRC + fCHOP restriction (datasheet)
 *
 * **SRC mode is only available for switching frequencies less than 50 kHz.**
 * When SRC = 1, use only FREQ_CFG that yields fCHOP < 50 kHz:
 * - **FREQM = 0**: use FMAIN_DIV4 (25 kHz) or FMAIN_DIV3 (33.33 kHz); do not use
 *   FMAIN_DIV2 (50 kHz) or FMAIN (100 kHz).
 * - **FREQM = 1**: use FMAIN_DIV4 (20 kHz), FMAIN_DIV3 (26.66 kHz), or
 *   FMAIN_DIV2 (40 kHz); do not use FMAIN (80 kHz).
 *
 * @note FREQ_CFG can be changed "on-the-fly" while channel is operating.
 * @note FREQM is a global setting in STATUS register (affects all channels).
 */
enum class ChopFreq : uint8_t {
  FMAIN_DIV4 = 0, ///< FreqMain / 4 (fCHOP = 25kHz if FREQM=0, 20kHz if FREQM=1). Valid with SRC=1.
  FMAIN_DIV3 = 1, ///< FreqMain / 3 (fCHOP = 33.33kHz if FREQM=0, 26.66kHz if FREQM=1). Valid with SRC=1.
  FMAIN_DIV2 = 2, ///< FreqMain / 2 (fCHOP = 50kHz if FREQM=0, 40kHz if FREQM=1). With SRC=1 use only if FREQM=1 (40kHz).
  FMAIN      = 3  ///< FreqMain (fCHOP = 100kHz if FREQM=0, 80kHz if FREQM=1). Not valid with SRC=1.
};

/**
 * @brief Get chopping frequency in kHz for conversion (FREQM + FREQ_CFG)
 * @param master_clock_80khz false = 100 kHz base, true = 80 kHz base
 * @param cf    Channel ChopFreq (FREQ_CFG)
 * @return fCHOP in kHz (e.g. 25, 33, 50, 100 for 100 kHz base)
 */
inline uint32_t getChopFreqKhz(bool master_clock_80khz, ChopFreq cf) {
  switch (cf) {
    case ChopFreq::FMAIN_DIV4: return master_clock_80khz ? 20u : 25u;
    case ChopFreq::FMAIN_DIV3: return master_clock_80khz ? 26u : 33u;
    case ChopFreq::FMAIN_DIV2: return master_clock_80khz ? 40u : 50u;
    case ChopFreq::FMAIN:      return master_clock_80khz ? 80u : 100u;
    default:                   return 25u;
  }
}

/**
 * @brief Convert current in mA to 7-bit raw value (0–127) for CDR
 * @param full_scale_current_ma Full-scale current in mA (must be > 0)
 * @param ma     Desired current in mA
 * @return Raw 7-bit value; clamped to 127 if ma >= full_scale_current_ma
 */
inline uint8_t currentMaToRaw(uint32_t full_scale_current_ma, uint32_t ma) {
  if (full_scale_current_ma == 0) return 0;
  if (ma >= full_scale_current_ma) return 127;
  uint32_t raw = (ma * 127u + full_scale_current_ma / 2u) / full_scale_current_ma;
  return raw > 127u ? 127u : static_cast<uint8_t>(raw);
}

/**
 * @brief Convert HIT time in ms to 8-bit raw value (0–255)
 * @param ms     HIT time in ms (0 = no HIT; < 0 or very large = continuous, 255)
 * @param master_clock_80khz false = 100 kHz base, true = 80 kHz base
 * @param cf     Chopping frequency (FREQ_CFG)
 * @return Raw 8-bit HIT_T; 255 = continuous
 */
inline uint8_t hitTimeMsToRaw(float ms, bool master_clock_80khz, ChopFreq cf) {
  uint32_t fchop_khz = getChopFreqKhz(master_clock_80khz, cf);
  if (ms < 0.0f || ms >= 1000000.0f) return 255;
  if (ms == 0.0f) return 0;
  float fchop_hz = static_cast<float>(fchop_khz) * 1000.0f;
  uint32_t raw = static_cast<uint32_t>((ms / 1000.0f) * fchop_hz / 40.0f + 0.5f);
  if (raw > 254u) return 255;
  if (raw == 0u) return 1;
  return static_cast<uint8_t>(raw);
}

/**
 * @brief Fault type enumeration
 */
enum class FaultType : uint8_t {
  OCP   = 0, ///< Overcurrent protection (OCP)
  HHF   = 1, ///< HIT current not reached (HHF)
  OLF   = 2, ///< Open-load detection (OLF)
  DPM   = 3, ///< Detection of plunger movement (DPM)
  OVT   = 4, ///< Overtemperature (OVT, from STATUS)
  UVM   = 5, ///< Undervoltage lockout (UVM, from STATUS)
  COMER = 6  ///< Communication error (COMER, from STATUS)
};

/**
 * @brief Human-readable fault type name (for logging and tests)
 * @param ft FaultType value
 * @return Non-null C string (e.g. "Overcurrent", "HIT not reached")
 */
inline const char *FaultTypeToStr(FaultType ft) {
  switch (ft) {
    case FaultType::OCP:   return "Overcurrent";
    case FaultType::HHF:   return "HIT not reached";
    case FaultType::OLF:   return "Open-load";
    case FaultType::DPM:   return "Plunger movement";
    case FaultType::OVT:   return "Overtemperature";
    case FaultType::UVM:   return "Undervoltage";
    case FaultType::COMER: return "Communication error";
    default:               return "Unknown fault";
  }
}

// ============================================================================
// Structures
// ============================================================================

/**
 * @brief Channel configuration structure
 *
 * Stores configuration in **user units** (mA for CDR, % for VDR, ms for hit time)
 * and automatically computes register values when `toRegister()` is called.
 *
 * ### User Units (Primary Storage)
 *
 * **CDR Mode**:
 * - `hit_current_value` and `hold_current_value` are in **milliamps (mA)**
 * - Example: `hit_current_value = 500.0f` means 500 mA
 * - Requires `full_scale_current_ma` to be set for conversion to 7-bit register value
 *
 * **VDR Mode**:
 * - `hit_current_value` and `hold_current_value` are **duty cycle percentages (0-100)**
 * - Example: `hit_current_value = 80.0f` means 80% duty cycle
 *
 * **HIT Time**:
 * - `hit_time_ms` is in **milliseconds**
 * - `0.0f` = no HIT time
 * - `< 0.0f` or `>= 1000000.0f` = continuous (infinite)
 * - Requires `master_clock_80khz` and `chop_freq` for conversion to 8-bit register value
 *
 * ### Context Fields (Required for Conversion)
 *
 * - `full_scale_current_ma`: Full-scale current in mA (required for CDR mode)
 * - `master_clock_80khz`: Master clock 80 kHz base (required for hit_time_ms conversion)
 * - `chop_freq`: Chopping frequency divider (required for hit_time_ms conversion)
 *
 * ### Register Conversion
 *
 * - `toRegister()` computes raw register values from user units using context
 * - `fromRegister(val, full_scale_current_ma, master_clock_80khz)` converts raw register → user units
 *
 * ### Restrictions
 *
 * - `drive_mode` and `side_mode` can only be changed when:
 *   - All channels are OFF (ONCHx = 0 for all channels)
 *   - Both TRIGA and TRIGB inputs are logic-low
 *
 * - CDR mode (`drive_mode = CDR`) is only supported in low-side mode (`side_mode = LOW_SIDE`)
 *
 * - High-side mode (`side_mode = HIGH_SIDE`) only supports VDR mode
 *
 * - `half_full_scale` is only available for low-side applications
 *
 * @note See max22200_registers.hpp for detailed bit field definitions.
 * @note The driver's `GetChannelConfig()` automatically sets `full_scale_current_ma` and `master_clock_80khz` from
 *       `BoardConfig` and `StatusConfig` when reading from hardware.
 */
struct ChannelConfig {
  // ── User Units (Primary Storage) ──────────────────────────────────────────
  // These are the "proper units" - the class computes register values from these

  float    hit_current_value;  ///< HIT value in user units:
                               ///< CDR: current in mA (e.g. 500.0 for 500 mA)
                               ///< VDR: duty cycle in percent (e.g. 80.0 for 80%)
  float    hold_current_value; ///< HOLD value in user units:
                               ///< CDR: current in mA
                               ///< VDR: duty cycle in percent
  float    hit_time_ms;        ///< HIT time in milliseconds
                               ///< 0.0 = no HIT time
                               ///< < 0.0 or >= 1000000.0 = continuous (infinite)

  // ── Context for Conversion (Required for toRegister()) ───────────────────
  uint32_t full_scale_current_ma;  ///< Full-scale current in mA (for CDR mode conversion)
                                   ///< Must be set for CDR mode; ignored for VDR
  bool     master_clock_80khz;     ///< Master clock base (false = 100 kHz, true = 80 kHz)
                                   ///< Required for hit_time_ms → register conversion

  // ── Register Fields (Direct Mapping) ─────────────────────────────────────
  bool     half_full_scale;        ///< Half full-scale (false=1x, true=0.5x IFS)
                                   ///< When true: IFS halved, RON doubled; low-side only
  bool     trigger_from_pin;       ///< Trigger source (false=SPI ONCH, true=TRIG pin)
                                   ///< When true: Channel controlled by TRIGA/TRIGB pins
  DriveMode drive_mode;            ///< Drive mode: CDR (current) or VDR (voltage)
  SideMode  side_mode;             ///< Side mode: LOW_SIDE (CDR/VDR) or HIGH_SIDE (VDR only)
  ChopFreq  chop_freq;             ///< Chopping frequency divider (FREQ_CFG[1:0])
  bool      slew_rate_control_enabled;   ///< Slew rate control enable; reduces EMI; fCHOP < 50 kHz
  bool      open_load_detection_enabled; ///< Open-load detection enable
  bool      plunger_movement_detection_enabled; ///< Plunger movement detection enable (low-side only)
  bool      hit_current_check_enabled;   ///< HIT current check enable (HHF fault if IHIT not reached)

  /**
   * @brief Default constructor — safe defaults
   */
  ChannelConfig()
      : hit_current_value(0.0f), hold_current_value(0.0f), hit_time_ms(0.0f),
        full_scale_current_ma(1000), master_clock_80khz(false),
        half_full_scale(false), trigger_from_pin(false),
        drive_mode(DriveMode::CDR), side_mode(SideMode::LOW_SIDE),
        chop_freq(ChopFreq::FMAIN_DIV4), slew_rate_control_enabled(false),
        open_load_detection_enabled(false), plunger_movement_detection_enabled(false),
        hit_current_check_enabled(false) {}

  /**
   * @brief Build 32-bit register value from user units
   *
   * Computes raw register values from hit_current_value, hold_current_value,
   * and hit_time_ms using full_scale_current_ma, master_clock_80khz, and chop_freq for conversion.
   *
   * @note Requires full_scale_current_ma > 0 for CDR mode (for current conversion).
   * @note Uses master_clock_80khz and chop_freq for hit_time_ms → register conversion.
   */
  uint32_t toRegister() const {
    // Convert hit_current_value to raw 7-bit
    uint8_t hit_raw;
    if (drive_mode == DriveMode::CDR) {
      // CDR: hit_current_value is in mA
      if (full_scale_current_ma == 0) {
        hit_raw = 0;  // Invalid IFS
      } else if (hit_current_value >= static_cast<float>(full_scale_current_ma)) {
        hit_raw = 127;
      } else {
        uint32_t ma = static_cast<uint32_t>(hit_current_value + 0.5f);
        hit_raw = currentMaToRaw(full_scale_current_ma, ma);
      }
    } else {
      // VDR: hit_current_value is duty percent (0-100)
      if (hit_current_value <= 0.0f) {
        hit_raw = 0;
      } else if (hit_current_value >= 100.0f) {
        hit_raw = 127;
      } else {
        uint32_t r = static_cast<uint32_t>((hit_current_value / 100.0f) * 127.0f + 0.5f);
        hit_raw = (r > 127u) ? 127u : static_cast<uint8_t>(r);
      }
    }

    // Convert hold_current_value to raw 7-bit
    uint8_t hold_raw;
    if (drive_mode == DriveMode::CDR) {
      // CDR: hold_current_value is in mA
      if (full_scale_current_ma == 0) {
        hold_raw = 0;
      } else if (hold_current_value >= static_cast<float>(full_scale_current_ma)) {
        hold_raw = 127;
      } else {
        uint32_t ma = static_cast<uint32_t>(hold_current_value + 0.5f);
        hold_raw = currentMaToRaw(full_scale_current_ma, ma);
      }
    } else {
      // VDR: hold_current_value is duty percent (0-100)
      if (hold_current_value <= 0.0f) {
        hold_raw = 0;
      } else if (hold_current_value >= 100.0f) {
        hold_raw = 127;
      } else {
        uint32_t r = static_cast<uint32_t>((hold_current_value / 100.0f) * 127.0f + 0.5f);
        hold_raw = (r > 127u) ? 127u : static_cast<uint8_t>(r);
      }
    }

    // Convert hit_time_ms to raw 8-bit
    uint8_t hit_time_raw = hitTimeMsToRaw(hit_time_ms, master_clock_80khz, chop_freq);

    // Build register value
    uint32_t val = 0;
    if (half_full_scale) val |= CfgChReg::HFS_BIT;
    val |= (static_cast<uint32_t>(hold_raw & 0x7F) << CfgChReg::HOLD_SHIFT);
    if (trigger_from_pin) val |= CfgChReg::TRGNSPI_BIT;
    val |= (static_cast<uint32_t>(hit_raw & 0x7F) << CfgChReg::HIT_SHIFT);
    val |= (static_cast<uint32_t>(hit_time_raw) << CfgChReg::HITT_SHIFT);
    if (drive_mode == DriveMode::VDR) val |= CfgChReg::VDRNCDR_BIT;
    if (side_mode == SideMode::HIGH_SIDE) val |= CfgChReg::HSNLS_BIT;
    val |= (static_cast<uint32_t>(chop_freq) << CfgChReg::FREQ_CFG_SHIFT);
    if (slew_rate_control_enabled) val |= CfgChReg::SRC_BIT;
    if (open_load_detection_enabled) val |= CfgChReg::OL_EN_BIT;
    if (plunger_movement_detection_enabled) val |= CfgChReg::DPM_EN_BIT;
    if (hit_current_check_enabled) val |= CfgChReg::HHF_EN_BIT;
    return val;
  }

  /**
   * @brief Parse a 32-bit register value into user units
   *
   * Converts raw register values to user units (mA for CDR, % for VDR, ms for hit_time).
   * Requires full_scale_current_ma and master_clock_80khz to be set for proper conversion.
   *
   * @param val 32-bit register value from CFG_CHx
   * @param full_scale_current_ma_param Full-scale current in mA (for CDR mode conversion)
   * @param master_clock_80khz_param   Master clock 80 kHz base (for hit_time conversion)
   */
  void fromRegister(uint32_t val, uint32_t full_scale_current_ma_param, bool master_clock_80khz_param) {
    // Parse register fields
    half_full_scale = (val & CfgChReg::HFS_BIT) != 0;
    uint8_t hold_raw = static_cast<uint8_t>((val >> CfgChReg::HOLD_SHIFT) & 0x7F);
    trigger_from_pin = (val & CfgChReg::TRGNSPI_BIT) != 0;
    uint8_t hit_raw  = static_cast<uint8_t>((val >> CfgChReg::HIT_SHIFT) & 0x7F);
    uint8_t hit_time_raw = static_cast<uint8_t>((val >> CfgChReg::HITT_SHIFT) & 0xFF);
    drive_mode = (val & CfgChReg::VDRNCDR_BIT) ? DriveMode::VDR : DriveMode::CDR;
    side_mode  = (val & CfgChReg::HSNLS_BIT) ? SideMode::HIGH_SIDE : SideMode::LOW_SIDE;
    chop_freq  = static_cast<ChopFreq>((val >> CfgChReg::FREQ_CFG_SHIFT) & 0x03);
    slew_rate_control_enabled = (val & CfgChReg::SRC_BIT) != 0;
    open_load_detection_enabled = (val & CfgChReg::OL_EN_BIT) != 0;
    plunger_movement_detection_enabled = (val & CfgChReg::DPM_EN_BIT) != 0;
    hit_current_check_enabled = (val & CfgChReg::HHF_EN_BIT) != 0;

    // Store context
    full_scale_current_ma = full_scale_current_ma_param;
    master_clock_80khz = master_clock_80khz_param;

    // Convert raw → user units
    if (drive_mode == DriveMode::CDR) {
      // CDR: raw → mA
      if (full_scale_current_ma > 0) {
        hit_current_value = (static_cast<float>(hit_raw) / 127.0f) * static_cast<float>(full_scale_current_ma);
        hold_current_value = (static_cast<float>(hold_raw) / 127.0f) * static_cast<float>(full_scale_current_ma);
      } else {
        hit_current_value = 0.0f;
        hold_current_value = 0.0f;
      }
    } else {
      // VDR: raw → duty percent
      hit_current_value = (static_cast<float>(hit_raw) / 127.0f) * 100.0f;
      hold_current_value = (static_cast<float>(hold_raw) / 127.0f) * 100.0f;
    }

    // Convert hit_time raw → ms
    if (hit_time_raw == 0) {
      hit_time_ms = 0.0f;
    } else if (hit_time_raw == 255) {
      hit_time_ms = -1.0f;  // Continuous (infinite)
    } else {
      uint32_t fchop_khz = getChopFreqKhz(master_clock_80khz, chop_freq);
      float fchop_hz = static_cast<float>(fchop_khz) * 1000.0f;
      hit_time_ms = (static_cast<float>(hit_time_raw) * 40.0f / fchop_hz) * 1000.0f;
    }
  }

  // ── Inline probe helpers (human-readable, compiler can optimize away) ─────

  bool isCdr() const { return drive_mode == DriveMode::CDR; }
  bool isVdr() const { return drive_mode == DriveMode::VDR; }
  bool isLowSide() const { return side_mode == SideMode::LOW_SIDE; }
  bool isHighSide() const { return side_mode == SideMode::HIGH_SIDE; }
  bool hasHitTime() const { return hit_time_ms > 0.0f; }
  bool isContinuousHit() const { return hit_time_ms < 0.0f || hit_time_ms >= 1000000.0f; }
  bool isTriggeredBySpi() const { return !trigger_from_pin; }
  bool isTriggeredByPin() const { return trigger_from_pin; }
  bool isHalfFullScale() const { return half_full_scale; }
  bool isSlewRateControlEnabled() const { return slew_rate_control_enabled; }
  bool isOpenLoadDetectionEnabled() const { return open_load_detection_enabled; }
  bool isPlungerMovementDetectionEnabled() const { return plunger_movement_detection_enabled; }
  bool isHitCurrentCheckEnabled() const { return hit_current_check_enabled; }
  /** @brief Chopping frequency config (FREQ_CFG); see ChopFreq enum */
  ChopFreq getChopFreq() const { return chop_freq; }
};

/**
 * @brief STATUS register structure
 *
 * Maps directly to the 32-bit STATUS register (0x00). Contains channel activation,
 * hardware configuration, fault masks, fault flags, and the global ACTIVE bit.
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
 *   Can be updated with fast 8-bit write to STATUS[31:24] for low-latency control.
 *
 * - **ACTIVE**: Global enable bit. Must be set to 1 for normal operation.
 *   When 0, device enters low-power mode and all channels are three-stated.
 *   Default is 0 at power-up. Set to 1 during initialization.
 *
 * - **UVM**: Undervoltage flag. Set high at power-up and must be cleared by
 *   reading the STATUS register. Reading STATUS clears UVM and deasserts nFAULT.
 *
 * - **CMxy**: Channel-pair mode bits (CM76, CM54, CM32, CM10). Configure pairs
 *   of contiguous channels for independent, parallel, or H-bridge operation.
 *   Can only be written when both channels in the pair are OFF (ONCHx=0, ONCHy=0).
 *
 * - **Fault Masks (M_OVT, M_OCP, etc.)**: When set to 1, the fault is masked
 *   and does not assert the FAULT pin. When 0 (default), fault events assert FAULT.
 *   M_COMF defaults to 1 (masked) at reset.
 *
 * @note Fault flags (bits 7:1) are read-only. They are cleared by reading the
 *       STATUS register (UVM) or FAULT register (OCP, HHF, OLF, DPM).
 * @note Reading STATUS register clears UVM flag and deasserts nFAULT pin.
 */
struct StatusConfig {
  // Byte 3 (bits 31:24)
  uint8_t channels_on_mask;  ///< Channel on/off bitmask (bit N = channel N)
                             ///< Can be updated with fast 8-bit write to STATUS[31:24]
                             ///< For channels with trigger_from_pin=true, ONCH bit is ignored (TRIG pin controls)

  // Byte 2 (bits 23:16) — fault masks (1=masked, 0=signaled on FAULT pin)
  bool overtemperature_masked;       ///< OVT fault mask; when true OVT does not assert FAULT pin
  bool overcurrent_masked;           ///< OCP fault mask; when true OCP does not assert FAULT pin
  bool open_load_fault_masked;       ///< OLF fault mask; when true OLF does not assert FAULT pin
  bool hit_not_reached_masked;       ///< HHF fault mask; when true HHF does not assert FAULT pin
  bool plunger_movement_fault_masked;///< DPM fault mask; when true DPM does not assert FAULT pin
  bool communication_error_masked;   ///< COMER mask (default=true); when true COMER does not assert FAULT pin
  bool undervoltage_masked;          ///< UVM fault mask; when true UVM does not assert FAULT pin
  bool master_clock_80khz;           ///< Master clock base (false=100kHz, true=80kHz); affects fCHOP

  // Byte 1 (bits 15:8) — channel-pair mode
  ChannelMode channel_pair_mode_76;  ///< CH6/CH7 pair mode; change only when both CH6 and CH7 are OFF
  ChannelMode channel_pair_mode_54;  ///< CH4/CH5 pair mode
  ChannelMode channel_pair_mode_32;  ///< CH2/CH3 pair mode
  ChannelMode channel_pair_mode_10;  ///< CH0/CH1 pair mode

  // Byte 0 (bits 7:0) — fault flags (read-only) + ACTIVE
  bool active;                      ///< Global enable (0=low-power, 1=normal); set to 1 during init

  // Read-only fault flags (populated on read, cleared by reading STATUS or FAULT register)
  bool overtemperature;             ///< OVT fault flag (read-only); die over temp
  bool overcurrent;                  ///< OCP fault flag (read-only); output exceeded OCP level
  bool open_load_fault;              ///< OLF fault flag (read-only); load disconnected if OL enabled
  bool hit_not_reached;              ///< HHF fault flag (read-only); IHIT not reached by end of tHIT
  bool plunger_movement_fault;       ///< DPM fault flag (read-only); no valid plunger movement
  bool communication_error;          ///< COMER flag (read-only); SPI error detected
  bool undervoltage;                 ///< UVM fault flag (read-only); clear by reading STATUS at init

  /**
   * @brief Default constructor
   */
  StatusConfig()
      : channels_on_mask(0), overtemperature_masked(false), overcurrent_masked(false),
        open_load_fault_masked(false), hit_not_reached_masked(false),
        plunger_movement_fault_masked(false), communication_error_masked(true),
        undervoltage_masked(false), master_clock_80khz(false),
        channel_pair_mode_76(ChannelMode::INDEPENDENT), channel_pair_mode_54(ChannelMode::INDEPENDENT),
        channel_pair_mode_32(ChannelMode::INDEPENDENT), channel_pair_mode_10(ChannelMode::INDEPENDENT),
        active(false), overtemperature(false), overcurrent(false), open_load_fault(false),
        hit_not_reached(false), plunger_movement_fault(false), communication_error(false),
        undervoltage(false) {}

  /**
   * @brief Build 32-bit register value from writable fields
   */
  uint32_t toRegister() const {
    uint32_t val = 0;
    val |= (static_cast<uint32_t>(channels_on_mask) << StatusReg::ONCH_SHIFT);
    if (overtemperature_masked)  val |= StatusReg::M_OVT_BIT;
    if (overcurrent_masked)      val |= StatusReg::M_OCP_BIT;
    if (open_load_fault_masked)  val |= StatusReg::M_OLF_BIT;
    if (hit_not_reached_masked) val |= StatusReg::M_HHF_BIT;
    if (plunger_movement_fault_masked) val |= StatusReg::M_DPM_BIT;
    if (communication_error_masked) val |= StatusReg::M_COMF_BIT;
    if (undervoltage_masked)     val |= StatusReg::M_UVM_BIT;
    if (master_clock_80khz)      val |= StatusReg::FREQM_BIT;
    val |= (static_cast<uint32_t>(channel_pair_mode_76) << StatusReg::CM76_SHIFT);
    val |= (static_cast<uint32_t>(channel_pair_mode_54) << StatusReg::CM54_SHIFT);
    val |= (static_cast<uint32_t>(channel_pair_mode_32) << StatusReg::CM32_SHIFT);
    val |= (static_cast<uint32_t>(channel_pair_mode_10) << StatusReg::CM10_SHIFT);
    if (active) val |= StatusReg::ACTIVE_BIT;
    return val;
  }

  /**
   * @brief Parse a 32-bit register value
   */
  void fromRegister(uint32_t val) {
    channels_on_mask = static_cast<uint8_t>((val >> StatusReg::ONCH_SHIFT) & 0xFF);
    overtemperature_masked = (val & StatusReg::M_OVT_BIT) != 0;
    overcurrent_masked     = (val & StatusReg::M_OCP_BIT) != 0;
    open_load_fault_masked = (val & StatusReg::M_OLF_BIT) != 0;
    hit_not_reached_masked = (val & StatusReg::M_HHF_BIT) != 0;
    plunger_movement_fault_masked = (val & StatusReg::M_DPM_BIT) != 0;
    communication_error_masked = (val & StatusReg::M_COMF_BIT) != 0;
    undervoltage_masked    = (val & StatusReg::M_UVM_BIT) != 0;
    master_clock_80khz     = (val & StatusReg::FREQM_BIT) != 0;
    channel_pair_mode_76   = static_cast<ChannelMode>((val >> StatusReg::CM76_SHIFT) & 0x03);
    channel_pair_mode_54   = static_cast<ChannelMode>((val >> StatusReg::CM54_SHIFT) & 0x03);
    channel_pair_mode_32   = static_cast<ChannelMode>((val >> StatusReg::CM32_SHIFT) & 0x03);
    channel_pair_mode_10   = static_cast<ChannelMode>((val >> StatusReg::CM10_SHIFT) & 0x03);
    overtemperature = (val & StatusReg::OVT_BIT) != 0;
    overcurrent     = (val & StatusReg::OCP_BIT) != 0;
    open_load_fault = (val & StatusReg::OLF_BIT) != 0;
    hit_not_reached = (val & StatusReg::HHF_BIT) != 0;
    plunger_movement_fault = (val & StatusReg::DPM_BIT) != 0;
    communication_error   = (val & StatusReg::COMER_BIT) != 0;
    undervoltage   = (val & StatusReg::UVM_BIT) != 0;
    active = (val & StatusReg::ACTIVE_BIT) != 0;
  }

  /**
   * @brief Check if any fault flag is active
   */
  bool hasFault() const {
    return overtemperature || overcurrent || open_load_fault || hit_not_reached
        || plunger_movement_fault || communication_error || undervoltage;
  }

  // ── Inline probe helpers ─────────────────────────────────────────────────

  bool hasOvertemperature() const { return overtemperature; }
  bool hasOvercurrent() const { return overcurrent; }
  bool hasOpenLoadFault() const { return open_load_fault; }
  bool hasHitNotReached() const { return hit_not_reached; }
  bool hasPlungerMovementFault() const { return plunger_movement_fault; }
  bool hasCommunicationError() const { return communication_error; }
  bool hasUndervoltage() const { return undervoltage; }
  bool isActive() const { return active; }
  bool isChannelOn(uint8_t ch) const {
    return ch < 8u && (channels_on_mask & (1u << ch)) != 0u;
  }
  uint8_t channelCountOn() const {
    uint8_t n = 0;
    for (uint8_t i = 0; i < 8; i++) {
      if (channels_on_mask & (1u << i)) n++;
    }
    return n;
  }

  bool isOvertemperatureMasked() const { return overtemperature_masked; }
  bool isOvercurrentMasked() const { return overcurrent_masked; }
  bool isOpenLoadFaultMasked() const { return open_load_fault_masked; }
  bool isHitNotReachedMasked() const { return hit_not_reached_masked; }
  bool isPlungerMovementFaultMasked() const { return plunger_movement_fault_masked; }
  bool isCommunicationErrorMasked() const { return communication_error_masked; }
  bool isUndervoltageMasked() const { return undervoltage_masked; }

  bool is80KHzBase() const { return master_clock_80khz; }
  bool is100KHzBase() const { return !master_clock_80khz; }

  ChannelMode getChannelPairMode76() const { return channel_pair_mode_76; }
  ChannelMode getChannelPairMode54() const { return channel_pair_mode_54; }
  ChannelMode getChannelPairMode32() const { return channel_pair_mode_32; }
  ChannelMode getChannelPairMode10() const { return channel_pair_mode_10; }
  uint8_t getChannelsOnMask() const { return channels_on_mask; }
};

/**
 * @brief Per-channel fault information from FAULT register (0x09)
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
struct FaultStatus {
  uint8_t overcurrent_channel_mask;            ///< OCP per-channel bitmask (bit N = channel N)
  uint8_t hit_not_reached_channel_mask;       ///< HHF per-channel bitmask (bit N = channel N)
  uint8_t open_load_fault_channel_mask;       ///< OLF per-channel bitmask (bit N = channel N)
  uint8_t plunger_movement_fault_channel_mask; ///< DPM per-channel bitmask (bit N = channel N)

  FaultStatus() : overcurrent_channel_mask(0), hit_not_reached_channel_mask(0),
                  open_load_fault_channel_mask(0), plunger_movement_fault_channel_mask(0) {}

  void fromRegister(uint32_t val) {
    overcurrent_channel_mask            = static_cast<uint8_t>((val >> FaultReg::OCP_SHIFT) & 0xFF);
    hit_not_reached_channel_mask        = static_cast<uint8_t>((val >> FaultReg::HHF_SHIFT) & 0xFF);
    open_load_fault_channel_mask        = static_cast<uint8_t>((val >> FaultReg::OLF_SHIFT) & 0xFF);
    plunger_movement_fault_channel_mask = static_cast<uint8_t>((val >> FaultReg::DPM_SHIFT) & 0xFF);
  }

  /**
   * @brief Check if any per-channel fault is active
   */
  bool hasFault() const {
    return (overcurrent_channel_mask | hit_not_reached_channel_mask
         | open_load_fault_channel_mask | plunger_movement_fault_channel_mask) != 0;
  }

  /**
   * @brief Count total per-channel faults
   */
  uint8_t getFaultCount() const {
    uint8_t count = 0;
    for (uint8_t i = 0; i < 8; i++) {
      if (overcurrent_channel_mask & (1u << i)) count++;
      if (hit_not_reached_channel_mask & (1u << i)) count++;
      if (open_load_fault_channel_mask & (1u << i)) count++;
      if (plunger_movement_fault_channel_mask & (1u << i)) count++;
    }
    return count;
  }

  // ── Inline probe helpers (human-readable, compiler can optimize away) ─────

  bool hasOvercurrent() const { return overcurrent_channel_mask != 0; }
  bool hasHitNotReached() const { return hit_not_reached_channel_mask != 0; }
  bool hasOpenLoadFault() const { return open_load_fault_channel_mask != 0; }
  bool hasPlungerMovementFault() const { return plunger_movement_fault_channel_mask != 0; }
  bool hasFaultOnChannel(uint8_t ch) const {
    return ch < NUM_CHANNELS_ && ((overcurrent_channel_mask | hit_not_reached_channel_mask
         | open_load_fault_channel_mask | plunger_movement_fault_channel_mask) & (1u << ch)) != 0u;
  }
  bool hasOvercurrentOnChannel(uint8_t ch) const {
    return ch < NUM_CHANNELS_ && (overcurrent_channel_mask & (1u << ch)) != 0u;
  }
  bool hasHitNotReachedOnChannel(uint8_t ch) const {
    return ch < NUM_CHANNELS_ && (hit_not_reached_channel_mask & (1u << ch)) != 0u;
  }
  bool hasOpenLoadFaultOnChannel(uint8_t ch) const {
    return ch < NUM_CHANNELS_ && (open_load_fault_channel_mask & (1u << ch)) != 0u;
  }
  bool hasPlungerMovementFaultOnChannel(uint8_t ch) const {
    return ch < NUM_CHANNELS_ && (plunger_movement_fault_channel_mask & (1u << ch)) != 0u;
  }
  uint8_t channelsWithAnyFault() const {
    return overcurrent_channel_mask | hit_not_reached_channel_mask
         | open_load_fault_channel_mask | plunger_movement_fault_channel_mask;
  }
};

/**
 * @brief DPM (Detection of Plunger Movement) algorithm configuration
 *
 * Maps to the CFG_DPM register (0x0A). Settings are global and apply to all
 * channels that have DPM_EN set. See datasheet "Detection of Plunger Movement
 * Register Description (CFG_DPM)".
 *
 * - plunger_movement_start_current: Starting current for DPM (0-127). ISTART = value × (IFS/127).
 * - plunger_movement_debounce_time: Debounce in chopping periods (0-15). TDEB = value / fCHOP.
 * - plunger_movement_current_threshold: Current dip threshold (0-15). IPTH = value × (IFS/127).
 */
struct DpmConfig {
  uint8_t plunger_movement_start_current;   ///< DPM_ISTART (0-127) — starting current for dip search
  uint8_t plunger_movement_debounce_time;   ///< DPM_TDEB (0-15) — min dip duration in chopping periods
  uint8_t plunger_movement_current_threshold; ///< DPM_IPTH (0-15) — min dip amplitude threshold

  DpmConfig() : plunger_movement_start_current(0), plunger_movement_debounce_time(0),
                plunger_movement_current_threshold(0) {}

  uint32_t toRegister() const {
    return (static_cast<uint32_t>(plunger_movement_start_current & 0x7Fu) << CfgDpmReg::DPM_ISTART_SHIFT) |
           (static_cast<uint32_t>(plunger_movement_debounce_time & 0x0Fu) << CfgDpmReg::DPM_TDEB_SHIFT) |
           (static_cast<uint32_t>(plunger_movement_current_threshold & 0x0Fu) << CfgDpmReg::DPM_IPTH_SHIFT);
  }

  void fromRegister(uint32_t val) {
    plunger_movement_start_current   = static_cast<uint8_t>((val >> CfgDpmReg::DPM_ISTART_SHIFT) & 0x7F);
    plunger_movement_debounce_time   = static_cast<uint8_t>((val >> CfgDpmReg::DPM_TDEB_SHIFT) & 0x0F);
    plunger_movement_current_threshold = static_cast<uint8_t>((val >> CfgDpmReg::DPM_IPTH_SHIFT) & 0x0F);
  }

  uint8_t getPlungerMovementStartCurrent() const { return plunger_movement_start_current; }
  uint8_t getPlungerMovementDebounceTime() const { return plunger_movement_debounce_time; }
  uint8_t getPlungerMovementCurrentThreshold() const { return plunger_movement_current_threshold; }
};

/**
 * @brief Full-bridge state for a channel pair (datasheet Table 7)
 *
 * For a pair (x, y) = (0,1), (2,3), (4,5), or (6,7): ONCHy and ONCHx determine
 * the bridge state. Forward uses CFG_CHy; Reverse uses CFG_CHx.
 */
enum class FullBridgeState : uint8_t {
  HiZ     = 0, ///< ONCHx=0, ONCHy=0 — high impedance
  Forward = 1, ///< ONCHx=1, ONCHy=0 — forward (control from CFG_CHy)
  Reverse = 2, ///< ONCHx=0, ONCHy=1 — reverse (control from CFG_CHx)
  Brake   = 3  ///< ONCHx=1, ONCHy=1 — brake (both outputs to GND)
};

/**
 * @brief Driver status enumeration
 */
enum class DriverStatus : uint8_t {
  OK = 0,               ///< Operation successful
  INITIALIZATION_ERROR,  ///< Initialization failed
  COMMUNICATION_ERROR,   ///< SPI communication error
  INVALID_PARAMETER,     ///< Invalid parameter provided
  HARDWARE_FAULT,        ///< Hardware fault detected
  TIMEOUT                ///< Operation timeout
};

/**
 * @brief Return a human-readable string for DriverStatus (for logging and tests)
 * @param s Status value
 * @return Non-null C string
 */
inline const char *DriverStatusToStr(DriverStatus s) {
  switch (s) {
    case DriverStatus::OK:
      return "OK";
    case DriverStatus::INITIALIZATION_ERROR:
      return "INITIALIZATION_ERROR";
    case DriverStatus::COMMUNICATION_ERROR:
      return "COMMUNICATION_ERROR";
    case DriverStatus::INVALID_PARAMETER:
      return "INVALID_PARAMETER";
    case DriverStatus::HARDWARE_FAULT:
      return "HARDWARE_FAULT";
    case DriverStatus::TIMEOUT:
      return "TIMEOUT";
    default:
      return "UNKNOWN";
  }
}

/**
 * @brief Channel state enumeration
 */
enum class ChannelState : uint8_t {
  DISABLED = 0,
  ENABLED,
  HIT_PHASE,
  HOLD_PHASE,
  FAULT
};

/**
 * @brief Array type for channel configurations
 */
using ChannelConfigArray = std::array<ChannelConfig, NUM_CHANNELS_>;

/**
 * @brief Callback function type for fault events
 */
using FaultCallback = void (*)(uint8_t channel, FaultType fault_type,
                               void *user_data);

/**
 * @brief Callback function type for channel state changes
 */
using StateChangeCallback = void (*)(uint8_t channel, ChannelState old_state,
                                     ChannelState new_state, void *user_data);

/**
 * @brief Board/scale configuration for unit-based APIs
 *
 * Stores the full-scale current (IFS) and optional safety limits for
 * current and duty cycle. Used by convenience APIs that work in real units
 * (mA, A, percent) instead of raw register values.
 *
 * ### Full-Scale Current (IFS)
 *
 * The IFS is determined by the external resistor RREF on the IREF pin:
 * - **IFS (mA) = KFS × 1000 / RREF (Ω)**
 * - **KFS = 15k** when HFS = 0 (full-scale, IFS up to 1000 mA)
 * - **KFS = 7.5k** when HFS = 1 (half-scale, IFS up to 500 mA)
 * - **RREF range**: 15kΩ to 100kΩ recommended
 *
 * ### Safety Limits
 *
 * - **max_current_ma**: Maximum allowed current in mA (0 = no limit).
 *   All current-setting APIs clamp to this value.
 * - **max_duty_percent**: Maximum allowed duty cycle in percent (0 = no limit).
 *   All duty-setting APIs clamp to this value (in addition to δMIN/δMAX).
 *
 * @note Set IFS based on your board's RREF value and HFS setting.
 * @note Safety limits are optional (0 = disabled). When set, they provide
 *       an additional safety layer beyond the device's hardware limits.
 */
struct BoardConfig {
  uint32_t full_scale_current_ma; ///< Full-scale current in mA (from RREF: IFS = KFS×1000/RREF)
  uint32_t max_current_ma;      ///< Max current limit in mA (0 = no limit, applies to all channels)
  uint8_t  max_duty_percent;    ///< Max duty limit in percent (0 = no limit, applies to VDR mode)

  BoardConfig() : full_scale_current_ma(1000), max_current_ma(0), max_duty_percent(0) {}

  /**
   * @brief Construct BoardConfig from RREF resistor value
   *
   * IFS = KFS × 1000 / RREF (KFS = 15 kΩ full-scale, 7.5 kΩ half-scale).
   *
   * @param rref_kohm RREF resistor value in kΩ (e.g., 15.0 for 15kΩ)
   * @param hfs       true if HFS=1 (half-scale), false if HFS=0 (full-scale)
   *
   * @example
   * @code
   * // RREF = 30kΩ, HFS = 0 → IFS = 15k × 1000 / 30k = 500 mA
   * BoardConfig config(30.0f, false);
   * @endcode
   */
  BoardConfig(float rref_kohm, bool hfs)
      : full_scale_current_ma(0), max_current_ma(0), max_duty_percent(0) {
    const float kfs = hfs ? 7.5f : 15.0f;  // KFS in kΩ
    full_scale_current_ma = static_cast<uint32_t>((kfs * 1000.0f) / rref_kohm + 0.5f);
  }

  /** @brief True if a max current limit is configured (0 = no limit) */
  bool hasMaxCurrentLimit() const { return max_current_ma > 0u; }
  /** @brief True if a max duty limit is configured (0 = no limit) */
  bool hasMaxDutyLimit() const { return max_duty_percent > 0u; }
  /** @brief True if IFS is configured (non-zero) */
  bool hasIfsConfigured() const { return full_scale_current_ma > 0u; }
  /** @brief Full-scale current in mA (IFS from RREF) */
  uint32_t getFullScaleCurrentMa() const { return full_scale_current_ma; }
  /** @brief Max current limit in mA (0 = no limit) */
  uint32_t getMaxCurrentLimitMa() const { return max_current_ma; }
  /** @brief Max duty limit in percent (0 = no limit) */
  uint8_t getMaxDutyLimitPercent() const { return max_duty_percent; }
};

/**
 * @brief Duty cycle limits (δMIN, δMAX) for a given configuration
 *
 * Returned by GetDutyLimits() based on FREQM, FREQ_CFG, and SRC settings.
 * See datasheet Table 2 for the complete table.
 */
struct DutyLimits {
  uint8_t min_percent;  ///< Minimum duty cycle in percent (δMIN)
  uint8_t max_percent;  ///< Maximum duty cycle in percent (δMAX)

  DutyLimits() : min_percent(4), max_percent(96) {}

  /** @brief Minimum duty cycle in percent (δMIN) */
  uint8_t getMinPercent() const { return min_percent; }
  /** @brief Maximum duty cycle in percent (δMAX) */
  uint8_t getMaxPercent() const { return max_percent; }
  /** @brief True if @p percent is within [min_percent, max_percent] */
  bool inRange(float percent) const {
    return percent >= static_cast<float>(min_percent) && percent <= static_cast<float>(max_percent);
  }
  /** @brief Clamp @p percent to [min_percent, max_percent] */
  float clamp(float percent) const {
    if (percent <= static_cast<float>(min_percent)) return static_cast<float>(min_percent);
    if (percent >= static_cast<float>(max_percent)) return static_cast<float>(max_percent);
    return percent;
  }
};

/**
 * @brief Driver statistics structure
 */
struct DriverStatistics {
  uint32_t total_transfers;
  uint32_t failed_transfers;
  uint32_t fault_events;
  uint32_t state_changes;
  uint32_t uptime_ms;

  DriverStatistics()
      : total_transfers(0), failed_transfers(0), fault_events(0),
        state_changes(0), uptime_ms(0) {}

  float getSuccessRate() const {
    if (total_transfers == 0) return 100.0f;
    return ((float)(total_transfers - failed_transfers) / total_transfers) * 100.0f;
  }

  bool hasFailures() const { return failed_transfers != 0u; }
  bool isHealthy() const { return failed_transfers == 0u; }
  uint32_t getTotalTransfers() const { return total_transfers; }
  uint32_t getFailedTransfers() const { return failed_transfers; }
  uint32_t getFaultEvents() const { return fault_events; }
  uint32_t getStateChanges() const { return state_changes; }
  uint32_t getUptimeMs() const { return uptime_ms; }
};

} // namespace max22200

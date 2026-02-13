/**
 * @file max22200_spi_interface.hpp
 * @brief CRTP-based template interface for SPI communication
 * @copyright Copyright (c) 2024-2025 HardFOC. All rights reserved.
 */
#pragma once
#include <cstddef>
#include <cstdint>

namespace max22200 {

// ============================================================================
// GPIO Enums -- Standardised Control Pin Model
// ============================================================================

/**
 * @enum CtrlPin
 * @brief Identifies the hardware control pins of the MAX22200.
 *
 * Used with `GpioSet()` / `GpioRead()` / `GpioSetActive()` / `GpioSetInactive()`
 * to control the IC's dedicated GPIO pins through the SpiInterface.
 *
 * The mapping from `GpioSignal::ACTIVE` / `INACTIVE` to physical HIGH / LOW
 * is determined by the platform bus implementation, based on the board's
 * active-level design:
 * - **ENABLE**: Active-high (ACTIVE → physical HIGH, enables driver outputs)
 * - **FAULT**:  Active-low  (ACTIVE → physical LOW, fault condition present)
 * - **CMD**:    Active-high (ACTIVE → physical HIGH, SPI register mode)
 */
enum class CtrlPin : uint8_t {
  ENABLE = 0, ///< Output enable (active-high on the physical pin)
  FAULT,      ///< Fault status output (active-low, open-drain)
  CMD         ///< Command mode select (HIGH = SPI register, LOW = direct drive)
};

/**
 * @enum GpioSignal
 * @brief Abstract signal level for control pins.
 *
 * Decouples the driver's intent from the physical pin polarity. The platform
 * bus implementation translates `ACTIVE` / `INACTIVE` to the correct
 * electrical level for each pin.
 */
enum class GpioSignal : uint8_t {
  INACTIVE = 0, ///< Pin function is deasserted
  ACTIVE   = 1  ///< Pin function is asserted
};

/**
 * @brief CRTP-based template interface for SPI communication
 *
 * This template class provides a hardware-agnostic interface for SPI
 * communication using the CRTP pattern. Platform-specific implementations
 * should inherit from this template with themselves as the template parameter.
 *
 * Benefits of CRTP:
 * - Compile-time polymorphism (no virtual function overhead)
 * - Static dispatch instead of dynamic dispatch
 * - Better optimization opportunities for the compiler
 *
 * Example usage:
 * @code
 * class MySPI : public max22200::SpiInterface<MySPI> {
 * public:
 *   bool Initialize() { ... }
 *   bool Transfer(...) { ... }
 *   // ... implement other methods
 * };
 * @endcode
 *
 * @tparam Derived The derived class type (CRTP pattern)
 */
template <typename Derived> class SpiInterface {
public:
  /**
   * @brief Initialize the SPI interface
   *
   * This method should perform any necessary initialization of the
   * SPI hardware, including setting up clock rates, modes, and
   * other configuration parameters.
   *
   * @return true if initialization was successful, false otherwise
   */
  bool Initialize() { return static_cast<Derived *>(this)->Initialize(); }

  /**
   * @brief Transfer data over SPI interface
   *
   * Performs a full-duplex SPI transfer. The method should handle
   * both transmission and reception of data simultaneously.
   *
   * @param tx_data Pointer to data buffer to transmit
   * @param rx_data Pointer to buffer for received data (can be same as tx_data)
   * @param length Number of bytes to transfer
   * @return true if transfer was successful, false otherwise
   *
   * @note Both tx_data and rx_data must be valid pointers and
   *       length must be greater than 0 for a successful transfer.
   */
  bool Transfer(const uint8_t *tx_data, uint8_t *rx_data, size_t length) {
    return static_cast<Derived *>(this)->Transfer(tx_data, rx_data, length);
  }

  /**
   * @brief Set chip select state
   *
   * Controls the chip select (CS) line for the SPI device.
   *
   * @param state true to assert CS (active low), false to deassert
   *
   * @note The actual polarity of CS depends on hardware implementation.
   *       This method should handle the correct CS polarity internally.
   */
  void SetChipSelect(bool state) {
    return static_cast<Derived *>(this)->SetChipSelect(state);
  }

  /**
   * @brief Configure SPI parameters
   *
   * Sets up SPI communication parameters such as clock speed,
   * data mode, and bit order.
   *
   * @param speed_hz SPI clock speed in Hz
   * @param mode SPI mode (0-3) defining clock polarity and phase
   * @param msb_first true for MSB first, false for LSB first
   * @return true if configuration was successful, false otherwise
   */
  bool Configure(uint32_t speed_hz, uint8_t mode, bool msb_first = true) {
    return static_cast<Derived *>(this)->Configure(speed_hz, mode, msb_first);
  }

  /**
   * @brief Check if SPI interface is ready
   *
   * Verifies that the SPI interface is properly initialized
   * and ready for communication.
   *
   * @return true if interface is ready, false otherwise
   */
  bool IsReady() const { return static_cast<const Derived *>(this)->IsReady(); }

  /**
   * @brief Blocking delay in microseconds (from comm/CRTP implementation).
   *
   * Used for device timing requirements, e.g. MAX22200 power-up delay
   * (0.5 ms after ENABLE) before first SPI access. Platform implements
   * the actual wait (e.g. esp_rom_delay_us, or RTOS-aware delay).
   *
   * @param us  Delay duration in microseconds.
   */
  void DelayUs(uint32_t us) { static_cast<Derived *>(this)->DelayUs(us); }

  // --------------------------------------------------------------------------
  /// @name GPIO Pin Control
  ///
  /// Unified interface for controlling MAX22200 hardware control pins. The
  /// platform bus implementation maps GpioSignal::ACTIVE/INACTIVE to the
  /// correct physical level for each pin based on its polarity.
  /// @{

  /**
   * @brief Set a control pin to the specified signal state.
   *
   * @param[in] pin     Which control pin to drive (ENABLE, CMD).
   * @param[in] signal  ACTIVE to assert the pin function, INACTIVE to deassert.
   */
  void GpioSet(CtrlPin pin, GpioSignal signal) {
    static_cast<Derived *>(this)->GpioSet(pin, signal);
  }

  /**
   * @brief Read the current state of a control pin.
   *
   * Primarily used for reading the FAULT pin status.
   *
   * @param[in]  pin     Which control pin to read.
   * @param[out] signal  Receives the current signal state.
   * @return true if the read was successful, false otherwise.
   */
  bool GpioRead(CtrlPin pin, GpioSignal &signal) {
    return static_cast<Derived *>(this)->GpioRead(pin, signal);
  }

  /**
   * @brief Assert a control pin (set to ACTIVE).
   * Convenience wrapper for `GpioSet(pin, GpioSignal::ACTIVE)`.
   * @param[in] pin  Which control pin to assert.
   */
  void GpioSetActive(CtrlPin pin) { GpioSet(pin, GpioSignal::ACTIVE); }

  /**
   * @brief Deassert a control pin (set to INACTIVE).
   * Convenience wrapper for `GpioSet(pin, GpioSignal::INACTIVE)`.
   * @param[in] pin  Which control pin to deassert.
   */
  void GpioSetInactive(CtrlPin pin) { GpioSet(pin, GpioSignal::INACTIVE); }

  /// @}

protected:
  /**
   * @brief Protected constructor to prevent direct instantiation
   */
  SpiInterface() = default;

  // Prevent copying
  SpiInterface(const SpiInterface &) = delete;
  SpiInterface &operator=(const SpiInterface &) = delete;

  // Allow moving
  SpiInterface(SpiInterface &&) = default;
  SpiInterface &operator=(SpiInterface &&) = default;

  /**
   * @brief Protected destructor
   * @note Derived classes can have public destructors
   */
  ~SpiInterface() = default;
};

} // namespace max22200

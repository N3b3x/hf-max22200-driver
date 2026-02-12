/**
 * @file max22200_spi_interface.hpp
 * @brief CRTP-based template interface for SPI communication
 * @copyright Copyright (c) 2024-2025 HardFOC. All rights reserved.
 */
#pragma once
#include <cstddef>
#include <cstdint>

namespace max22200 {

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

/**
 * @file SPIInterface.h
 * @brief Abstract base class for SPI communication interface
 * @author MAX22200 Driver Library
 * @date 2024
 *
 * This file defines the abstract base class for SPI communication,
 * providing hardware abstraction for the MAX22200 driver.
 */

#ifndef SPI_INTERFACE_H
#define SPI_INTERFACE_H

#include <cstddef>
#include <cstdint>

/**
 * @brief Abstract base class for SPI communication interface
 *
 * This class provides a hardware-agnostic interface for SPI communication
 * that must be implemented by platform-specific derived classes. It allows
 * the MAX22200 driver to work with any SPI implementation without being
 * tied to specific hardware details.
 *
 * @note This is a pure virtual class - derived classes must implement
 *       all virtual methods to be instantiable.
 */
class SPIInterface {
public:
  /**
   * @brief Virtual destructor
   *
   * Ensures proper cleanup of derived class objects when accessed
   * through base class pointers.
   */
  virtual ~SPIInterface() = default;

  /**
   * @brief Initialize the SPI interface
   *
   * This method should perform any necessary initialization of the
   * SPI hardware, including setting up clock rates, modes, and
   * other configuration parameters.
   *
   * @return true if initialization was successful, false otherwise
   */
  virtual bool initialize() = 0;

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
  virtual bool transfer(const uint8_t* tx_data, uint8_t* rx_data, size_t length) = 0;

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
  virtual void setChipSelect(bool state) = 0;

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
  virtual bool configure(uint32_t speed_hz, uint8_t mode, bool msb_first = true) = 0;

  /**
   * @brief Check if SPI interface is ready
   *
   * Verifies that the SPI interface is properly initialized
   * and ready for communication.
   *
   * @return true if interface is ready, false otherwise
   */
  virtual bool isReady() const = 0;
};

#endif // SPI_INTERFACE_H

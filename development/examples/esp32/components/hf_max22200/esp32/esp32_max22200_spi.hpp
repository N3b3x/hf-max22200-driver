/**
 * @file esp32_max22200_spi.hpp
 * @brief ESP32 SPI transport implementation for MAX22200 driver
 *
 * This file provides the ESP32-specific implementation of the
 * max22200::SpiInterface for communicating with MAX22200 octal solenoid and
 * motor driver over SPI.
 *
 * @author HardFOC Development Team
 * @date 2025
 * @copyright HardFOC
 */

#pragma once

#include "../../../inc/max22200_spi_interface.hpp"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include <cstddef>
#include <cstdint>

/**
 * @class Esp32Max22200Spi
 * @brief ESP32 SPI transport implementation for MAX22200 driver
 *
 * This class implements the max22200::SpiInterface using ESP-IDF's SPI
 * driver with CRTP pattern. It supports configurable SPI pins, frequency, and
 * chip select.
 */
class Esp32Max22200Spi : public max22200::SpiInterface<Esp32Max22200Spi> {
public:
  /**
   * @brief SPI configuration structure
   */
  struct SPIConfig {
    spi_host_device_t host = SPI2_HOST; ///< SPI host (SPI2_HOST for ESP32-C6)
    gpio_num_t miso_pin = GPIO_NUM_2;   ///< MISO pin (default GPIO2)
    gpio_num_t mosi_pin = GPIO_NUM_7;   ///< MOSI pin (default GPIO7)
    gpio_num_t sclk_pin = GPIO_NUM_6;   ///< SCLK pin (default GPIO6)
    gpio_num_t cs_pin = GPIO_NUM_10;    ///< CS pin (default GPIO10)
    uint32_t frequency = 10000000;      ///< SPI frequency in Hz (default 10MHz)
    uint8_t mode = 0;                   ///< SPI mode (default 0: CPOL=0, CPHA=0)
    uint8_t queue_size = 1;             ///< Transaction queue size
  };

  /**
   * @brief Constructor with default SPI configuration
   */
  Esp32Max22200Spi() : Esp32Max22200Spi(SPIConfig{}) {}

  /**
   * @brief Constructor with custom SPI configuration
   * @param config SPI configuration parameters
   */
  explicit Esp32Max22200Spi(const SPIConfig& config);

  /**
   * @brief Destructor - cleans up SPI resources
   */
  ~Esp32Max22200Spi();

  /**
   * @brief Initialize the SPI bus
   * @return true if successful, false otherwise
   */
  bool Initialize();

  /**
   * @brief Perform a full-duplex SPI data transfer
   * @param tx_data Pointer to data to transmit
   * @param rx_data Pointer to buffer for received data
   * @param length Number of bytes to transfer
   * @return true if successful, false otherwise
   */
  bool Transfer(const uint8_t* tx_data, uint8_t* rx_data, size_t length);

  /**
   * @brief Set chip select state
   * @param state true to assert CS (active low), false to deassert
   */
  void SetChipSelect(bool state);

  /**
   * @brief Configure SPI parameters
   * @param speed_hz SPI clock speed in Hz
   * @param mode SPI mode (0-3)
   * @param msb_first true for MSB first, false for LSB first
   * @return true if successful, false otherwise
   */
  bool Configure(uint32_t speed_hz, uint8_t mode, bool msb_first = true);

  /**
   * @brief Check if SPI interface is ready
   * @return true if ready, false otherwise
   */
  bool IsReady() const;

private:
  SPIConfig config_;               ///< SPI configuration
  spi_device_handle_t spi_device_; ///< SPI device handle
  bool initialized_;               ///< Initialization state
  static constexpr const char* TAG = "Esp32Max22200Spi"; ///< Logging tag

  /**
   * @brief Initialize SPI bus
   * @return true if successful, false otherwise
   */
  bool initializeSPI();

  /**
   * @brief Add SPI device to the bus
   * @return true if successful, false otherwise
   */
  bool addSPIDevice();
};


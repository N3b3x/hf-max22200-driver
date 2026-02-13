/**
 * @file esp32_max22200_bus.hpp
 * @brief ESP32 SPI transport implementation for MAX22200 driver (header-only)
 *
 * This file provides the ESP32-specific implementation of the
 * max22200::SpiInterface for communicating with MAX22200 octal solenoid and
 * motor driver over SPI.
 *
 * Header-only implementation — no separate .cpp file needed.
 *
 * @author N3b3x
 * @date 2025
 * @copyright HardFOC
 */

#pragma once

#include "../../../inc/max22200_spi_interface.hpp"
#include "esp32_max22200_test_config.hpp"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>

/**
 * @class Esp32Max22200SpiBus
 * @brief ESP32 SPI transport implementation for MAX22200 driver
 *
 * This class implements the max22200::SpiInterface using ESP-IDF's SPI
 * driver with CRTP pattern. It supports configurable SPI pins, frequency, and
 * chip select.
 */
class Esp32Max22200SpiBus : public max22200::SpiInterface<Esp32Max22200SpiBus> {
public:
  /**
   * @brief SPI configuration structure
   *
   * Pin members have no defaults; set them from your board config
   * (e.g. MAX22200_TestConfig::SPIPins) so wiring is explicit per target.
   */
  struct SPIConfig {
    spi_host_device_t host = SPI2_HOST; ///< SPI host (e.g. SPI2_HOST for ESP32-C6)
    gpio_num_t miso_pin;   ///< MISO pin (set from board config)
    gpio_num_t mosi_pin;   ///< MOSI pin (set from board config)
    gpio_num_t sclk_pin;   ///< SCLK pin (set from board config)
    gpio_num_t cs_pin;     ///< CS pin (set from board config)
    int16_t enable_pin = -1;  ///< ENABLE pin (active-high, -1 = not configured)
    int16_t fault_pin = -1;   ///< FAULT pin (active-low, open-drain input, -1 = not configured)
    int16_t cmd_pin = -1;     ///< CMD pin (active-high = SPI mode, -1 = not configured)
    int16_t triga_pin = -1;   ///< TRIGA trigger pin (direct drive, -1 = not configured)
    int16_t trigb_pin = -1;   ///< TRIGB trigger pin (direct drive, -1 = not configured)
    uint32_t frequency = 10000000;      ///< SPI frequency in Hz (default 10MHz)
    uint8_t mode = 0;       ///< SPI mode (default 0: CPOL=0, CPHA=0)
    uint8_t queue_size = 1; ///< Transaction queue size
    uint8_t cs_ena_pretrans = 1;  ///< CS asserted N clock cycles before transaction
    uint8_t cs_ena_posttrans = 1; ///< CS held N clock cycles after transaction
  };

  /**
   * @brief Constructor with SPI configuration (pins must be set by caller)
   * @param config SPI configuration parameters
   */
  explicit Esp32Max22200SpiBus(const SPIConfig &config)
      : config_(config), spi_device_(nullptr), initialized_(false) {}

  /**
   * @brief Destructor - cleans up SPI resources
   */
  ~Esp32Max22200SpiBus() {
    if (spi_device_ != nullptr) {
      spi_bus_remove_device(spi_device_);
      spi_bus_free(config_.host);
      spi_device_ = nullptr;
    }
  }

  /**
   * @brief Initialize the SPI bus
   * @return true if successful, false otherwise
   */
  bool Initialize() {
    if (initialized_) {
      return true;
    }

    if (!initializeGPIO()) {
      ESP_LOGE(TAG, "Failed to initialize GPIO pins");
      return false;
    }

    if (!initializeSPI()) {
      ESP_LOGE(TAG, "Failed to initialize SPI bus");
      return false;
    }

    if (!addSPIDevice()) {
      ESP_LOGE(TAG, "Failed to add SPI device");
      spi_bus_free(config_.host);
      return false;
    }

    initialized_ = true;
    ESP_LOGI(TAG, "SPI interface initialized successfully");
    return true;
  }

  /**
   * @brief Perform a full-duplex SPI data transfer
   * @param tx_data Pointer to data to transmit
   * @param rx_data Pointer to buffer for received data
   * @param length Number of bytes to transfer
   * @return true if successful, false otherwise
   */
  bool Transfer(const uint8_t *tx_data, uint8_t *rx_data, size_t length) {
    if (!initialized_ || spi_device_ == nullptr) {
      ESP_LOGE(TAG, "SPI not initialized");
      return false;
    }

    spi_transaction_t trans = {};
    trans.length = length * 8; // Length in bits
    trans.tx_buffer = tx_data;
    trans.rx_buffer = rx_data;

    esp_err_t ret = spi_device_transmit(spi_device_, &trans);
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "SPI transfer failed: %s", esp_err_to_name(ret));
      return false;
    }

    return true;
  }

  /**
   * @brief Set chip select state
   * @param state true to assert CS (active low), false to deassert
   */
  void SetChipSelect(bool state) {
    // CS is handled automatically by ESP-IDF SPI driver
    // This function is kept for interface compatibility
    (void)state;
  }

  /**
   * @brief Configure SPI parameters
   * @param speed_hz SPI clock speed in Hz
   * @param mode SPI mode (0-3)
   * @param msb_first true for MSB first, false for LSB first
   * @return true if successful, false otherwise
   */
  bool Configure(uint32_t speed_hz, uint8_t mode, bool msb_first = true) {
    if (!initialized_) {
      ESP_LOGE(TAG, "SPI not initialized");
      return false;
    }

    // ESP-IDF doesn't support runtime reconfiguration easily
    // Configuration is set during device addition
    (void)speed_hz;
    (void)mode;
    (void)msb_first;
    return true;
  }

  /**
   * @brief Check if SPI interface is ready
   * @return true if ready, false otherwise
   */
  bool IsReady() const {
    return initialized_ && (spi_device_ != nullptr);
  }

  // ── GPIO Pin Control ─────────────────────────────────────────────────

  /**
   * @brief Set a control pin to the specified signal state
   *
   * Maps GpioSignal to physical level based on pin polarity:
   * - ENABLE: active-high (ACTIVE → GPIO 1, INACTIVE → GPIO 0)
   * - CMD:    active-high (ACTIVE → GPIO 1, INACTIVE → GPIO 0)
   * - FAULT:  read-only, GpioSet on FAULT returns silently
   *
   * @param pin   Which control pin to drive
   * @param signal ACTIVE to assert, INACTIVE to deassert
   */
  void GpioSet(max22200::CtrlPin pin, max22200::GpioSignal signal) {
    int gpio_pin = -1;
    int level = 0;
    switch (pin) {
      case max22200::CtrlPin::ENABLE:
        gpio_pin = config_.enable_pin;
        level = (signal == max22200::GpioSignal::ACTIVE) ? 1 : 0; // Active-high
        break;
      case max22200::CtrlPin::CMD:
        gpio_pin = config_.cmd_pin;
        level = (signal == max22200::GpioSignal::ACTIVE) ? 1 : 0; // Active-high
        break;
      case max22200::CtrlPin::FAULT:
        return; // FAULT is read-only
    }
    if (gpio_pin >= 0) {
      gpio_set_level(static_cast<gpio_num_t>(gpio_pin), level);
    }
  }

  /**
   * @brief Read the current state of a control pin
   *
   * Primarily used for reading the FAULT pin:
   * - FAULT: active-low (GPIO 0 → ACTIVE, GPIO 1 → INACTIVE)
   *
   * @param pin    Which control pin to read
   * @param signal Receives the current signal state
   * @return true if read was successful, false if pin not configured
   */
  bool GpioRead(max22200::CtrlPin pin, max22200::GpioSignal &signal) {
    if (pin != max22200::CtrlPin::FAULT || config_.fault_pin < 0) {
      return false;
    }
    int level = gpio_get_level(static_cast<gpio_num_t>(config_.fault_pin));
    // FAULT is active-low: physical 0 = fault present (ACTIVE)
    signal = (level == 0) ? max22200::GpioSignal::ACTIVE
                          : max22200::GpioSignal::INACTIVE;
    return true;
  }

  /**
   * @brief Set TRIGA pin level (direct-drive trigger A)
   * @param active true = drive high (inactive), false = drive low (trigger)
   */
  void SetTrigA(bool active) {
    if (config_.triga_pin >= 0) {
      gpio_set_level(static_cast<gpio_num_t>(config_.triga_pin), active ? 1 : 0);
    }
  }

  /**
   * @brief Set TRIGB pin level (direct-drive trigger B)
   * @param active true = drive high (inactive), false = drive low (trigger)
   */
  void SetTrigB(bool active) {
    if (config_.trigb_pin >= 0) {
      gpio_set_level(static_cast<gpio_num_t>(config_.trigb_pin), active ? 1 : 0);
    }
  }

  /** @return true if TRIGA pin is configured */
  bool HasTrigA() const { return config_.triga_pin >= 0; }
  /** @return true if TRIGB pin is configured */
  bool HasTrigB() const { return config_.trigb_pin >= 0; }

private:
  SPIConfig config_;               ///< SPI configuration
  spi_device_handle_t spi_device_; ///< SPI device handle
  bool initialized_;               ///< Initialization state
  static constexpr const char *TAG = "Esp32Max22200SpiBus"; ///< Logging tag

  /**
   * @brief Initialize GPIO pins (ENABLE, FAULT, CMD)
   * @return true if successful, false otherwise
   */
  bool initializeGPIO() {
    // Configure output pins (ENABLE, CMD)
    auto configure_output = [this](int16_t pin, const char *name, int initial) -> bool {
      if (pin < 0) return true; // Not configured, skip
      gpio_config_t cfg = {
          .pin_bit_mask = (1ULL << pin),
          .mode = GPIO_MODE_OUTPUT,
          .pull_up_en = GPIO_PULLUP_DISABLE,
          .pull_down_en = GPIO_PULLDOWN_DISABLE,
          .intr_type = GPIO_INTR_DISABLE};
      if (gpio_config(&cfg) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure %s pin (GPIO%d)", name, pin);
        return false;
      }
      gpio_set_level(static_cast<gpio_num_t>(pin), initial);
      ESP_LOGI(TAG, "%s pin (GPIO%d) initialized, level=%d", name, pin, initial);
      return true;
    };
    if (!configure_output(config_.enable_pin, "ENABLE", 0)) return false;
    if (!configure_output(config_.cmd_pin, "CMD", 1)) return false; // CMD HIGH = SPI mode
    if (!configure_output(config_.triga_pin, "TRIGA", 1)) return false; // TRIGA high = inactive
    if (!configure_output(config_.trigb_pin, "TRIGB", 1)) return false; // TRIGB high = inactive

    // Configure FAULT as input with pull-up (active-low, open-drain)
    if (config_.fault_pin >= 0) {
      gpio_config_t cfg = {
          .pin_bit_mask = (1ULL << config_.fault_pin),
          .mode = GPIO_MODE_INPUT,
          .pull_up_en = GPIO_PULLUP_ENABLE,
          .pull_down_en = GPIO_PULLDOWN_DISABLE,
          .intr_type = GPIO_INTR_DISABLE};
      if (gpio_config(&cfg) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure FAULT pin (GPIO%d)", config_.fault_pin);
        return false;
      }
      ESP_LOGI(TAG, "FAULT pin (GPIO%d) initialized as input", config_.fault_pin);
    }
    return true;
  }

  /**
   * @brief Initialize SPI bus
   * @return true if successful, false otherwise
   */
  bool initializeSPI() {
    spi_bus_config_t bus_cfg = {};
    bus_cfg.mosi_io_num = config_.mosi_pin;
    bus_cfg.miso_io_num = config_.miso_pin;
    bus_cfg.sclk_io_num = config_.sclk_pin;
    bus_cfg.quadwp_io_num = -1;
    bus_cfg.quadhd_io_num = -1;
    bus_cfg.max_transfer_sz = 64; // MAX22200 uses 3-byte transfers
    bus_cfg.flags = SPICOMMON_BUSFLAG_MASTER;

    esp_err_t ret = spi_bus_initialize(config_.host, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(ret));
      return false;
    }

    return true;
  }

  /**
   * @brief Add SPI device to the bus
   * @return true if successful, false otherwise
   */
  bool addSPIDevice() {
    spi_device_interface_config_t dev_cfg = {};
    dev_cfg.command_bits = 0;
    dev_cfg.address_bits = 0;
    dev_cfg.dummy_bits = 0;
    dev_cfg.clock_speed_hz = config_.frequency;
    dev_cfg.mode = config_.mode;
    dev_cfg.duty_cycle_pos = 128;
    dev_cfg.spics_io_num = config_.cs_pin;
    dev_cfg.queue_size = config_.queue_size;
    dev_cfg.cs_ena_pretrans = config_.cs_ena_pretrans;
    dev_cfg.cs_ena_posttrans = config_.cs_ena_posttrans;
    dev_cfg.flags = 0;
    dev_cfg.input_delay_ns = 0;
    dev_cfg.pre_cb = nullptr;
    dev_cfg.post_cb = nullptr;

    esp_err_t ret = spi_bus_add_device(config_.host, &dev_cfg, &spi_device_);
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "Failed to add SPI device: %s", esp_err_to_name(ret));
      return false;
    }

    return true;
  }
};

/**
 * @brief Factory function to create a configured Esp32Max22200SpiBus instance
 *
 * Creates an Esp32Max22200SpiBus using pin and parameter values from
 * MAX22200_TestConfig namespace (esp32_max22200_test_config.hpp).
 *
 * @return std::unique_ptr<Esp32Max22200SpiBus> Configured SPI interface
 */
inline auto CreateEsp32Max22200SpiBus() noexcept -> std::unique_ptr<Esp32Max22200SpiBus> {
  using namespace MAX22200_TestConfig;

  Esp32Max22200SpiBus::SPIConfig config;

  // SPI pins from esp32_max22200_test_config.hpp
  config.host = SPI2_HOST;
  config.miso_pin = static_cast<gpio_num_t>(SPIPins::MISO);
  config.mosi_pin = static_cast<gpio_num_t>(SPIPins::MOSI);
  config.sclk_pin = static_cast<gpio_num_t>(SPIPins::SCLK);
  config.cs_pin = static_cast<gpio_num_t>(SPIPins::CS);

  // SPI parameters from esp32_max22200_test_config.hpp
  config.frequency = SPIParams::FREQUENCY;
  config.mode = SPIParams::MODE;
  config.queue_size = SPIParams::QUEUE_SIZE;
  config.cs_ena_pretrans = SPIParams::CS_ENA_PRETRANS;
  config.cs_ena_posttrans = SPIParams::CS_ENA_POSTTRANS;

  // Control pins from esp32_max22200_test_config.hpp
  config.enable_pin = ControlPins::ENABLE;
  config.fault_pin = ControlPins::FAULT;
  config.cmd_pin = ControlPins::CMD;
  config.triga_pin = ControlPins::TRIGA;
  config.trigb_pin = ControlPins::TRIGB;

  return std::make_unique<Esp32Max22200SpiBus>(config);
}

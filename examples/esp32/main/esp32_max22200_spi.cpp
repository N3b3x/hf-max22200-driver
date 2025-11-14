/**
 * @file esp32_max22200_spi.cpp
 * @brief ESP32 SPI transport implementation for MAX22200 driver
 *
 * @author HardFOC Development Team
 * @date 2025
 * @copyright HardFOC
 */

#include "esp32_max22200_spi.hpp"
#include "driver/spi_master.h"
#include "esp_log.h"
#include <cstring>

Esp32Max22200Spi::Esp32Max22200Spi(const SPIConfig &config)
    : config_(config), spi_device_(nullptr), initialized_(false) {}

Esp32Max22200Spi::~Esp32Max22200Spi() {
  if (spi_device_ != nullptr) {
    spi_bus_remove_device(spi_device_);
    spi_bus_free(config_.host);
    spi_device_ = nullptr;
  }
}

bool Esp32Max22200Spi::Initialize() {
  if (initialized_) {
    return true;
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

bool Esp32Max22200Spi::Transfer(const uint8_t *tx_data, uint8_t *rx_data,
                                size_t length) {
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

void Esp32Max22200Spi::SetChipSelect(bool state) {
  // CS is handled automatically by ESP-IDF SPI driver
  // This function is kept for interface compatibility
  (void)state;
}

bool Esp32Max22200Spi::Configure(uint32_t speed_hz, uint8_t mode,
                                 bool msb_first) {
  if (!initialized_) {
    ESP_LOGE(TAG, "SPI not initialized");
    return false;
  }

  // ESP-IDF doesn't support runtime reconfiguration easily
  // Configuration is set during device addition
  // For now, just validate parameters
  (void)speed_hz;
  (void)mode;
  (void)msb_first;
  return true;
}

bool Esp32Max22200Spi::IsReady() const {
  return initialized_ && (spi_device_ != nullptr);
}

bool Esp32Max22200Spi::initializeSPI() {
  spi_bus_config_t bus_cfg = {};
  bus_cfg.mosi_io_num = config_.mosi_pin;
  bus_cfg.miso_io_num = config_.miso_pin;
  bus_cfg.sclk_io_num = config_.sclk_pin;
  bus_cfg.quadwp_io_num = -1;
  bus_cfg.quadhd_io_num = -1;
  bus_cfg.max_transfer_sz = 64; // MAX22200 uses 3-byte transfers

  esp_err_t ret = spi_bus_initialize(config_.host, &bus_cfg, SPI_DMA_CH_AUTO);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(ret));
    return false;
  }

  return true;
}

bool Esp32Max22200Spi::addSPIDevice() {
  spi_device_interface_config_t dev_cfg = {};
  dev_cfg.clock_speed_hz = config_.frequency;
  dev_cfg.mode = config_.mode;
  dev_cfg.spics_io_num = config_.cs_pin;
  dev_cfg.queue_size = config_.queue_size;
  dev_cfg.flags = 0;
  dev_cfg.pre_cb = nullptr;

  esp_err_t ret = spi_bus_add_device(config_.host, &dev_cfg, &spi_device_);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to add SPI device: %s", esp_err_to_name(ret));
    return false;
  }

  return true;
}

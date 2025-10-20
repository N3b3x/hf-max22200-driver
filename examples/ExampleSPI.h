/**
 * @file ExampleSPI.h
 * @brief Example SPI implementation for demonstration purposes
 * @author MAX22200 Driver Library
 * @date 2024
 * 
 * This file contains an example implementation of the SPIInterface
 * for demonstration and testing purposes. In a real application,
 * this would be replaced with hardware-specific SPI implementation.
 */

#ifndef EXAMPLE_SPI_H
#define EXAMPLE_SPI_H

#include "../include/SPIInterface.h"
#include <cstdint>
#include <cstring>

namespace MAX22200 {

/**
 * @brief Example SPI implementation for demonstration
 * 
 * This class provides a simple implementation of the SPIInterface
 * for demonstration purposes. In a real embedded system, this would
 * be replaced with hardware-specific SPI implementation.
 * 
 * @note This implementation is for demonstration only and does not
 *       perform actual hardware SPI communication.
 */
class ExampleSPI : public SPIInterface {
public:
    /**
     * @brief Constructor
     * 
     * @param max_speed Maximum SPI speed in Hz
     */
    explicit ExampleSPI(uint32_t max_speed = 10000000);
    
    /**
     * @brief Destructor
     */
    virtual ~ExampleSPI() = default;
    
    // SPIInterface implementation
    bool initialize() override;
    bool transfer(const uint8_t* tx_data, uint8_t* rx_data, size_t length) override;
    void setChipSelect(bool state) override;
    bool configure(uint32_t speed_hz, uint8_t mode, bool msb_first = true) override;
    bool isReady() const override;

private:
    uint32_t max_speed_;        ///< Maximum SPI speed
    uint32_t current_speed_;    ///< Current SPI speed
    uint8_t current_mode_;      ///< Current SPI mode
    bool msb_first_;            ///< MSB first flag
    bool initialized_;          ///< Initialization state
    bool cs_state_;             ///< Chip select state
    
    // Statistics for demonstration
    uint32_t transfer_count_;   ///< Transfer count
    uint32_t error_count_;      ///< Error count
};

} // namespace MAX22200

#endif // EXAMPLE_SPI_H

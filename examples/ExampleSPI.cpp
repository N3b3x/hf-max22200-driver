/**
 * @file ExampleSPI.cpp
 * @brief Example SPI implementation for demonstration purposes
 * @author MAX22200 Driver Library
 * @date 2024
 * 
 * This file contains the implementation of the ExampleSPI class
 * for demonstration and testing purposes.
 */

#include "ExampleSPI.h"
#include <iostream>
#include <iomanip>

namespace MAX22200 {

// Constructor
ExampleSPI::ExampleSPI(uint32_t max_speed)
    : max_speed_(max_speed)
    , current_speed_(0)
    , current_mode_(0)
    , msb_first_(true)
    , initialized_(false)
    , cs_state_(true)
    , transfer_count_(0)
    , error_count_(0)
{
}

// Initialize SPI interface
bool ExampleSPI::initialize() {
    if (initialized_) {
        return true;
    }
    
    // Simulate SPI initialization
    std::cout << "[ExampleSPI] Initializing SPI interface..." << std::endl;
    std::cout << "[ExampleSPI] Max speed: " << max_speed_ << " Hz" << std::endl;
    
    initialized_ = true;
    return true;
}

// Transfer data over SPI
bool ExampleSPI::transfer(const uint8_t* tx_data, uint8_t* rx_data, size_t length) {
    if (!initialized_ || !tx_data || !rx_data || length == 0) {
        error_count_++;
        return false;
    }
    
    transfer_count_++;
    
    // Simulate SPI transfer
    std::cout << "[ExampleSPI] Transfer " << transfer_count_ << ": ";
    std::cout << "CS=" << (cs_state_ ? "HIGH" : "LOW") << ", ";
    std::cout << "Length=" << length << ", ";
    std::cout << "Speed=" << current_speed_ << "Hz, ";
    std::cout << "Mode=" << static_cast<int>(current_mode_) << ", ";
    std::cout << "MSB=" << (msb_first_ ? "First" : "Last") << std::endl;
    
    // Print TX data
    std::cout << "[ExampleSPI] TX: ";
    for (size_t i = 0; i < length; ++i) {
        std::cout << "0x" << std::hex << std::setfill('0') << std::setw(2) 
                  << static_cast<int>(tx_data[i]) << " ";
    }
    std::cout << std::dec << std::endl;
    
    // Simulate receiving data (echo back for demonstration)
    std::memcpy(rx_data, tx_data, length);
    
    // Print RX data
    std::cout << "[ExampleSPI] RX: ";
    for (size_t i = 0; i < length; ++i) {
        std::cout << "0x" << std::hex << std::setfill('0') << std::setw(2) 
                  << static_cast<int>(rx_data[i]) << " ";
    }
    std::cout << std::dec << std::endl;
    
    return true;
}

// Set chip select state
void ExampleSPI::setChipSelect(bool state) {
    cs_state_ = state;
    std::cout << "[ExampleSPI] Chip Select: " << (state ? "HIGH" : "LOW") << std::endl;
}

// Configure SPI parameters
bool ExampleSPI::configure(uint32_t speed_hz, uint8_t mode, bool msb_first) {
    if (!initialized_) {
        return false;
    }
    
    if (speed_hz > max_speed_) {
        std::cout << "[ExampleSPI] Warning: Requested speed " << speed_hz 
                  << " exceeds maximum " << max_speed_ << std::endl;
        speed_hz = max_speed_;
    }
    
    current_speed_ = speed_hz;
    current_mode_ = mode;
    msb_first_ = msb_first;
    
    std::cout << "[ExampleSPI] Configured: Speed=" << speed_hz 
              << "Hz, Mode=" << static_cast<int>(mode) 
              << ", MSB=" << (msb_first ? "First" : "Last") << std::endl;
    
    return true;
}

// Check if SPI interface is ready
bool ExampleSPI::isReady() const {
    return initialized_;
}

} // namespace MAX22200

/**
 * @file example_usage.cpp
 * @brief Example usage of MAX22200 driver library
 * @author MAX22200 Driver Library
 * @date 2024
 * 
 * This file demonstrates how to use the MAX22200 driver library
 * for controlling solenoid and motor drivers.
 */

#include "../include/MAX22200.h"
#include "ExampleSPI.h"
#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>

using namespace MAX22200;

/**
 * @brief Fault callback function
 * 
 * This function is called when a fault occurs on any channel.
 * 
 * @param channel Channel number where fault occurred
 * @param fault_type Type of fault
 * @param user_data User data pointer
 */
void faultCallback(uint8_t channel, FaultType fault_type, void* user_data) {
    std::cout << "[Fault Callback] Channel " << static_cast<int>(channel) 
              << " fault: ";
    
    switch (fault_type) {
        case FaultType::OCP:
            std::cout << "Overcurrent Protection";
            break;
        case FaultType::OL:
            std::cout << "Open Load";
            break;
        case FaultType::DPM:
            std::cout << "Detection of Plunger Movement";
            break;
        case FaultType::UVLO:
            std::cout << "Undervoltage Lockout";
            break;
        case FaultType::HHF:
            std::cout << "HIT Current Not Reached";
            break;
        case FaultType::TSD:
            std::cout << "Thermal Shutdown";
            break;
        default:
            std::cout << "Unknown";
            break;
    }
    std::cout << std::endl;
}

/**
 * @brief State change callback function
 * 
 * This function is called when a channel state changes.
 * 
 * @param channel Channel number
 * @param old_state Previous state
 * @param new_state New state
 * @param user_data User data pointer
 */
void stateChangeCallback(uint8_t channel, ChannelState old_state, 
                        ChannelState new_state, void* user_data) {
    std::cout << "[State Change] Channel " << static_cast<int>(channel) 
              << " changed from ";
    
    switch (old_state) {
        case ChannelState::DISABLED:
            std::cout << "DISABLED";
            break;
        case ChannelState::ENABLED:
            std::cout << "ENABLED";
            break;
        case ChannelState::HIT_PHASE:
            std::cout << "HIT_PHASE";
            break;
        case ChannelState::HOLD_PHASE:
            std::cout << "HOLD_PHASE";
            break;
        case ChannelState::FAULT:
            std::cout << "FAULT";
            break;
    }
    
    std::cout << " to ";
    
    switch (new_state) {
        case ChannelState::DISABLED:
            std::cout << "DISABLED";
            break;
        case ChannelState::ENABLED:
            std::cout << "ENABLED";
            break;
        case ChannelState::HIT_PHASE:
            std::cout << "HIT_PHASE";
            break;
        case ChannelState::HOLD_PHASE:
            std::cout << "HOLD_PHASE";
            break;
        case ChannelState::FAULT:
            std::cout << "FAULT";
            break;
    }
    std::cout << std::endl;
}

/**
 * @brief Print driver statistics
 * 
 * @param driver Reference to MAX22200 driver
 */
void printStatistics(const MAX22200& driver) {
    DriverStatistics stats;
    if (driver.getStatistics(stats) == DriverStatus::OK) {
        std::cout << "\n=== Driver Statistics ===" << std::endl;
        std::cout << "Total transfers: " << stats.total_transfers << std::endl;
        std::cout << "Failed transfers: " << stats.failed_transfers << std::endl;
        std::cout << "Fault events: " << stats.fault_events << std::endl;
        std::cout << "State changes: " << stats.state_changes << std::endl;
        std::cout << "Success rate: " << std::fixed << std::setprecision(2) 
                  << stats.getSuccessRate() << "%" << std::endl;
        std::cout << "Uptime: " << stats.uptime_ms << " ms" << std::endl;
    }
}

/**
 * @brief Print channel configuration
 * 
 * @param driver Reference to MAX22200 driver
 * @param channel Channel number
 */
void printChannelConfig(const MAX22200& driver, uint8_t channel) {
    ChannelConfig config;
    if (driver.getChannelConfig(channel, config) == DriverStatus::OK) {
        std::cout << "\n=== Channel " << static_cast<int>(channel) << " Configuration ===" << std::endl;
        std::cout << "Enabled: " << (config.enabled ? "Yes" : "No") << std::endl;
        std::cout << "Drive mode: " << (config.drive_mode == DriveMode::CDR ? "CDR" : "VDR") << std::endl;
        std::cout << "Bridge mode: " << (config.bridge_mode == BridgeMode::HALF_BRIDGE ? "Half" : "Full") << std::endl;
        std::cout << "Parallel mode: " << (config.parallel_mode ? "Yes" : "No") << std::endl;
        std::cout << "Polarity: " << (config.polarity == OutputPolarity::NORMAL ? "Normal" : "Inverted") << std::endl;
        std::cout << "HIT current: " << config.hit_current << std::endl;
        std::cout << "HOLD current: " << config.hold_current << std::endl;
        std::cout << "HIT time: " << config.hit_time << std::endl;
    }
}

/**
 * @brief Print fault status
 * 
 * @param driver Reference to MAX22200 driver
 */
void printFaultStatus(const MAX22200& driver) {
    FaultStatus status;
    if (driver.readFaultStatus(status) == DriverStatus::OK) {
        std::cout << "\n=== Fault Status ===" << std::endl;
        std::cout << "Overcurrent Protection: " << (status.overcurrent_protection ? "FAULT" : "OK") << std::endl;
        std::cout << "Open Load: " << (status.open_load ? "FAULT" : "OK") << std::endl;
        std::cout << "Plunger Movement: " << (status.plunger_movement ? "DETECTED" : "NONE") << std::endl;
        std::cout << "Undervoltage Lockout: " << (status.undervoltage_lockout ? "FAULT" : "OK") << std::endl;
        std::cout << "HIT Current Not Reached: " << (status.hit_current_not_reached ? "FAULT" : "OK") << std::endl;
        std::cout << "Thermal Shutdown: " << (status.thermal_shutdown ? "FAULT" : "OK") << std::endl;
        std::cout << "Active faults: " << static_cast<int>(status.getFaultCount()) << std::endl;
    }
}

/**
 * @brief Main function demonstrating MAX22200 driver usage
 */
int main() {
    std::cout << "MAX22200 Driver Library Example" << std::endl;
    std::cout << "Version: " << MAX22200::MAX22200::getVersion() << std::endl;
    std::cout << "=================================" << std::endl;
    
    // Create SPI interface
    ExampleSPI spi(10000000); // 10 MHz max speed
    
    // Create MAX22200 driver
    MAX22200::MAX22200 driver(spi, true); // Enable diagnostics
    
    // Set up callbacks
    driver.setFaultCallback(faultCallback, nullptr);
    driver.setStateChangeCallback(stateChangeCallback, nullptr);
    
    // Initialize the driver
    std::cout << "\nInitializing MAX22200 driver..." << std::endl;
    DriverStatus status = driver.initialize();
    if (status != DriverStatus::OK) {
        std::cout << "Failed to initialize driver. Status: " << static_cast<int>(status) << std::endl;
        return -1;
    }
    std::cout << "Driver initialized successfully!" << std::endl;
    
    // Configure global settings
    std::cout << "\nConfiguring global settings..." << std::endl;
    GlobalConfig global_config;
    global_config.diagnostic_enable = true;
    global_config.ics_enable = true;
    global_config.daisy_chain_mode = false;
    global_config.sleep_mode = false;
    
    status = driver.configureGlobal(global_config);
    if (status != DriverStatus::OK) {
        std::cout << "Failed to configure global settings. Status: " << static_cast<int>(status) << std::endl;
        return -1;
    }
    std::cout << "Global settings configured successfully!" << std::endl;
    
    // Configure individual channels
    std::cout << "\nConfiguring channels..." << std::endl;
    
    // Channel 0: Solenoid with CDR mode
    ChannelConfig ch0_config;
    ch0_config.enabled = true;
    ch0_config.drive_mode = DriveMode::CDR;
    ch0_config.bridge_mode = BridgeMode::HALF_BRIDGE;
    ch0_config.parallel_mode = false;
    ch0_config.polarity = OutputPolarity::NORMAL;
    ch0_config.hit_current = 800;  // High initial current
    ch0_config.hold_current = 200; // Lower holding current
    ch0_config.hit_time = 1000;    // 1ms hit time
    
    status = driver.configureChannel(0, ch0_config);
    if (status == DriverStatus::OK) {
        std::cout << "Channel 0 configured successfully!" << std::endl;
    }
    
    // Channel 1: Motor with VDR mode
    ChannelConfig ch1_config;
    ch1_config.enabled = true;
    ch1_config.drive_mode = DriveMode::VDR;
    ch1_config.bridge_mode = BridgeMode::FULL_BRIDGE;
    ch1_config.parallel_mode = false;
    ch1_config.polarity = OutputPolarity::NORMAL;
    ch1_config.hit_current = 500;
    ch1_config.hold_current = 300;
    ch1_config.hit_time = 2000;
    
    status = driver.configureChannel(1, ch1_config);
    if (status == DriverStatus::OK) {
        std::cout << "Channel 1 configured successfully!" << std::endl;
    }
    
    // Channel 2: Parallel mode configuration
    ChannelConfig ch2_config;
    ch2_config.enabled = true;
    ch2_config.drive_mode = DriveMode::CDR;
    ch2_config.bridge_mode = BridgeMode::HALF_BRIDGE;
    ch2_config.parallel_mode = true;  // Enable parallel mode
    ch2_config.polarity = OutputPolarity::NORMAL;
    ch2_config.hit_current = 600;
    ch2_config.hold_current = 150;
    ch2_config.hit_time = 1500;
    
    status = driver.configureChannel(2, ch2_config);
    if (status == DriverStatus::OK) {
        std::cout << "Channel 2 configured successfully!" << std::endl;
    }
    
    // Enable configured channels
    std::cout << "\nEnabling channels..." << std::endl;
    driver.enableChannel(0, true);
    driver.enableChannel(1, true);
    driver.enableChannel(2, true);
    
    // Read and display configurations
    printChannelConfig(driver, 0);
    printChannelConfig(driver, 1);
    printChannelConfig(driver, 2);
    
    // Read fault status
    printFaultStatus(driver);
    
    // Demonstrate current control
    std::cout << "\nDemonstrating current control..." << std::endl;
    
    // Set different currents for channel 0
    driver.setHitCurrent(0, 900);
    driver.setHoldCurrent(0, 100);
    
    // Read back current settings
    uint16_t hit_current, hold_current;
    if (driver.getCurrents(0, hit_current, hold_current) == DriverStatus::OK) {
        std::cout << "Channel 0 currents - HIT: " << hit_current 
                  << ", HOLD: " << hold_current << std::endl;
    }
    
    // Demonstrate timing control
    std::cout << "\nDemonstrating timing control..." << std::endl;
    driver.setHitTime(0, 500);  // 0.5ms hit time
    
    uint16_t hit_time;
    if (driver.getHitTime(0, hit_time) == DriverStatus::OK) {
        std::cout << "Channel 0 HIT time: " << hit_time << std::endl;
    }
    
    // Demonstrate channel status reading
    std::cout << "\nReading channel statuses..." << std::endl;
    ChannelStatusArray statuses;
    if (driver.readAllChannelStatuses(statuses) == DriverStatus::OK) {
        for (uint8_t i = 0; i < NUM_CHANNELS; ++i) {
            if (statuses[i].enabled) {
                std::cout << "Channel " << static_cast<int>(i) 
                          << ": Enabled, Current=" << statuses[i].current_reading
                          << ", Fault=" << (statuses[i].fault_active ? "Yes" : "No") << std::endl;
            }
        }
    }
    
    // Demonstrate sleep mode
    std::cout << "\nDemonstrating sleep mode..." << std::endl;
    driver.setSleepMode(true);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    driver.setSleepMode(false);
    std::cout << "Sleep mode toggled" << std::endl;
    
    // Print final statistics
    printStatistics(driver);
    
    // Deinitialize
    std::cout << "\nDeinitializing driver..." << std::endl;
    driver.deinitialize();
    std::cout << "Driver deinitialized successfully!" << std::endl;
    
    std::cout << "\nExample completed successfully!" << std::endl;
    return 0;
}

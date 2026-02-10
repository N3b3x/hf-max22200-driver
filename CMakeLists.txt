cmake_minimum_required(VERSION 3.16)

# MAX22200 Octal Solenoid and Motor Driver Component
# This component provides a hardware-agnostic driver for the MAX22200
# Supports SPI communication via CRTP compile-time polymorphism

set(COMPONENT_INCLUDE_DIRS "inc")

# Source files
# Note: The driver is header-only (CRTP template implementations are
#       included from headers), so no source files need to be compiled
set(COMPONENT_SRCS
)

# Component dependencies
set(COMPONENT_REQUIRES
    freertos
    esp_common
    esp_hw_support
    esp_system
    log
    soc
    hal
    esp_driver_gpio
    esp_driver_spi
)

# Register the component
idf_component_register(
    SRCS ${COMPONENT_SRCS}
    INCLUDE_DIRS ${COMPONENT_INCLUDE_DIRS}
    REQUIRES ${COMPONENT_REQUIRES}
)

# Set C++ standard (MAX22200 driver uses C++20)
target_compile_features(${COMPONENT_LIB} PRIVATE cxx_std_20)

# Add compiler flags for better C++ support
target_compile_options(${COMPONENT_LIB} PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
    $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
    $<$<COMPILE_LANGUAGE:CXX>:-std=gnu++20>
)

# Define component version (optional)
set_property(TARGET ${COMPONENT_LIB} PROPERTY VERSION "1.0.0")

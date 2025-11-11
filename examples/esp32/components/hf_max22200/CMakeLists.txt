# =============================================================================
# MAX22200 Driver Component - CMakeLists.txt
# =============================================================================
# This component provides the MAX22200 driver library
# =============================================================================

cmake_minimum_required(VERSION 3.16)

# =============================================================================
# Path Configuration
# =============================================================================
# Set the source root path relative to this component directory
set(SRC_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../src")
set(INC_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../inc")

message(STATUS "MAX22200 Driver - ESP32 Component Configuration")
message(STATUS "Source Root: ${SRC_ROOT}")

# =============================================================================
# Source File Discovery and Validation
# =============================================================================
# List all potential driver source files
set(DRIVER_SRCS
    "${SRC_ROOT}/MAX22200.cpp"
)

# Check which source files actually exist and add them
set(EXISTING_SRCS "")
foreach(src_file ${DRIVER_SRCS})
    if(EXISTS "${src_file}")
        list(APPEND EXISTING_SRCS "${src_file}")
        message(STATUS "Found source: ${src_file}")
    else()
        message(WARNING "Source file not found: ${src_file}")
    endif()
endforeach()

# =============================================================================
# Include Directories Configuration
# =============================================================================
# Define include directories for the driver component
set(DRIVER_INCLUDE_DIRS
    "${INC_ROOT}"                       # Main driver headers
)

# =============================================================================
# Component Dependencies
# =============================================================================
# Define required ESP-IDF components for the driver
set(DRIVER_REQUIRES
    driver              # ESP-IDF driver framework
)

# =============================================================================
# Component Registration
# =============================================================================
# Register the driver component with ESP-IDF build system
idf_component_register(
    SRCS ${EXISTING_SRCS}                 # Source files (validated above)
    INCLUDE_DIRS ${DRIVER_INCLUDE_DIRS}   # Include directories defined above
    REQUIRES ${DRIVER_REQUIRES}           # Component dependencies defined above
)

# =============================================================================
# Compiler Configuration
# =============================================================================
# Set C++ standard
target_compile_features(${COMPONENT_LIB} PRIVATE cxx_std_17)

# =============================================================================
# Compiler Flags
# =============================================================================
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wall                    # Enable all warnings
    -Wextra                  # Enable extra warnings
    -O2                      # Optimize for performance
    -g                       # Include debug information
)

# =============================================================================
# Build Summary
# =============================================================================
list(LENGTH EXISTING_SRCS NUM_SOURCES)
message(STATUS "=== MAX22200 Driver Component Summary ===")
message(STATUS "Component configured successfully")
message(STATUS "Included ${NUM_SOURCES} source files")
message(STATUS "C++ Standard: 17")
message(STATUS "===============================================")

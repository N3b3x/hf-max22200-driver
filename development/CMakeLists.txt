cmake_minimum_required(VERSION 3.20)

project(MAX22200Driver 
    VERSION 1.0.0
    DESCRIPTION "MAX22200 Octal Solenoid and Motor Driver Library"
    LANGUAGES CXX
)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler-specific options
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG")
endif()

# Include directories
include_directories(inc)

# Create the library
# Note: MAX22200.cpp is a template implementation file included by the header,
#       so it should NOT be compiled separately
add_library(MAX22200Driver INTERFACE)
target_include_directories(MAX22200Driver INTERFACE inc)

# Set target properties
set_target_properties(MAX22200Driver PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    PUBLIC_HEADER inc/MAX22200.h inc/SPIInterface.h inc/MAX22200_Registers.h inc/MAX22200_Types.h
)

# Create example executable
add_executable(example_usage
    examples/example_usage.cpp
    examples/ExampleSPI.cpp
)

target_link_libraries(example_usage MAX22200Driver)

# Create example SPI library
add_library(ExampleSPI STATIC
    examples/ExampleSPI.cpp
)

target_link_libraries(ExampleSPI MAX22200Driver)

# Installation
install(TARGETS MAX22200Driver ExampleSPI
    EXPORT MAX22200DriverTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    PUBLIC_HEADER DESTINATION include
)

install(EXPORT MAX22200DriverTargets
    FILE MAX22200DriverTargets.cmake
    NAMESPACE MAX22200::
    DESTINATION lib/cmake/MAX22200Driver
)

# Create config file
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/MAX22200DriverConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/MAX22200DriverConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/MAX22200DriverConfig.cmake"
    INSTALL_DESTINATION lib/cmake/MAX22200Driver
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/MAX22200DriverConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/MAX22200DriverConfigVersion.cmake"
    DESTINATION lib/cmake/MAX22200Driver
)

# Documentation target (if Doxygen is available)
find_package(Doxygen QUIET)
if(DOXYGEN_FOUND)
    set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile.in)
    set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)
    
    configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)
    
    add_custom_target(docs
        COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM
    )
endif()

# Testing (if enabled)
option(BUILD_TESTS "Build tests" OFF)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Print configuration summary
message(STATUS "")
message(STATUS "MAX22200 Driver Library Configuration:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Install Prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "  Build Tests: ${BUILD_TESTS}")
message(STATUS "  Doxygen Found: ${DOXYGEN_FOUND}")
message(STATUS "")

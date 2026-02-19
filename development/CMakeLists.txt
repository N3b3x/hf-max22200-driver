#===============================================================================
# MAX22200 Driver - Root CMake
# Defines the main build target for the driver. Minimal, platform-agnostic.
#===============================================================================

cmake_minimum_required(VERSION 3.16)

project(hf_max22200_driver LANGUAGES CXX)

#===============================================================================
# Load shared build settings (target name, includes, sources, dependencies)
#===============================================================================
include(${CMAKE_CURRENT_LIST_DIR}/cmake/hf_max22200_build_settings.cmake)

#===============================================================================
# Target type selection
#===============================================================================
if(HF_MAX22200_SOURCE_FILES)
    add_library(${HF_MAX22200_TARGET_NAME} STATIC ${HF_MAX22200_SOURCE_FILES})
    target_include_directories(${HF_MAX22200_TARGET_NAME}
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/inc>
            $<BUILD_INTERFACE:${HF_MAX22200_VERSION_HEADER_DIR}>
            $<INSTALL_INTERFACE:include>
    )
    target_compile_features(${HF_MAX22200_TARGET_NAME} PUBLIC cxx_std_20)
else()
    add_library(${HF_MAX22200_TARGET_NAME} INTERFACE)
    target_include_directories(${HF_MAX22200_TARGET_NAME}
        INTERFACE
            $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/inc>
            $<BUILD_INTERFACE:${HF_MAX22200_VERSION_HEADER_DIR}>
            $<INSTALL_INTERFACE:include>
    )
    target_compile_features(${HF_MAX22200_TARGET_NAME} INTERFACE cxx_std_20)
endif()

add_library(hf::max22200 ALIAS ${HF_MAX22200_TARGET_NAME})

#===============================================================================
# Versioning
#===============================================================================
set_target_properties(${HF_MAX22200_TARGET_NAME} PROPERTIES VERSION ${HF_MAX22200_VERSION} SOVERSION ${HF_MAX22200_VERSION_MAJOR})

option(HF_MAX22200_ENABLE_WARNINGS "Enable extra warnings for MAX22200 driver" OFF)
if(HF_MAX22200_ENABLE_WARNINGS)
    if(TARGET ${HF_MAX22200_TARGET_NAME})
        target_compile_options(${HF_MAX22200_TARGET_NAME} INTERFACE -Wall -Wextra -Wpedantic)
    endif()
endif()

#===============================================================================
# Install and export support
#===============================================================================
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS ${HF_MAX22200_TARGET_NAME} EXPORT hf_max22200Targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY inc/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h" PATTERN "*.h.in" EXCLUDE)
install(FILES "${HF_MAX22200_VERSION_HEADER}" DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(EXPORT hf_max22200Targets
    FILE hf_max22200Targets.cmake
    NAMESPACE hf::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/hf_max22200
)

configure_package_config_file(
    "${CMAKE_CURRENT_LIST_DIR}/cmake/hf_max22200Config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/hf_max22200Config.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/hf_max22200
)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/hf_max22200ConfigVersion.cmake"
    VERSION ${HF_MAX22200_VERSION}
    COMPATIBILITY SameMajorVersion
)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/hf_max22200Config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/hf_max22200ConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/hf_max22200
)

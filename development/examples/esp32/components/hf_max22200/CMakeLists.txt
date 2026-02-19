###############################################################################
# HF MAX22200 Driver - ESP-IDF Component Wrapper
###############################################################################
# Note:
#   cmake_minimum_required() is intentionally omitted here — ESP-IDF's
#   project.cmake already sets it and adding another can cause policy conflicts.
###############################################################################

# =============================================================================
# Shared Build Contract Import
# =============================================================================
set(HF_MAX22200_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../.." CACHE PATH
    "Path to root of hf-max22200-driver (auto-detected from component location)")

get_filename_component(HF_MAX22200_ROOT "${HF_MAX22200_ROOT}" ABSOLUTE)
if(NOT EXISTS "${HF_MAX22200_ROOT}/cmake/hf_max22200_build_settings.cmake")
    message(FATAL_ERROR
        "Cannot find MAX22200 driver root.\n"
        "  Resolved path: ${HF_MAX22200_ROOT}\n"
        "  Expected file: ${HF_MAX22200_ROOT}/cmake/hf_max22200_build_settings.cmake\n"
        "  Fix: set HF_MAX22200_ROOT to the correct driver checkout path.")
endif()

include("${HF_MAX22200_ROOT}/cmake/hf_max22200_build_settings.cmake")

# =============================================================================
# ESP-IDF Component Registration
# =============================================================================
idf_component_register(
    SRCS ${HF_MAX22200_SOURCE_FILES}
    INCLUDE_DIRS ${HF_MAX22200_PUBLIC_INCLUDE_DIRS}
    REQUIRES ${HF_MAX22200_IDF_REQUIRES}
)

# =============================================================================
# C++ Standard Contract
# =============================================================================
if(HF_MAX22200_SOURCE_FILES)
    target_compile_features(${COMPONENT_LIB} PRIVATE cxx_std_20)
else()
    target_compile_features(${COMPONENT_LIB} INTERFACE cxx_std_20)
endif()

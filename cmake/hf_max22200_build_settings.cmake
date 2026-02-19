#===============================================================================
# MAX22200 Driver - Build Settings
# Shared variables for target name, includes, sources, and dependencies.
# This file is the SINGLE SOURCE OF TRUTH for the driver version.
#===============================================================================

include_guard(GLOBAL)

# Target name
set(HF_MAX22200_TARGET_NAME "hf_max22200")

#===============================================================================
# Versioning (single source of truth)
#===============================================================================
set(HF_MAX22200_VERSION_MAJOR 1)
set(HF_MAX22200_VERSION_MINOR 0)
set(HF_MAX22200_VERSION_PATCH 0)
set(HF_MAX22200_VERSION "${HF_MAX22200_VERSION_MAJOR}.${HF_MAX22200_VERSION_MINOR}.${HF_MAX22200_VERSION_PATCH}")

#===============================================================================
# Generate version header from template (into build directory)
#===============================================================================
set(HF_MAX22200_VERSION_TEMPLATE "${CMAKE_CURRENT_LIST_DIR}/../inc/max22200_version.h.in")
set(HF_MAX22200_VERSION_HEADER_DIR "${CMAKE_CURRENT_BINARY_DIR}/hf_max22200_generated")
set(HF_MAX22200_VERSION_HEADER     "${HF_MAX22200_VERSION_HEADER_DIR}/max22200_version.h")

file(MAKE_DIRECTORY "${HF_MAX22200_VERSION_HEADER_DIR}")

if(EXISTS "${HF_MAX22200_VERSION_TEMPLATE}")
    configure_file(
        "${HF_MAX22200_VERSION_TEMPLATE}"
        "${HF_MAX22200_VERSION_HEADER}"
        @ONLY
    )
    message(STATUS "MAX22200 driver v${HF_MAX22200_VERSION} — generated max22200_version.h in ${HF_MAX22200_VERSION_HEADER_DIR}")
else()
    message(WARNING "max22200_version.h.in not found at ${HF_MAX22200_VERSION_TEMPLATE}")
endif()

#===============================================================================
# Public include directories
#===============================================================================
set(HF_MAX22200_PUBLIC_INCLUDE_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/../inc"
    "${HF_MAX22200_VERSION_HEADER_DIR}"
)

#===============================================================================
# Source files (empty for header-only)
#===============================================================================
set(HF_MAX22200_SOURCE_FILES)

#===============================================================================
# ESP-IDF component dependencies
#===============================================================================
set(HF_MAX22200_IDF_REQUIRES driver)

---
layout: default
title: "⚙️ CMake Integration"
description: "How to integrate the MAX22200 driver into your CMake project"
nav_order: 5
parent: "📚 Documentation"
permalink: /docs/cmake_integration/
---

# MAX22200 — CMake Integration Guide

How to consume the MAX22200 driver in your CMake or ESP-IDF project.


---

## Quick Start (Generic CMake)

```cmake
add_subdirectory(external/hf-max22200-driver)
target_link_libraries(my_app PRIVATE hf::max22200)
```

---

## ESP-IDF Integration

Component wrapper: `examples/esp32/components/hf_max22200/`

```cmake
idf_component_register(
    SRCS "app_main.cpp"
    INCLUDE_DIRS "."
    REQUIRES hf_max22200 driver
)
target_compile_features(${COMPONENT_LIB} PRIVATE cxx_std_20)
```

---

## Build Variables

| Variable | Value |
|----------|-------|
| `HF_MAX22200_TARGET_NAME` | `hf_max22200` |
| `HF_MAX22200_VERSION` | Current `MAJOR.MINOR.PATCH` |
| `HF_MAX22200_PUBLIC_INCLUDE_DIRS` | `inc/` + generated header dir |
| `HF_MAX22200_SOURCE_FILES` | `""` (header-only) |
| `HF_MAX22200_IDF_REQUIRES` | `driver` |

---

**Navigation**
⬅️ [Back to Documentation Index](index.md)

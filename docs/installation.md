---
layout: default
title: "Installation"
nav_order: 1
parent: "Documentation"
permalink: /docs/installation/
---

# Installation

The driver is **header-only**. CMake generates `sfm_version.h` and exposes an
`INTERFACE` target `hf::sfm` for consumers.

## Requirements

- C++17 compiler (GCC 9+, Clang 10+, ESP-IDF toolchains)
- CMake 3.16+
- An I²C master that can do raw writes/reads to a 7-bit address (no register
  byte) — see [Quick start](quickstart.md)

## As a git submodule

```bash
git submodule add https://github.com/N3b3x/hf-sfm-flow-meter-driver.git external/hf-sfm-flow-meter-driver
git submodule update --init --recursive
```

Then in your `CMakeLists.txt`:

```cmake
add_subdirectory(external/hf-sfm-flow-meter-driver)
target_link_libraries(my_firmware PRIVATE hf::sfm)
```

## Without `add_subdirectory`

```cmake
include(external/hf-sfm-flow-meter-driver/cmake/hf_sfm_build_settings.cmake)
target_include_directories(my_firmware PRIVATE ${HF_SFM_PUBLIC_INCLUDE_DIRS})
```

## Host tests

```bash
cmake -S . -B build -DHF_SFM_BUILD_TESTS=ON
cmake --build build && ctest --test-dir build
```

The tests use a fake I²C adapter and replay data-sheet frames (CRC-8 vector
`0xBEEF → 0x92`, product identifier, scaling, gas gating).

## Repository layout

```
inc/          sfm.hpp (umbrella), sfm_driver.hpp, sfm_types.hpp,
              sfm_commands.hpp, sfm_i2c_interface.hpp, sfm_version.h.in
cmake/        build settings + package config template
docs/         this documentation
examples/     ESP-IDF (ESP32-S3) examples + scripts submodule
tests/host/   hardware-free unit tests
_config/      Doxygen, Jekyll, lint configuration
```

**Next:** [Quick start →](quickstart.md)

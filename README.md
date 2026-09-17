---
layout: default
title: "HardFOC SFM Flow Meter Driver"
description: "Hardware-agnostic C++17 I2C driver for Sensirion SFM4300 / SF06-family gas mass flow meters"
nav_order: 1
permalink: /
---

# HF-SFM Flow Meter Driver

**Header-only C++17 I²C driver for Sensirion SF06-family gas mass flow meters**
(SFM4300-20/-50, SFM3003, SFM3013, SFM3019, SFM3119): continuous measurement per
gas / binary mixture, single-frame flow + temperature + status read, on-sensor
averaging, scale/offset/unit query, sleep, product identifier, general-call reset.
Every received word is **CRC-8** checked (poly 0x31, init 0xFF).

[![C++](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![CI](https://github.com/N3b3x/hf-sfm-flow-meter-driver/actions/workflows/esp32-examples-build-ci.yml/badge.svg?branch=main)](https://github.com/N3b3x/hf-sfm-flow-meter-driver/actions/workflows/esp32-examples-build-ci.yml)
[![Docs](https://img.shields.io/badge/docs-GitHub%20Pages-blue)](https://n3b3x.github.io/hf-sfm-flow-meter-driver/)

## Table of contents

1. [Overview](#overview)
2. [Features](#features)
3. [Quick start](#quick-start)
4. [Documentation](#documentation)
5. [Examples](#examples)
6. [Official references](#official-references)
7. [License](#license)

## Overview

> **[Live documentation (GitHub Pages)](https://n3b3x.github.io/hf-sfm-flow-meter-driver/)** —
> Installation, I²C protocol tables, CMake, API summary, and troubleshooting.

The driver targets host firmware that closes a control loop on the flow reading:
the sensor samples every **0.5 ms** (τ63 < 5 ms) and, in *average-until-read*
mode, each read returns the mean since the previous read — so a fixed-rate poll
is anti-aliased without host filtering. Scale factor, offset and unit are read
from the device per gas table, and the **product identifier** is decoded into a
`Variant` so that gas tables the part does not carry (e.g. **CO2 on SFM4300-50-x**)
are refused instead of silently mis-measured.

## Features

- **CRTP** `sfm::I2cInterface<Derived>` — no virtual calls; you provide `Write` /
  `Read` / `DelayMs` / `EnsureInitialized` (+ optional general call).
- **`sfm::Driver<I2cT>`** — `StartContinuous(Gas, o2‰)`, `ReadMeasurement`,
  `ReadFlowOnly`, `ConfigureAveraging`, `UpdateConcentration`, `Stop`,
  `ReadScaleOffsetUnit`, `ReadProductIdentifier`, `EnterSleep` / `ExitSleep`, `SoftReset`.
- **Typed results** — `DriverResult<T>` with stable `DriverError` codes
  (`NoData` for the data-sheet NACK, `Crc`, `UnsupportedGas`, …).
- **Status word decode** — running command, averaging mode, gas fraction.
- **No heap allocation**; fits FreeRTOS / bare metal.
- **Host tests** (`-DHF_SFM_BUILD_TESTS=ON`) with a fake I²C replaying
  data-sheet frames; **ESP32-S3** examples.

## Quick start

CMake:

```cmake
add_subdirectory(/path/to/hf-sfm-flow-meter-driver)
target_link_libraries(your_target PRIVATE hf::sfm)
```

Code:

```cpp
#include "sfm.hpp"

MyI2c i2c;                                   // inherits sfm::I2cInterface<MyI2c>
sfm::Driver<MyI2c> dev(i2c, sfm::addr::kSfm4300Default);

auto id = dev.ReadProductIdentifier();       // → Variant (idle mode)
auto ok = dev.StartContinuous(sfm::Gas::CO2);// UnsupportedGas on a 50 slm part
auto m  = dev.ReadMeasurement();             // m.value.flow [slm], temperature_c, status
```

## Documentation

- [Installation](docs/installation.md) · [Quick start](docs/quickstart.md) ·
  [I²C protocol](docs/i2c_protocol.md) · [Hardware setup](docs/hardware_setup.md) ·
  [CMake integration](docs/cmake_integration.md) · [API reference](docs/api_reference.md) ·
  [Examples](docs/examples.md) · [Troubleshooting](docs/troubleshooting.md) ·
  [Datasheets](docs/datasheet/README.md)

## Examples

`examples/esp32/` — `sfm_minimal_example` (identifier + 1 Hz frame) and
`sfm_flow_demo` (500 Hz poll, fixed-N averaging, variant-aware gas). Build with
`./scripts/build_app.sh <app> Debug` after `git submodule update --init --recursive`.

## Official references

- Sensirion SFM4300 data sheet (§4 I²C command set, §4.5 scaling)
- [`Sensirion/embedded-i2c-sfm-sf06`](https://github.com/Sensirion/embedded-i2c-sfm-sf06) reference driver (BSD-3) — see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)

## License

MIT — see [LICENSE](LICENSE). Protocol constants cross-checked against Sensirion's
BSD-3 reference implementation; notice reproduced in `THIRD_PARTY_NOTICES.md`.

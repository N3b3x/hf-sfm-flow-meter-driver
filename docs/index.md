---
layout: default
title: "Documentation"
description: "Complete documentation for the HardFOC Sensirion SFM4300 / SF06 I2C flow-meter driver"
nav_order: 2
parent: "HardFOC SFM Flow Meter Driver"
permalink: /docs/
has_children: true
---

# HF-SFM driver documentation

This site mirrors the [`docs/`](https://github.com/N3b3x/hf-sfm-flow-meter-driver/tree/main/docs) folder in the repository and documents the **Sensirion SF06 I²C command set** (SFM4300 data sheet §4) as implemented by this header-only driver. The same command set is shared by SFM3003, SFM3013, SFM3019 and SFM3119.

> **Browse on GitHub:** [repository home](https://github.com/N3b3x/hf-sfm-flow-meter-driver) · [Issues](https://github.com/N3b3x/hf-sfm-flow-meter-driver/issues)

## Documentation structure

### Getting started

1. **[Installation](installation.md)** — Toolchain, CMake, submodule layout
2. **[Quick start](quickstart.md)** — Minimal CRTP I²C adapter and first flow frame
3. **[I²C protocol](i2c_protocol.md)** — Commands, framing, CRC-8, scale/offset, status word

### Hardware and integration

4. **[Hardware setup](hardware_setup.md)** — Address strapping, pull-ups, supply, fittings, gas tables per variant
5. **[CMake integration](cmake_integration.md)** — `hf_sfm_build_settings.cmake`, `hf::sfm`

### Reference and examples

6. **[API reference](api_reference.md)** — `Driver<I2cT>`, types, results, status decode
7. **[Examples](examples.md)** — ESP32-S3 `build_app.sh` workflow
8. **[Troubleshooting](troubleshooting.md)** — NACKs, CRC errors, unsupported gas, averaging

### Manufacturer

9. **[Datasheet and links](datasheet/README.md)** — Bundled PDF and Sensirion references

---

**Back:** [Repository README →](../README.md)

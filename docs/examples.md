---
layout: default
title: "Examples"
nav_order: 7
parent: "Documentation"
permalink: /docs/examples/
---

# Examples

## ESP32-S3 (`examples/esp32`)

| App | Role |
|-----|------|
| `sfm_minimal_example` | reset, product identifier, Air table, 1 Hz frame |
| `sfm_flow_demo` | 500 Hz poll, fixed-N=4 averaging, CO2 table when available, per-second statistics |

```bash
cd examples/esp32
git submodule update --init --recursive
./scripts/build_app.sh sfm_minimal_example Debug
./scripts/flash_app.sh sfm_minimal_example Debug
```

Wiring and address details: [Hardware setup](hardware_setup.md) and
[`examples/esp32/README.md`](../examples/esp32/README.md).

## Transport adapter

`examples/esp32/main/include/hf_sfm_esp_i2c.hpp` — `SfmEspIdfI2c<port, sda, scl, hz>`
implements the four CRTP calls plus `WriteGeneralCallImpl` with the legacy
ESP-IDF `driver/i2c.h` API.

**Next:** [Troubleshooting →](troubleshooting.md)

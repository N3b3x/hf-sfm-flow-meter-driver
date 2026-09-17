# ESP32 examples — HF-SFM (Sensirion SFM4300 / SF06 family)

ESP-IDF examples for the **SF06 I2C command set** (SFM4300 data sheet §4).
Default address **0x2A** (ADDR pin to GND or floating), **400 kHz** I2C.

## Prerequisites

- ESP-IDF **v5.4+** (CI uses `release/v5.5`)
- Python 3 with PyYAML (for `scripts/generate_matrix.py`)

## One-time setup

From `examples/esp32/`:

```bash
git submodule update --init --recursive
```

This pulls `scripts/` (**hf-espidf-project-tools**): `build_app.sh`, `flash_app.sh`,
and `generate_matrix.py` (same layout as other `hf-*-driver` submodules).

## Default wiring (ESP32-S3)

| MCU signal | GPIO (default) | Sensor |
|------------|----------------|--------|
| SDA        | **8**          | SDA    |
| SCL        | **9**          | SCL    |
| 3V3 / 5V   | VDD            | 3.0–5.5 V per data sheet |
| GND        | GND            | GND    |

External pull-ups (2.2–4.7 kΩ) are required; the ESP32 internal pull-ups are only
a bench convenience. Change the `using I2c = SfmEspIdfI2c<...>` alias in each
`.cpp` to move pins or speed.

## Apps (`app_config.yml`)

| App | Role |
|-----|------|
| `sfm_minimal_example` | Reset, product identifier, Air table, 1 Hz frame |
| `sfm_flow_demo` | 500 Hz poll, fixed-N=4 averaging, CO2 table when available, 1 s statistics |

```bash
./scripts/build_app.sh list
./scripts/build_app.sh sfm_minimal_example Debug
./scripts/flash_app.sh sfm_minimal_example Debug
```

## Gas tables by variant

`SFM4300-20-x` carries O2, Air, N2O, CO2 and the three binary mixtures.
`SFM4300-50-x` carries **O2, Air and Air/O2 only** — `StartContinuous(Gas::CO2)`
returns `UnsupportedGas` once the variant is known from the product identifier.

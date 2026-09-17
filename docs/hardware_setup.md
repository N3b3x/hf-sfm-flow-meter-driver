---
layout: default
title: "Hardware setup"
nav_order: 4
parent: "Documentation"
permalink: /docs/hardware-setup/
---

# Hardware setup

## Electrical

- Supply **3.0–5.5 V** (SFM4300); I²C lines at the supply level.
- I²C **400 kHz** standard, up to **1 MHz Fm+**; external pull-ups required.
- `IRQn` (open-drain, active-low) pulses at 2 kHz / N when a new average is
  ready; optional.

## Address (SFM4300 `ADDR` pin)

| ADDR | 7-bit address |
|------|---------------|
| GND or floating | `0x2A` |
| GND via 1.2 kΩ | `0x2B` |
| GND via 2.7 kΩ | `0x2C` |
| GND via 5.6 kΩ | `0x2D` |

Other SF06 parts: SFM3119 `0x29`, SFM3003 `0x28`, SFM3013 `0x2F`, SFM3019 `0x2E`.

## Fittings (SFM4300)

| Suffix | Connection |
|--------|------------|
| `-B` | basemount — base plate with 7 mm inner-diameter ports, O-ring sealed; fix with the side screw holes only |
| `-O` | O-rings |
| `-P` | 8 mm Legris push-in fittings (8 mm OD tube) |

## Gas tables by variant

| Variant | Range | Calibrated tables |
|---------|-------|-------------------|
| SFM4300-20-x | 0–20 slm | O2, Air, N2O, CO2, Air/O2, N2O/O2, CO2/O2 |
| SFM4300-50-x | 0–50 slm | **O2, Air, Air/O2 only** |

The driver enforces this once the variant is known (`ReadProductIdentifier()`
or `AssumeVariant()`): `StartContinuous(Gas::CO2)` on a 50 slm part returns
`DriverError::UnsupportedGas`. Running a thermal flow sensor with the wrong gas
table yields large systematic errors — do not paper over this in the application.

## Pressure and orientation

Rated to 6 bar (line pressure), pressure drop < 25 mbar at 20 slm (-20) and
< 100 mbar at 50 slm (-50). Keep straight tube length upstream per the data
sheet for the stated accuracy.

**Next:** [CMake integration →](cmake_integration.md)

---
layout: default
title: "API reference"
nav_order: 6
parent: "Documentation"
permalink: /docs/api-reference/
---

# API reference

Authoritative declarations live under [`inc/`](https://github.com/N3b3x/hf-sfm-flow-meter-driver/tree/main/inc).
Run **Doxygen** (`doxygen _config/Doxyfile`) for HTML cross-links.

## Transport — `sfm::I2cInterface<Derived>`

CRTP base. Implement:

- `bool Write(uint8_t addr7, const uint8_t* data, size_t len);`
- `bool Read(uint8_t addr7, uint8_t* out, size_t len);` — `false` on NACK
- `void DelayMs(uint32_t ms);`
- `bool EnsureInitialized();`
- Optional `bool WriteGeneralCallImpl(uint8_t byte);`

## Driver — `sfm::Driver<I2cT>`

| Method | Description |
|--------|-------------|
| `Driver(i2c, address = 0x2A)` | bind transport + 7-bit address |
| `ReadProductIdentifier()` | `0xE102` → `ProductInfo` (id, serial, `Variant`); idle only |
| `AssumeVariant(v)` / `KnownVariant()` | set / query the variant used for gas gating |
| `ReadScaleOffsetUnit(gas)` | `0x3661` → `Scaling` |
| `StartContinuous(gas, o2_permille = 0)` | reads scaling, gates on variant, starts, waits 12 ms |
| `Stop()` | `0x3FF9` |
| `ReadMeasurement()` | 3 words → `Measurement` (flow, °C, `StatusWord`) |
| `ReadFlowOnly()` | 1 word → flow |
| `ConfigureAveraging(n)` | `0x366A`, 0 = average-until-read, 1…128 fixed-N |
| `UpdateConcentration(o2_permille)` | set + activate |
| `EnterSleep()` / `ExitSleep()` | §4.3.8 |
| `SoftReset()` | general call `0x06`; `NotSupported` without general-call transport |
| `ActiveGas()` / `ActiveScaling()` / `Measuring()` | session state |
| `EncodeWord()` / `DecodeWords()` | static framing helpers (also used by tests) |

## Types (`sfm_types.hpp`)

- `Gas` — O2, Air, N2O, CO2, AirO2Mix, N2OO2Mix, CO2O2Mix; `StartCommandFor(g)`, `IsMixture(g)`
- `Variant` — decoded from the product identifier; `FullScaleSlm(v)`, `SupportsGas(v, g)`
- `Scaling` — `scale`, `offset`, `unit`, `ToFlow(raw)`
- `StatusWord` — `command_nibble()`, `exponential_smoothing()`, `fixed_n_averaging()`, `gas_fraction_permille()`, `pure_gas()`
- `Measurement` — raw + engineering values
- `DriverResult<T>` / `DriverError` — `None, InvalidParameter, NotInitialized, BusWrite, BusRead, NoData, Crc, UnsupportedGas, NotSupported, UnexpectedUnit`

## Constants (`sfm_commands.hpp`)

`addr::*`, `cmd::*`, `timing::*`, `kFlowUnitSlm20C = 0x0148`,
`kTemperatureScalePerC = 200`, `Crc8()`, `Crc8Word()`.

**Next:** [Examples →](examples.md)

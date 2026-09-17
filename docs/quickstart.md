---
layout: default
title: "Quick start"
nav_order: 2
parent: "Documentation"
permalink: /docs/quickstart/
---

# Quick start

You supply a **CRTP** adapter that inherits `sfm::I2cInterface<YourType>` and
implements four calls: `Write`, `Read`, `DelayMs`, `EnsureInitialized`.
Optional `WriteGeneralCallImpl` enables `SoftReset()` (0x06 to address 0x00).

```cpp
#include "sfm.hpp"

class MyI2c : public sfm::I2cInterface<MyI2c> {
public:
    bool Write(uint8_t addr7, const uint8_t* data, size_t len) noexcept { /* START addr+W data STOP */ }
    bool Read(uint8_t addr7, uint8_t* out, size_t len) noexcept        { /* START addr+R read STOP; false on NACK */ }
    void DelayMs(uint32_t ms) noexcept                                  { /* RTOS / busy delay */ }
    bool EnsureInitialized() noexcept                                   { return true; }
    bool WriteGeneralCallImpl(uint8_t b) noexcept                       { /* optional */ }
};
```

## First frame

```cpp
MyI2c i2c;
sfm::Driver<MyI2c> dev(i2c, sfm::addr::kSfm4300Default);   // 0x2A

dev.SoftReset();                                            // optional, ~20 ms
auto id = dev.ReadProductIdentifier();                      // idle mode only
// id.value.variant → Sfm4300_20_P / Sfm4300_50_P / …

auto st = dev.StartContinuous(sfm::Gas::CO2);               // reads scale/offset first
if (!st.ok()) { /* UnsupportedGas on SFM4300-50-x → fall back to Gas::Air */ }

dev.ConfigureAveraging(0);                                  // average-until-read (default)

auto m = dev.ReadMeasurement();                             // ≥ 0.5 ms after the last read
if (m.ok()) {
    float slm = m.value.flow;                               // (raw − offset) / scale
    float tc  = m.value.temperature_c;                      // raw / 200
    bool pure = m.value.status.pure_gas();
}
```

## Polling model

The sensor measures every ~0.5 ms. With average-until-read (N = 0) each read
returns the mean of all samples since the previous read, so a 500 Hz poll is
anti-aliased for free. With fixed-N averaging, a read before a new average is
ready is NACKed → `DriverError::NoData`; treat it as "no update", not a fault.

## Result handling

Every call returns `sfm::DriverResult<T>`: check `.ok()` / `.error`.
`ToString(error)` gives a stable string for logs.

**Next:** [I²C protocol →](i2c_protocol.md)

---
layout: default
title: "I²C protocol"
nav_order: 3
parent: "Documentation"
permalink: /docs/i2c-protocol/
---

# I²C protocol (SF06 command set)

Source: SFM4300 data sheet §4. All values in `inc/sfm_commands.hpp`.

## Framing

- Commands are **16-bit**, big-endian, written without a register byte.
- Optional **16-bit argument** follows the command and carries a **CRC-8**.
- Reads return **16-bit words, each followed by CRC-8**; the driver verifies
  every word (`DriverError::Crc` on mismatch).
- CRC-8: polynomial **0x31**, init **0xFF**, no reflection, no final XOR.
  Test vector `0xBEEF → 0x92`.

## Commands

| Command | Code | Argument | Notes |
|---------|------|----------|-------|
| Start O2 | `0x3603` | — | Gas 0 |
| Start Air | `0x3608` | — | Gas 1 |
| Start N2O | `0x3615` | — | Gas 2 (SFM4300-20 only) |
| Start CO2 | `0x361E` | — | Gas 3 (SFM4300-20 only) |
| Start Air/O2 | `0x3632` | O2 ‰ | mixture 0 |
| Start N2O/O2 | `0x3639` | O2 ‰ | mixture 1 (SFM4300-20 only) |
| Start CO2/O2 | `0x3646` | O2 ‰ | mixture 2 (SFM4300-20 only) |
| Update concentration (set) | `0xE17D` | O2 ‰ | then activate |
| Update concentration (activate) | `0xE000` | — | |
| Stop continuous | `0x3FF9` | — | → idle |
| Configure averaging | `0x366A` | N | 0 = average-until-read, 1…128 fixed-N |
| Read scale / offset / unit | `0x3661` | start code | 3 words back |
| Enter sleep | `0x3677` | — | idle only |
| Exit sleep | any write header | — | not ACKed; poll ≤ 16 ms |
| Read product identifier | `0xE102` | — | 6 words: id(2) + serial(4); idle only |
| Soft reset | `0x06` to **0x00** | — | general call, ~16–20 ms |

## Measurement frame (after a start command)

```
Flow MSB, Flow LSB, CRC, Temp MSB, Temp LSB, CRC, Status MSB, Status LSB, CRC
```

The read may stop after any word (flow-only read = 3 bytes). A read header
while no fresh sample exists is **NACKed** (`DriverError::NoData`).

## Scaling (§4.5)

`flow = (raw − offset) / scale`, `temperature_c = raw / 200`.
SFM4300: scale **1000 per slm**, offset **−28672**, unit code **0x0148**
(slm at 20 °C, 1013.25 mbar). The driver reads them from the device per gas so
other SF06 parts work unchanged.

## Status word (§4.3.2)

| Bits | Meaning |
|------|---------|
| 15:12 | running command (0 O2, 1 Air, 2 N2O, 3 CO2, 4 Air/O2, 5 N2O/O2, 6 CO2/O2) |
| 11 | exponential smoothing active (average-until-read idle > 64 ms) |
| 10 | fixed-N averaging active |
| 9:0 | O2 fraction (‰) for mixtures, `0x3FF` for a pure gas |

## Timing

| Event | Time |
|-------|------|
| First result after start | 12 ms (accuracy settles by 30 ms) |
| Internal sample period | ~0.5 ms (2 kHz) |
| Soft reset | 16 ms typ., 20 ms max |
| Warm-up after reset / wake | 30 ms |

**Next:** [Hardware setup →](hardware_setup.md)

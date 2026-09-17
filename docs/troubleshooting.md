---
layout: default
title: "Troubleshooting"
nav_order: 8
parent: "Documentation"
permalink: /docs/troubleshooting/
---

# Troubleshooting

## `NoData` on every read

- No start command was issued, or `Stop()` was sent — the sensor NACKs reads in idle.
- Fixed-N averaging with a poll faster than N × 0.5 ms — expected; skip the tick.
- Wrong address: check the `ADDR` strap table in [Hardware setup](hardware_setup.md).

## `Crc` errors

- Bus noise or missing pull-ups; reduce to 100–400 kHz and check rise times.
- Reading fewer than 3 bytes per word boundary on a custom transport.

## `UnsupportedGas`

The known variant does not carry that lookup table (e.g. CO2 on SFM4300-50-x).
Use `Gas::Air` / `Gas::O2` or fit the 20 slm part. Do not bypass with
`AssumeVariant(Variant::Unknown)` in production — the reading would be
systematically wrong.

## `BusWrite` on `ReadProductIdentifier()`

The command is idle-only. Call `Stop()` first, or read the identifier before
`StartContinuous()`.

## `NotSupported` from `SoftReset()`

Your transport lacks `WriteGeneralCallImpl` (general-call address 0x00).
Either implement it or power-cycle the sensor.

## First readings a few % off

Data sheet: accuracy settles ~30 ms after start; discard the first frames.

**Back:** [Documentation home →](index.md)

/**
 * @file sfm_types.hpp
 * @brief Flow-meter types: families, gases, variants, readings, status word, results.
 *
 * @details Two Sensirion families share framing and CRC but not gas codes:
 *          - **SF06** (SFM4300, SFM3003/3013/3019/3119): flow + temperature +
 *            status per read, on-chip averaging, sleep.
 *          - **SFx6000** (SFM6000D): flow + reserved + status per read at
 *            1 kHz, temperature through an output-buffer pointer, no
 *            averaging or sleep, scale/offset read through pointer 0xE151.
 *          Scaling: flow = (raw − offset) / scale, read from the device per
 *          gas table. The family is learned from the product identifier;
 *          until it is, only O2 and Air may start (the only codes that mean
 *          the same gas on both families).
 *
 * @copyright Copyright (c) 2026 HardFOC. All rights reserved.
 */
#pragma once

#include "sfm_commands.hpp"

#include <cstdint>
#include <string_view>

namespace sfm {

/// Driver-level error codes (stable for logging / telemetry).
enum class DriverError : uint8_t {
    None = 0,
    InvalidParameter,   ///< bad argument (e.g. averaging window > 128)
    NotInitialized,     ///< transport `EnsureInitialized()` failed
    BusWrite,           ///< command write NACKed / bus error
    BusRead,            ///< read failed (bus error)
    NoData,             ///< read NACKed — no fresh sample yet / not measuring
    Crc,                ///< CRC-8 mismatch on a received word
    UnsupportedGas,     ///< gas / mixture not calibrated on this variant
    NotSupported,       ///< transport lacks the feature (e.g. general call)
    UnexpectedUnit,     ///< flow-unit code differs from the expected slm code
    VariantUnknown,     ///< gas needs a known part family (identity not read)
};

constexpr std::string_view ToString(DriverError e) noexcept {
    switch (e) {
        case DriverError::None:             return "None";
        case DriverError::InvalidParameter: return "InvalidParameter";
        case DriverError::NotInitialized:   return "NotInitialized";
        case DriverError::BusWrite:         return "BusWrite";
        case DriverError::BusRead:          return "BusRead";
        case DriverError::NoData:           return "NoData";
        case DriverError::Crc:              return "Crc";
        case DriverError::UnsupportedGas:   return "UnsupportedGas";
        case DriverError::NotSupported:     return "NotSupported";
        case DriverError::UnexpectedUnit:   return "UnexpectedUnit";
        case DriverError::VariantUnknown:   return "VariantUnknown";
    }
    return "?";
}

template <typename T>
struct DriverResult {
    T value{};
    DriverError error{DriverError::None};

    constexpr bool ok() const noexcept { return error == DriverError::None; }
    constexpr explicit operator bool() const noexcept { return ok(); }

    static constexpr DriverResult success(T v) noexcept { return {v, DriverError::None}; }
    static constexpr DriverResult failure(DriverError e) noexcept { return {T{}, e}; }
};

template <>
struct DriverResult<void> {
    DriverError error{DriverError::None};
    constexpr bool ok() const noexcept { return error == DriverError::None; }
    constexpr explicit operator bool() const noexcept { return ok(); }
    static constexpr DriverResult success() noexcept { return {DriverError::None}; }
    static constexpr DriverResult failure(DriverError e) noexcept { return {e}; }
};

/// Part family: decides command meaning, frame layout and optional features.
enum class Family : uint8_t {
    Sf06 = 0,  ///< SFM4300, SFM3003/3013/3019/3119
    Sfx6000,   ///< SFM6000D (SFC6000D controllers share the interface)
};

constexpr std::string_view ToString(Family f) noexcept {
    return f == Family::Sfx6000 ? "SFx6000" : "SF06";
}

/// Calibrated gas / gas-mixture lookup table selector (family-neutral).
enum class Gas : uint8_t {
    O2 = 0,      ///< SF06 Gas 0 · SFx6000 Gas 0
    Air,         ///< SF06 Gas 1 · SFx6000 Gas 1
    N2O,         ///< SF06 Gas 2 (SFM4300-20 only) · SFx6000 Gas 3
    CO2,         ///< SF06 Gas 3 (SFM4300-20 only) · SFx6000 Gas 2
    AirO2Mix,    ///< O2 in Air — argument: O2 volume fraction (‰)
    N2OO2Mix,    ///< SF06 mixture 1 — SFM4300-20 only
    CO2O2Mix,    ///< SF06 mixture 2 — SFM4300-20 only
    Ar,          ///< SFx6000 Gas 4 only
};

constexpr std::string_view ToString(Gas g) noexcept {
    switch (g) {
        case Gas::O2:       return "O2";
        case Gas::Air:      return "Air";
        case Gas::N2O:      return "N2O";
        case Gas::CO2:      return "CO2";
        case Gas::AirO2Mix: return "Air/O2";
        case Gas::N2OO2Mix: return "N2O/O2";
        case Gas::CO2O2Mix: return "CO2/O2";
        case Gas::Ar:       return "Ar";
    }
    return "?";
}

/**
 * @brief Start-measurement command code for @p g on @p family.
 * @return 0 when the family has no table for @p g.
 * @warning The same code means different gases on the two families
 *          (0x3615 / 0x361E). Always pass the family the part reported.
 */
constexpr uint16_t StartCommandFor(Family family, Gas g) noexcept {
    if (family == Family::Sfx6000) {
        switch (g) {
            case Gas::O2:       return cmd::kSfx6000StartGas0;
            case Gas::Air:      return cmd::kSfx6000StartGas1;
            case Gas::CO2:      return cmd::kSfx6000StartGas2;
            case Gas::N2O:      return cmd::kSfx6000StartGas3;
            case Gas::Ar:       return cmd::kSfx6000StartGas4;
            case Gas::AirO2Mix: return cmd::kSfx6000StartMix0;
            case Gas::N2OO2Mix:
            case Gas::CO2O2Mix: return 0;
        }
        return 0;
    }
    switch (g) {
        case Gas::O2:       return cmd::kStartO2;
        case Gas::Air:      return cmd::kStartAir;
        case Gas::N2O:      return cmd::kStartN2O;
        case Gas::CO2:      return cmd::kStartCO2;
        case Gas::AirO2Mix: return cmd::kStartAirO2Mix;
        case Gas::N2OO2Mix: return cmd::kStartN2OO2Mix;
        case Gas::CO2O2Mix: return cmd::kStartCO2O2Mix;
        case Gas::Ar:       return 0;
    }
    return 0;
}

static_assert(StartCommandFor(Family::Sf06, Gas::CO2) == 0x361E &&
                  StartCommandFor(Family::Sfx6000, Gas::CO2) == 0x3615,
              "CO2 start code differs by family (SFM4300 data sheet / SFx6000 reference)");

constexpr bool IsMixture(Gas g) noexcept {
    return g == Gas::AirO2Mix || g == Gas::N2OO2Mix || g == Gas::CO2O2Mix;
}

/// Part variant decoded from the product identifier (data sheet Table 13).
enum class Variant : uint8_t {
    Unknown = 0,
    Sfm4300_20_B,  ///< 0x0403011x — 20 slm, basemount
    Sfm4300_20_O,  ///< 0x0403021x — 20 slm, O-rings
    Sfm4300_20_P,  ///< 0x0403031x — 20 slm, push-in fittings
    Sfm4300_50_B,  ///< 0x0403091x — 50 slm, basemount
    Sfm4300_50_O,  ///< 0x0403071x — 50 slm, O-rings
    Sfm4300_50_P,  ///< 0x0403061x — 50 slm, push-in fittings
    Sfm6000D_50,   ///< 0x060211xx — 50 slm (Air/O2; 20 slm CO2/N2O/Ar)
    Sfm6000D_20,   ///< 0x060212xx — 20 slm (Air/O2; 10 slm CO2/N2O/Ar)
    Sfm6000D_5,    ///< 0x060214xx — 5 slm (Air/O2; 2 slm CO2/N2O/Ar)
};

constexpr std::string_view ToString(Variant v) noexcept {
    switch (v) {
        case Variant::Unknown:      return "Unknown";
        case Variant::Sfm4300_20_B: return "SFM4300-20-B";
        case Variant::Sfm4300_20_O: return "SFM4300-20-O";
        case Variant::Sfm4300_20_P: return "SFM4300-20-P";
        case Variant::Sfm4300_50_B: return "SFM4300-50-B";
        case Variant::Sfm4300_50_O: return "SFM4300-50-O";
        case Variant::Sfm4300_50_P: return "SFM4300-50-P";
        case Variant::Sfm6000D_50:  return "SFM6000D-50";
        case Variant::Sfm6000D_20:  return "SFM6000D-20";
        case Variant::Sfm6000D_5:   return "SFM6000D-5";
    }
    return "?";
}

/// Family of a known variant (Sf06 for Unknown — the caller's assumption).
constexpr Family FamilyOf(Variant v) noexcept {
    switch (v) {
        case Variant::Sfm6000D_50:
        case Variant::Sfm6000D_20:
        case Variant::Sfm6000D_5:   return Family::Sfx6000;
        default:                    return Family::Sf06;
    }
}

/**
 * @brief Decode a 32-bit product identifier.
 * @details SF06: low nibble = revision. SFx6000: low byte = revision
 *          (subject to change while the parts are preliminary).
 */
constexpr Variant VariantFromProductId(uint32_t product_id) noexcept {
    switch (product_id & 0xFFFFFF00U) {
        case 0x06021100U: return Variant::Sfm6000D_50;
        case 0x06021200U: return Variant::Sfm6000D_20;
        case 0x06021400U: return Variant::Sfm6000D_5;
        default:          break;
    }
    /* SF06: 0x0403 <variant> <revision>. The low byte is a revision — parts
     * ship as 0x...1x and 0x...01 (an SFM4300-20-P read 0x04030301 on the
     * 2026-10-02 bench) — so it is ignored. */
    switch (product_id & 0xFFFFFF00U) {
        case 0x04030100U: return Variant::Sfm4300_20_B;
        case 0x04030200U: return Variant::Sfm4300_20_O;
        case 0x04030300U: return Variant::Sfm4300_20_P;
        case 0x04030900U: return Variant::Sfm4300_50_B;
        case 0x04030700U: return Variant::Sfm4300_50_O;
        case 0x04030600U: return Variant::Sfm4300_50_P;
        default:          return Variant::Unknown;
    }
}

/// Full-scale Air / O2 flow [slm] for a known variant (0 when unknown).
constexpr float FullScaleSlm(Variant v) noexcept {
    switch (v) {
        case Variant::Sfm4300_20_B:
        case Variant::Sfm4300_20_O:
        case Variant::Sfm4300_20_P: return 20.0f;
        case Variant::Sfm4300_50_B:
        case Variant::Sfm4300_50_O:
        case Variant::Sfm4300_50_P: return 50.0f;
        case Variant::Sfm6000D_50:  return 50.0f;
        case Variant::Sfm6000D_20:  return 20.0f;
        case Variant::Sfm6000D_5:   return 5.0f;
        case Variant::Unknown:      return 0.0f;
    }
    return 0.0f;
}

/**
 * @brief Whether a variant carries a factory calibration for @p g.
 *
 * SFM4300-50-x: O2, Air, Air/O2 mixture only (Table 16). SFM4300-20-x: the
 * seven SF06 tables. SFM6000D-x: O2, Air, CO2, N2O, Ar and O2-in-Air.
 * Unknown variants: O2 and Air only — every other start code means a
 * different gas (or none) on the other family, so it is refused until the
 * identity is read (see `GasNeedsKnownFamily`).
 */
constexpr bool SupportsGas(Variant v, Gas g) noexcept {
    switch (v) {
        case Variant::Sfm4300_50_B:
        case Variant::Sfm4300_50_O:
        case Variant::Sfm4300_50_P:
            return g == Gas::O2 || g == Gas::Air || g == Gas::AirO2Mix;
        case Variant::Sfm4300_20_B:
        case Variant::Sfm4300_20_O:
        case Variant::Sfm4300_20_P:
            return g != Gas::Ar;
        case Variant::Sfm6000D_50:
        case Variant::Sfm6000D_20:
        case Variant::Sfm6000D_5:
            return g != Gas::N2OO2Mix && g != Gas::CO2O2Mix;
        case Variant::Unknown:
            return g == Gas::O2 || g == Gas::Air;
    }
    return false;
}

/// True when @p g's start code differs between families (needs a known variant).
constexpr bool GasNeedsKnownFamily(Gas g) noexcept {
    return g != Gas::O2 && g != Gas::Air;
}

/// Product identifier + 8-byte serial number (data sheet §4.3.9).
struct ProductInfo {
    uint32_t product_id{0};
    uint8_t serial[8]{};
    Variant variant{Variant::Unknown};
};

/// Scale factor / offset / unit for one gas lookup table (data sheet §4.3.6).
struct Scaling {
    int16_t scale{1};       ///< counts per flow unit (SFM4300: 1000 per slm)
    int16_t offset{0};      ///< counts at zero flow (SFM4300: −28672)
    uint16_t unit{0};       ///< unit code (SFM4300: 0x0148 = slm @ 20 °C)
    int16_t full_scale_raw{0};  ///< SFx6000 only: full-scale flow, raw counts
    uint16_t gas_id{0};         ///< SFx6000 only: SEMI gas ID
    bool valid{false};

    /// Full-scale flow in the device unit (SFx6000; 0 when not reported).
    constexpr float FullScaleFlow() const noexcept {
        return full_scale_raw != 0 ? ToFlow(full_scale_raw) : 0.0f;
    }

    /// raw → flow in the device unit (slm for SFM4300).
    constexpr float ToFlow(int16_t raw) const noexcept {
        return scale != 0 ? (static_cast<float>(raw) - static_cast<float>(offset)) /
                                static_cast<float>(scale)
                          : 0.0f;
    }
};

/**
 * @brief Decoded status word (data sheet §4.3.2, Table 5).
 *
 * bits 15:12 — currently running measurement command (0b0000 O2 … 0b0110 CO2/O2),
 * bit 11 — exponential smoothing active (avg-until-read idle > 64 ms),
 * bit 10 — fixed-N averaging active, bits 9:0 — gas fraction (‰) or 0x3FF for pure gas.
 */
struct StatusWord {
    uint16_t raw{0};

    constexpr uint8_t command_nibble() const noexcept { return static_cast<uint8_t>(raw >> 12); }
    constexpr bool exponential_smoothing() const noexcept { return (raw & (1U << 11)) != 0U; }
    constexpr bool fixed_n_averaging() const noexcept { return (raw & (1U << 10)) != 0U; }
    constexpr uint16_t gas_fraction_permille() const noexcept { return raw & 0x03FFU; }
    constexpr bool pure_gas() const noexcept {
        return gas_fraction_permille() == kStatusPureGasFraction;
    }
};

/// Map the status command nibble back onto a gas selector (per family).
constexpr Gas GasFromStatusNibble(Family family, uint8_t nibble) noexcept {
    if (family == Family::Sfx6000) {
        switch (nibble & 0x0FU) {
            case 0x0: return Gas::O2;
            case 0x1: return Gas::Air;
            case 0x2: return Gas::CO2;
            case 0x3: return Gas::N2O;
            case 0x4: return Gas::Ar;
            case 0xA: return Gas::AirO2Mix;
            default:  return Gas::Air;
        }
    }
    switch (nibble & 0x0FU) {
        case 0x0: return Gas::O2;
        case 0x1: return Gas::Air;
        case 0x2: return Gas::N2O;
        case 0x3: return Gas::CO2;
        case 0x4: return Gas::AirO2Mix;
        case 0x5: return Gas::N2OO2Mix;
        case 0x6: return Gas::CO2O2Mix;
        default:  return Gas::Air;
    }
}

/// One continuous-mode sample (data sheet §4.3.1 read frame).
struct Measurement {
    int16_t flow_raw{0};
    int16_t temperature_raw{0};
    StatusWord status{};
    float flow{0.0f};           ///< in device unit (slm for SFM4300)
    float temperature_c{0.0f};  ///< gas temperature [°C] (SF06 frame only)
    bool temperature_valid{true};  ///< false on SFx6000 (frame word 2 is reserved)
};

}  // namespace sfm

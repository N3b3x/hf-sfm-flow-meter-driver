/**
 * @file sfm_types.hpp
 * @brief SF06 flow-meter types: gases, variants, readings, status word, results.
 *
 * @details Scaling follows the SFM4300 data sheet §4.5: flow [slm] =
 *          (raw − offset) / scale, temperature [°C] = raw / 200. Scale and
 *          offset are read from the device per gas command (§4.3.6) rather
 *          than hard-coded, so other SF06 parts (different scale factors)
 *          work unchanged.
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

/// Calibrated gas / gas-mixture lookup table selector (data sheet Table 3).
enum class Gas : uint8_t {
    O2 = 0,      ///< Gas 0
    Air,         ///< Gas 1
    N2O,         ///< Gas 2 (SFM4300-20 only)
    CO2,         ///< Gas 3 (SFM4300-20 only)
    AirO2Mix,    ///< Gas mixture 0 — argument: O2 volume fraction (‰)
    N2OO2Mix,    ///< Gas mixture 1 — SFM4300-20 only
    CO2O2Mix,    ///< Gas mixture 2 — SFM4300-20 only
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
    }
    return "?";
}

/// Start-measurement command code for a gas selector.
constexpr uint16_t StartCommandFor(Gas g) noexcept {
    switch (g) {
        case Gas::O2:       return cmd::kStartO2;
        case Gas::Air:      return cmd::kStartAir;
        case Gas::N2O:      return cmd::kStartN2O;
        case Gas::CO2:      return cmd::kStartCO2;
        case Gas::AirO2Mix: return cmd::kStartAirO2Mix;
        case Gas::N2OO2Mix: return cmd::kStartN2OO2Mix;
        case Gas::CO2O2Mix: return cmd::kStartCO2O2Mix;
    }
    return cmd::kStartAir;
}

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
    }
    return "?";
}

/// Decode a 32-bit product identifier (low nibble = revision, masked off).
constexpr Variant VariantFromProductId(uint32_t product_id) noexcept {
    switch (product_id & 0xFFFFFFF0U) {
        case 0x04030110U: return Variant::Sfm4300_20_B;
        case 0x04030210U: return Variant::Sfm4300_20_O;
        case 0x04030310U: return Variant::Sfm4300_20_P;
        case 0x04030910U: return Variant::Sfm4300_50_B;
        case 0x04030710U: return Variant::Sfm4300_50_O;
        case 0x04030610U: return Variant::Sfm4300_50_P;
        default:          return Variant::Unknown;
    }
}

/// Full-scale flow [slm] for a known variant (0 when unknown).
constexpr float FullScaleSlm(Variant v) noexcept {
    switch (v) {
        case Variant::Sfm4300_20_B:
        case Variant::Sfm4300_20_O:
        case Variant::Sfm4300_20_P: return 20.0f;
        case Variant::Sfm4300_50_B:
        case Variant::Sfm4300_50_O:
        case Variant::Sfm4300_50_P: return 50.0f;
        case Variant::Unknown:      return 0.0f;
    }
    return 0.0f;
}

/**
 * @brief Whether a variant carries a factory calibration for @p g.
 *
 * SFM4300-50-x: O2, Air, Air/O2 mixture only (Table 16 — N2O, CO2 and their
 * mixtures are N/A). SFM4300-20-x: all seven. Unknown variants are assumed
 * to support everything so that other SF06 parts are not blocked; the device
 * itself reports an unsupported lookup table by not measuring.
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
        case Variant::Unknown:
            return true;
    }
    return true;
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
    bool valid{false};

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

/// Map the status command nibble back onto a gas selector.
constexpr Gas GasFromStatusNibble(uint8_t nibble) noexcept {
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
    float temperature_c{0.0f};  ///< gas temperature [°C]
};

}  // namespace sfm

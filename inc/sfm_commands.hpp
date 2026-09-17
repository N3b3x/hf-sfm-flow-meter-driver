/**
 * @file sfm_commands.hpp
 * @brief SF06 I2C command codes, addresses, timing and CRC-8 (SFM4300 data sheet §4).
 *
 * @details Command codes are shared across the SF06 family (SFM4300, SFM3003,
 *          SFM3013, SFM3019, SFM3119). Which *gases* a given part supports is
 *          part-specific — see `sfm_types.hpp` (`SupportsGas`). Checksum is
 *          CRC-8, polynomial 0x31, init 0xFF, no reflection, no final XOR;
 *          test vector `0xBE 0xEF → 0x92`.
 *
 * @copyright Copyright (c) 2026 HardFOC. All rights reserved.
 */
#pragma once

#include <cstddef>
#include <cstdint>

namespace sfm {

/// 7-bit I2C addresses (data sheet §4.1 / ADDR pin strapping on SFM4300).
namespace addr {
constexpr uint8_t kSfm4300Default = 0x2A;  ///< ADDR to GND or floating
constexpr uint8_t kSfm4300Alt1 = 0x2B;     ///< ADDR to GND via 1.2 kΩ
constexpr uint8_t kSfm4300Alt2 = 0x2C;     ///< ADDR to GND via 2.7 kΩ
constexpr uint8_t kSfm4300Alt3 = 0x2D;     ///< ADDR to GND via 5.6 kΩ
constexpr uint8_t kSfm3119 = 0x29;
constexpr uint8_t kSfm3003 = 0x28;
constexpr uint8_t kSfm3013 = 0x2F;
constexpr uint8_t kSfm3019 = 0x2E;
constexpr uint8_t kGeneralCall = 0x00;  ///< I2C general-call (soft reset)
}  // namespace addr

/// 16-bit command codes (data sheet §4.3, Tables 3–12).
namespace cmd {
constexpr uint16_t kStartO2 = 0x3603;
constexpr uint16_t kStartAir = 0x3608;
constexpr uint16_t kStartN2O = 0x3615;
constexpr uint16_t kStartCO2 = 0x361E;
constexpr uint16_t kStartAirO2Mix = 0x3632;   ///< argument: O2 volume fraction (‰)
constexpr uint16_t kStartN2OO2Mix = 0x3639;   ///< argument: O2 volume fraction (‰)
constexpr uint16_t kStartCO2O2Mix = 0x3646;   ///< argument: O2 volume fraction (‰)
constexpr uint16_t kUpdateConcentrationSet = 0xE17D;      ///< argument: ‰
constexpr uint16_t kUpdateConcentrationActivate = 0xE000;
constexpr uint16_t kStopContinuous = 0x3FF9;
constexpr uint16_t kConfigureAveraging = 0x366A;          ///< argument: N (0 = avg-until-read)
constexpr uint16_t kReadScaleOffsetUnit = 0x3661;         ///< argument: start command code
constexpr uint16_t kEnterSleep = 0x3677;
constexpr uint16_t kReadProductIdentifier = 0xE102;
constexpr uint8_t kGeneralCallReset = 0x06;               ///< single byte to address 0x00
}  // namespace cmd

/// Timing (data sheet §2.4 / §4.3).
namespace timing {
constexpr uint32_t kFirstResultAfterStartMs = 12;   ///< first reading valid after start
constexpr uint32_t kAccuracySettleAfterStartMs = 30; ///< few-% deviations possible before
constexpr uint32_t kSoftResetMs = 20;               ///< typ. 16 ms, max 20 ms
constexpr uint32_t kWarmUpMs = 30;                  ///< after reset / exit sleep
constexpr uint32_t kMeasurementPeriodUs = 500;      ///< internal sample period (~2 kHz)
constexpr uint16_t kMaxAveragingWindow = 128;
}  // namespace timing

/// Fixed flow-unit code returned by the SFM4300 (slm at 20 °C, 1013.25 mbar).
constexpr uint16_t kFlowUnitSlm20C = 0x0148;

/// Temperature scale (data sheet Tables 15/16: scale 200 per °C, offset 0).
constexpr float kTemperatureScalePerC = 200.0f;

/// Status-word gas fraction value for a pure calibration gas (bits 9:0).
constexpr uint16_t kStatusPureGasFraction = 0x3FF;

/**
 * @brief CRC-8 per Sensirion SF06 (poly 0x31, init 0xFF, no reflection/xorout).
 */
constexpr uint8_t Crc8(const uint8_t* data, std::size_t len) noexcept {
    uint8_t crc = 0xFFU;
    for (std::size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) {
            crc = static_cast<uint8_t>((crc & 0x80U) != 0U ? ((crc << 1) ^ 0x31U) : (crc << 1));
        }
    }
    return crc;
}

/// CRC-8 of one big-endian 16-bit word.
constexpr uint8_t Crc8Word(uint16_t word) noexcept {
    const uint8_t b[2] = {static_cast<uint8_t>(word >> 8), static_cast<uint8_t>(word & 0xFFU)};
    return Crc8(b, 2);
}

static_assert(Crc8Word(0xBEEF) == 0x92, "SF06 CRC-8 test vector (0xBEEF -> 0x92)");

}  // namespace sfm

/**
 * @file sfm_i2c_interface.hpp
 * @brief CRTP byte transport for Sensirion SF06-family flow meters (SFM4300, …).
 *
 * @details The driver only moves bytes. Sensirion SF06 devices do not use a
 *          register-address model: a transaction is either a raw write of a
 *          16-bit command (plus optional 16-bit argument + CRC) or a raw read
 *          of N words, each followed by a CRC-8 byte. The platform adapter
 *          therefore implements:
 *
 *            - `bool Write(uint8_t addr7, const uint8_t* data, std::size_t len);`
 *            - `bool Read(uint8_t addr7, uint8_t* out, std::size_t len);`
 *            - `void DelayMs(uint32_t ms);`
 *            - `bool EnsureInitialized();`
 *
 *          Optional (default returns `false` = not supported):
 *
 *            - `bool WriteGeneralCall(uint8_t byte);` — write one byte to the
 *              I2C general-call address 0x00 (used by `Driver::SoftReset`).
 *
 *          Reads return `false` on NACK. The SF06 sensors NACK a read header
 *          while no fresh measurement is available (data sheet §4.3.1); the
 *          driver reports that as `DriverError::NoData`.
 *
 * @copyright Copyright (c) 2026 HardFOC. All rights reserved.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace sfm {

/**
 * @brief CRTP base for the SF06 I2C transport.
 * @tparam Derived Concrete adapter type (e.g. `EspI2cAdapter`).
 */
template <typename Derived>
class I2cInterface {
public:
    /// Raw write of @p len bytes to 7-bit address @p addr7 (no register byte).
    bool Write(uint8_t addr7, const uint8_t* data, std::size_t len) noexcept {
        return static_cast<Derived*>(this)->Write(addr7, data, len);
    }

    /// Raw read of @p len bytes from 7-bit address @p addr7.
    bool Read(uint8_t addr7, uint8_t* out, std::size_t len) noexcept {
        return static_cast<Derived*>(this)->Read(addr7, out, len);
    }

    /// Blocking delay used for start-up (12 ms) and reset (20 ms) waits.
    void DelayMs(uint32_t ms) noexcept { static_cast<Derived*>(this)->DelayMs(ms); }

    /// Lazy bus bring-up; must return `true` before any transfer.
    bool EnsureInitialized() noexcept {
        return static_cast<Derived*>(this)->EnsureInitialized();
    }

    /// Optional I2C general-call write (address 0x00). Default: unsupported.
    bool WriteGeneralCall(uint8_t byte) noexcept {
        if constexpr (HasGeneralCall<Derived>::value) {
            return static_cast<Derived*>(this)->WriteGeneralCallImpl(byte);
        } else {
            (void)byte;
            return false;
        }
    }

private:
    template <typename, typename = void>
    struct HasGeneralCall : std::false_type {};
    template <typename T>
    struct HasGeneralCall<
        T, std::void_t<decltype(std::declval<T>().WriteGeneralCallImpl(uint8_t{}))>>
        : std::true_type {};
};

}  // namespace sfm

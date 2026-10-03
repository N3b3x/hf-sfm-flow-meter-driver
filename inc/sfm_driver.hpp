/**
 * @file sfm_driver.hpp
 * @brief Sensirion flow-meter I2C client: SF06 (SFM4300 data sheet §4) and
 *        SFx6000 (SFM6000D, SFC6xxx/SFM6xxx I2C interface reference v1.1).
 *
 * @details Start/stop continuous measurement per gas or binary mixture,
 *          single-frame read, concentration update, scale/offset/unit query,
 *          product identifier and general-call soft reset on both families;
 *          averaging and sleep on SF06; temperature through the output-buffer
 *          pointer on SFx6000. The family is learned from the product
 *          identifier (or given by the caller) and decides which start code
 *          means which gas. Every received 16-bit word is CRC-8 checked; every
 *          argument word is sent with its CRC-8.
 *
 *          Typical session:
 *          @code
 *          sfm::Driver<MyI2c> dev(i2c, sfm::addr::kSfm4300Default);
 *          auto id = dev.ReadProductIdentifier();          // idle mode only
 *          dev.StartContinuous(sfm::Gas::CO2);             // reads scaling first
 *          dev.ConfigureAveraging(0);                      // average-until-read
 *          auto m = dev.ReadMeasurement();                 // every ≥0.5 ms
 *          @endcode
 *
 * @copyright Copyright (c) 2026 HardFOC. All rights reserved.
 */
#pragma once

#include "sfm_commands.hpp"
#include "sfm_i2c_interface.hpp"
#include "sfm_types.hpp"

#include <cstddef>
#include <cstdint>

namespace sfm {

/**
 * @brief SF06 flow-meter client.
 * @tparam I2cT Concrete type inheriting `I2cInterface<I2cT>`.
 */
template <typename I2cT>
class Driver {
public:
    /**
     * @param i2c     Transport adapter (must outlive the driver).
     * @param address 7-bit device address (default SFM4300 0x2A).
     * @param family  Family assumed until the product identifier is read.
     */
    explicit Driver(I2cT& i2c, uint8_t address = addr::kSfm4300Default,
                    Family family = Family::Sf06) noexcept
        : i2c_(i2c), address_(address), family_(family) {}

    uint8_t Address() const noexcept { return address_; }
    void SetAddress(uint8_t address) noexcept { address_ = address; }

    /// Variant learned from the last successful `ReadProductIdentifier()`.
    Variant KnownVariant() const noexcept { return variant_; }
    /// Let the caller assert a variant when the identifier cannot be read (already measuring).
    void AssumeVariant(Variant v) noexcept {
        variant_ = v;
        if (v != Variant::Unknown) {
            family_ = FamilyOf(v);
        }
    }

    /// Family in use (from the identifier, `AssumeVariant`, or the constructor).
    Family PartFamily() const noexcept { return family_; }

    /// Gas selected by the last successful `StartContinuous()`.
    Gas ActiveGas() const noexcept { return active_gas_; }
    bool Measuring() const noexcept { return measuring_; }
    /// Scaling captured for the active gas.
    const Scaling& ActiveScaling() const noexcept { return scaling_; }

    // ------------------------------------------------------------------ identity

    /**
     * @brief Read product identifier and serial number (§4.3.9).
     * @note Idle mode only — call before `StartContinuous()` or after `Stop()`.
     */
    DriverResult<ProductInfo> ReadProductIdentifier() noexcept {
        if (!i2c_.EnsureInitialized()) {
            return DriverResult<ProductInfo>::failure(DriverError::NotInitialized);
        }
        if (!WriteCommand(cmd::kReadProductIdentifier)) {
            return DriverResult<ProductInfo>::failure(DriverError::BusWrite);
        }
        uint16_t words[6]{};
        const DriverError err = ReadWords(words, 6);
        if (err != DriverError::None) {
            return DriverResult<ProductInfo>::failure(err);
        }
        ProductInfo info{};
        info.product_id = (static_cast<uint32_t>(words[0]) << 16) | words[1];
        for (std::size_t i = 0; i < 4; ++i) {
            info.serial[2 * i] = static_cast<uint8_t>(words[2 + i] >> 8);
            info.serial[2 * i + 1] = static_cast<uint8_t>(words[2 + i] & 0xFFU);
        }
        info.variant = VariantFromProductId(info.product_id);
        variant_ = info.variant;
        if (variant_ != Variant::Unknown) {
            family_ = FamilyOf(variant_);
        }
        return DriverResult<ProductInfo>::success(info);
    }

    // ------------------------------------------------------------- measurement

    /**
     * @brief Query scale factor, offset and unit for a gas table (§4.3.6).
     * @param g Gas selector whose start command is passed as the argument.
     */
    DriverResult<Scaling> ReadScaleOffsetUnit(Gas g) noexcept {
        if (!i2c_.EnsureInitialized()) {
            return DriverResult<Scaling>::failure(DriverError::NotInitialized);
        }
        const uint16_t start = StartCommandFor(family_, g);
        if (start == 0U) {
            return DriverResult<Scaling>::failure(DriverError::UnsupportedGas);
        }
        if (!WriteCommandWithArg(cmd::kReadScaleOffsetUnit, start)) {
            return DriverResult<Scaling>::failure(DriverError::BusWrite);
        }
        /* SFx6000: the parameters sit behind an output-buffer pointer and
         * include full-scale flow and the SEMI gas ID (reference §3.3.12). */
        const bool sfx = (family_ == Family::Sfx6000);
        if (sfx && !WriteCommand(cmd::kSfx6000PointerGasInfo)) {
            return DriverResult<Scaling>::failure(DriverError::BusWrite);
        }
        uint16_t words[5]{};
        const DriverError err = ReadWords(words, sfx ? 5 : 3);
        if (err != DriverError::None) {
            return DriverResult<Scaling>::failure(err);
        }
        Scaling s{};
        s.scale = static_cast<int16_t>(words[0]);
        s.offset = static_cast<int16_t>(words[1]);
        s.unit = words[2];
        if (sfx) {
            s.full_scale_raw = static_cast<int16_t>(words[3]);
            s.gas_id = words[4];
        }
        s.valid = (s.scale != 0);
        return DriverResult<Scaling>::success(s);
    }

    /**
     * @brief Start continuous measurement for @p g (§4.3.1).
     *
     * Reads the gas table scaling first (so `ReadMeasurement()` can convert),
     * refuses gases the known variant does not calibrate, then issues the start
     * command and waits `timing::kFirstResultAfterStartMs`.
     *
     * @param g            Gas or mixture.
     * @param o2_permille  O2 volume fraction for mixtures (ignored for pure gases).
     */
    DriverResult<void> StartContinuous(Gas g, uint16_t o2_permille = 0) noexcept {
        if (!i2c_.EnsureInitialized()) {
            return DriverResult<void>::failure(DriverError::NotInitialized);
        }
        if (variant_ == Variant::Unknown && GasNeedsKnownFamily(g)) {
            /* The start code for this gas means another gas on the other
             * family: read the identifier (or AssumeVariant) first. */
            return DriverResult<void>::failure(DriverError::VariantUnknown);
        }
        if (!SupportsGas(variant_, g)) {
            return DriverResult<void>::failure(DriverError::UnsupportedGas);
        }
        if (IsMixture(g) && o2_permille > 1000U) {
            return DriverResult<void>::failure(DriverError::InvalidParameter);
        }
        const auto sc = ReadScaleOffsetUnit(g);
        if (!sc.ok()) {
            return DriverResult<void>::failure(sc.error);
        }
        const uint16_t start = StartCommandFor(family_, g);
        const bool ok = IsMixture(g) ? WriteCommandWithArg(start, o2_permille)
                                     : WriteCommand(start);
        if (!ok) {
            return DriverResult<void>::failure(DriverError::BusWrite);
        }
        scaling_ = sc.value;
        active_gas_ = g;
        measuring_ = true;
        i2c_.DelayMs(timing::kFirstResultAfterStartMs);
        return DriverResult<void>::success();
    }

    /// Stop continuous measurement → idle (§4.3.4).
    DriverResult<void> Stop() noexcept {
        if (!i2c_.EnsureInitialized()) {
            return DriverResult<void>::failure(DriverError::NotInitialized);
        }
        if (!WriteCommand(cmd::kStopContinuous)) {
            return DriverResult<void>::failure(DriverError::BusWrite);
        }
        measuring_ = false;
        return DriverResult<void>::success();
    }

    /**
     * @brief Read one flow / temperature / status frame (§4.3.1).
     *
     * Requires a running continuous measurement. A NACK on the read header
     * (no fresh sample yet, or idle) is reported as `DriverError::NoData`.
     */
    DriverResult<Measurement> ReadMeasurement() noexcept {
        uint8_t rx[9]{};
        if (!i2c_.Read(address_, rx, sizeof(rx))) {
            return DriverResult<Measurement>::failure(DriverError::NoData);
        }
        return DecodeMeasurement(family_, scaling_, rx);
    }

    /**
     * @brief Decode one 9-byte continuous-mode frame (3 × word + CRC).
     * @details Pure: the same decode serves the blocking read above and an
     *          interrupt / DMA reader that fetched the frame itself.
     *          SF06: flow, temperature, status. SFx6000: flow, reserved,
     *          status (temperature through `ReadTemperature()`).
     */
    static DriverResult<Measurement> DecodeMeasurement(Family family, const Scaling& scaling,
                                                       const uint8_t frame[9]) noexcept {
        uint16_t words[3]{};
        const DriverError err = DecodeWords(frame, 3, words);
        if (err != DriverError::None) {
            return DriverResult<Measurement>::failure(err);
        }
        Measurement m{};
        m.flow_raw = static_cast<int16_t>(words[0]);
        m.status.raw = words[2];
        m.flow = scaling.valid ? scaling.ToFlow(m.flow_raw) : 0.0f;
        if (family == Family::Sfx6000) {
            /* Word 2 is reserved on SFx6000; temperature has its own read. */
            m.temperature_valid = false;
        } else {
            m.temperature_raw = static_cast<int16_t>(words[1]);
            m.temperature_c = static_cast<float>(m.temperature_raw) / kTemperatureScalePerC;
        }
        return DriverResult<Measurement>::success(m);
    }

    /**
     * @brief SFx6000: read the flow-chip temperature while measuring
     *        (pointer 0xE102 → one word → pointer back to results 0xE000).
     * @return `NotSupported` on SF06 (temperature is in every frame).
     */
    DriverResult<float> ReadTemperature() noexcept {
        if (family_ != Family::Sfx6000) {
            return DriverResult<float>::failure(DriverError::NotSupported);
        }
        if (!WriteCommand(cmd::kSfx6000PointerTemperature)) {
            return DriverResult<float>::failure(DriverError::BusWrite);
        }
        uint16_t word[1]{};
        const DriverError err = ReadWords(word, 1);
        const bool back = WriteCommand(cmd::kSfx6000PointerResult);
        if (err != DriverError::None) {
            return DriverResult<float>::failure(err);
        }
        if (!back) {
            return DriverResult<float>::failure(DriverError::BusWrite);
        }
        return DriverResult<float>::success(static_cast<float>(static_cast<int16_t>(word[0])) /
                                            kTemperatureScalePerC);
    }

    /// Flow-only read (2 words + CRC) for the tightest loop; status not refreshed.
    DriverResult<float> ReadFlowOnly() noexcept {
        uint16_t words[1]{};
        const DriverError err = ReadWords(words, 1);
        if (err != DriverError::None) {
            return DriverResult<float>::failure(err);
        }
        return DriverResult<float>::success(
            scaling_.valid ? scaling_.ToFlow(static_cast<int16_t>(words[0])) : 0.0f);
    }

    /**
     * @brief Configure on-sensor averaging (§4.3.5).
     * @param window 0 = average-until-read (default), 1…128 = fixed-N.
     */
    DriverResult<void> ConfigureAveraging(uint16_t window) noexcept {
        if (family_ == Family::Sfx6000) {
            return DriverResult<void>::failure(DriverError::NotSupported);
        }
        if (window > timing::kMaxAveragingWindow) {
            return DriverResult<void>::failure(DriverError::InvalidParameter);
        }
        if (!i2c_.EnsureInitialized()) {
            return DriverResult<void>::failure(DriverError::NotInitialized);
        }
        return WriteCommandWithArg(cmd::kConfigureAveraging, window)
                   ? DriverResult<void>::success()
                   : DriverResult<void>::failure(DriverError::BusWrite);
    }

    /**
     * @brief Update the O2 fraction of a running mixture measurement (§4.3.3).
     * @param o2_permille New O2 volume fraction (‰).
     */
    DriverResult<void> UpdateConcentration(uint16_t o2_permille) noexcept {
        if (o2_permille > 1000U) {
            return DriverResult<void>::failure(DriverError::InvalidParameter);
        }
        if (!i2c_.EnsureInitialized()) {
            return DriverResult<void>::failure(DriverError::NotInitialized);
        }
        if (!WriteCommandWithArg(cmd::kUpdateConcentrationSet, o2_permille)) {
            return DriverResult<void>::failure(DriverError::BusWrite);
        }
        if (!WriteCommand(cmd::kUpdateConcentrationActivate)) {
            return DriverResult<void>::failure(DriverError::BusWrite);
        }
        return DriverResult<void>::success();
    }

    // -------------------------------------------------------------- power / reset

    /// Enter sleep (idle mode only, §4.3.8).
    DriverResult<void> EnterSleep() noexcept {
        if (family_ == Family::Sfx6000) {
            return DriverResult<void>::failure(DriverError::NotSupported);
        }
        if (!i2c_.EnsureInitialized()) {
            return DriverResult<void>::failure(DriverError::NotInitialized);
        }
        return WriteCommand(cmd::kEnterSleep) ? DriverResult<void>::success()
                                              : DriverResult<void>::failure(DriverError::BusWrite);
    }

    /**
     * @brief Exit sleep: any write header wakes the part; it does not ACK (§4.3.8).
     * Polls up to ~20 ms until the address is acknowledged again.
     */
    DriverResult<void> ExitSleep() noexcept {
        if (family_ == Family::Sfx6000) {
            return DriverResult<void>::failure(DriverError::NotSupported);
        }
        if (!i2c_.EnsureInitialized()) {
            return DriverResult<void>::failure(DriverError::NotInitialized);
        }
        const uint8_t dummy = 0x00;
        (void)i2c_.Write(address_, &dummy, 1);
        for (int i = 0; i < 4; ++i) {
            i2c_.DelayMs(5);
            if (i2c_.Write(address_, &dummy, 0)) {
                return DriverResult<void>::success();
            }
        }
        return DriverResult<void>::failure(DriverError::BusWrite);
    }

    /**
     * @brief I2C general-call soft reset (§4.3.7): 0x06 to address 0x00, then ~20 ms.
     * @return `NotSupported` when the transport has no general-call write.
     */
    DriverResult<void> SoftReset() noexcept {
        if (!i2c_.EnsureInitialized()) {
            return DriverResult<void>::failure(DriverError::NotInitialized);
        }
        if (!i2c_.WriteGeneralCall(cmd::kGeneralCallReset)) {
            return DriverResult<void>::failure(DriverError::NotSupported);
        }
        measuring_ = false;
        scaling_ = Scaling{};
        i2c_.DelayMs(family_ == Family::Sfx6000 ? timing::kSfx6000SoftResetMs
                                                : timing::kSoftResetMs);
        return DriverResult<void>::success();
    }

    // ------------------------------------------------------------------ framing

    /// Encode a 16-bit word + CRC-8 into 3 bytes.
    static void EncodeWord(uint16_t word, uint8_t out[3]) noexcept {
        out[0] = static_cast<uint8_t>(word >> 8);
        out[1] = static_cast<uint8_t>(word & 0xFFU);
        out[2] = Crc8(out, 2);
    }

    /// Decode `count` (word, crc) triplets; returns `Crc` on the first mismatch.
    static DriverError DecodeWords(const uint8_t* in, std::size_t count, uint16_t* out) noexcept {
        for (std::size_t i = 0; i < count; ++i) {
            const uint8_t* p = in + 3 * i;
            if (Crc8(p, 2) != p[2]) {
                return DriverError::Crc;
            }
            out[i] = static_cast<uint16_t>((static_cast<uint16_t>(p[0]) << 8) | p[1]);
        }
        return DriverError::None;
    }

private:
    bool WriteCommand(uint16_t command) noexcept {
        const uint8_t tx[2] = {static_cast<uint8_t>(command >> 8),
                               static_cast<uint8_t>(command & 0xFFU)};
        return i2c_.Write(address_, tx, sizeof(tx));
    }

    bool WriteCommandWithArg(uint16_t command, uint16_t arg) noexcept {
        uint8_t tx[5];
        tx[0] = static_cast<uint8_t>(command >> 8);
        tx[1] = static_cast<uint8_t>(command & 0xFFU);
        EncodeWord(arg, &tx[2]);
        return i2c_.Write(address_, tx, sizeof(tx));
    }

    DriverError ReadWords(uint16_t* out, std::size_t count) noexcept {
        if (count == 0 || count > 6) {
            return DriverError::InvalidParameter;
        }
        uint8_t rx[18]{};
        if (!i2c_.Read(address_, rx, 3 * count)) {
            return DriverError::NoData;
        }
        return DecodeWords(rx, count, out);
    }

    I2cT& i2c_;
    uint8_t address_;
    Family family_{Family::Sf06};
    Variant variant_{Variant::Unknown};
    Gas active_gas_{Gas::Air};
    Scaling scaling_{};
    bool measuring_{false};
};

}  // namespace sfm

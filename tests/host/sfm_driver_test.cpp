// =============================================================================
// HF-SFM host tests — CRC-8, framing, scaling, gas gating, status decode.
// Fake I2C adapter records writes and replays scripted read frames.
// =============================================================================
#include "sfm.hpp"

#include <cstdio>
#include <cstring>
#include <vector>

namespace {

int g_failures = 0;

#define CHECK(cond)                                                                    \
    do {                                                                               \
        if (!(cond)) {                                                                 \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                \
            ++g_failures;                                                              \
        }                                                                              \
    } while (0)

class FakeI2c : public sfm::I2cInterface<FakeI2c> {
public:
    struct Write_ {
        uint8_t addr;
        std::vector<uint8_t> bytes;
    };
    std::vector<Write_> writes;
    std::vector<std::vector<uint8_t>> read_queue;
    std::vector<uint8_t> general_calls;
    bool nack_reads{false};
    uint32_t delayed_ms{0};

    bool Write(uint8_t addr7, const uint8_t* data, std::size_t len) noexcept {
        writes.push_back({addr7, std::vector<uint8_t>(data, data + len)});
        return true;
    }
    bool Read(uint8_t addr7, uint8_t* out, std::size_t len) noexcept {
        (void)addr7;
        if (nack_reads || read_queue.empty()) {
            return false;
        }
        const auto& f = read_queue.front();
        if (f.size() < len) {
            return false;
        }
        std::memcpy(out, f.data(), len);
        read_queue.erase(read_queue.begin());
        return true;
    }
    void DelayMs(uint32_t ms) noexcept { delayed_ms += ms; }
    bool EnsureInitialized() noexcept { return true; }
    bool WriteGeneralCallImpl(uint8_t b) noexcept {
        general_calls.push_back(b);
        return true;
    }

    void QueueWords(std::initializer_list<uint16_t> words) {
        std::vector<uint8_t> f;
        for (uint16_t w : words) {
            uint8_t t[3];
            sfm::Driver<FakeI2c>::EncodeWord(w, t);
            f.insert(f.end(), t, t + 3);
        }
        read_queue.push_back(f);
    }
};

void TestCrc() {
    CHECK(sfm::Crc8Word(0xBEEF) == 0x92);
    const uint8_t zeros[2] = {0, 0};
    CHECK(sfm::Crc8(zeros, 2) == 0x81);  // known SF06 value for 0x0000
}

void TestProductIdentifier() {
    FakeI2c i2c;
    sfm::Driver<FakeI2c> dev(i2c);
    // SFM4300-20-P, revision 3, serial 0x0102030405060708
    i2c.QueueWords({0x0403, 0x0313, 0x0102, 0x0304, 0x0506, 0x0708});
    const auto r = dev.ReadProductIdentifier();
    CHECK(r.ok());
    CHECK(r.value.product_id == 0x04030313U);
    CHECK(r.value.variant == sfm::Variant::Sfm4300_20_P);
    CHECK(r.value.serial[0] == 0x01 && r.value.serial[7] == 0x08);
    CHECK(dev.KnownVariant() == sfm::Variant::Sfm4300_20_P);
    CHECK(i2c.writes.size() == 1 && i2c.writes[0].bytes.size() == 2);
    CHECK(i2c.writes[0].bytes[0] == 0xE1 && i2c.writes[0].bytes[1] == 0x02);
    CHECK(sfm::FullScaleSlm(r.value.variant) == 20.0f);
    CHECK(sfm::VariantFromProductId(0x04030615U) == sfm::Variant::Sfm4300_50_P);
    // Revision byte ignored: an SFM4300-20-P on the 2026-10-02 bench read 0x04030301.
    CHECK(sfm::VariantFromProductId(0x04030301U) == sfm::Variant::Sfm4300_20_P);
    CHECK(sfm::VariantFromProductId(0x04030110U) == sfm::Variant::Sfm4300_20_B);
    CHECK(sfm::VariantFromProductId(0x04030401U) == sfm::Variant::Unknown);
}

void TestStartAndMeasure() {
    FakeI2c i2c;
    sfm::Driver<FakeI2c> dev(i2c);
    dev.AssumeVariant(sfm::Variant::Sfm4300_20_P);
    // scale 1000, offset -28672 (0x9000), unit 0x0148
    i2c.QueueWords({1000, static_cast<uint16_t>(-28672), sfm::kFlowUnitSlm20C});
    const auto s = dev.StartContinuous(sfm::Gas::CO2);
    CHECK(s.ok());
    CHECK(dev.Measuring());
    CHECK(dev.ActiveGas() == sfm::Gas::CO2);
    CHECK(dev.ActiveScaling().valid);
    CHECK(i2c.delayed_ms == sfm::timing::kFirstResultAfterStartMs);
    // writes: [0x3661 + arg 0x361E + crc], [0x361E]
    CHECK(i2c.writes.size() == 2);
    CHECK(i2c.writes[0].bytes.size() == 5);
    CHECK(i2c.writes[0].bytes[0] == 0x36 && i2c.writes[0].bytes[1] == 0x61);
    CHECK(i2c.writes[0].bytes[2] == 0x36 && i2c.writes[0].bytes[3] == 0x1E);
    CHECK(i2c.writes[0].bytes[4] == sfm::Crc8Word(0x361E));
    CHECK(i2c.writes[1].bytes.size() == 2 && i2c.writes[1].bytes[1] == 0x1E);

    // 2.500 slm → raw = 2.5*1000 - 28672 = -26172 ; 23.5 °C → 4700 ; status CO2 pure
    const int16_t raw_flow = static_cast<int16_t>(2500 - 28672);
    const uint16_t status = static_cast<uint16_t>((0x3U << 12) | sfm::kStatusPureGasFraction);
    i2c.QueueWords({static_cast<uint16_t>(raw_flow), 4700, status});
    const auto m = dev.ReadMeasurement();
    CHECK(m.ok());
    CHECK(m.value.flow > 2.4999f && m.value.flow < 2.5001f);
    CHECK(m.value.temperature_c > 23.49f && m.value.temperature_c < 23.51f);
    CHECK(m.value.status.pure_gas());
    CHECK(!m.value.status.fixed_n_averaging());
    CHECK(sfm::GasFromStatusNibble(sfm::Family::Sf06, m.value.status.command_nibble()) ==
          sfm::Gas::CO2);

    // NACK while no data
    i2c.nack_reads = true;
    const auto n = dev.ReadMeasurement();
    CHECK(!n.ok() && n.error == sfm::DriverError::NoData);
    i2c.nack_reads = false;

    // Corrupt CRC
    i2c.QueueWords({0x1234, 0x0000, 0x0000});
    i2c.read_queue.back()[2] ^= 0xFF;
    const auto c = dev.ReadMeasurement();
    CHECK(!c.ok() && c.error == sfm::DriverError::Crc);
}

void TestGasGating() {
    FakeI2c i2c;
    sfm::Driver<FakeI2c> dev(i2c);
    dev.AssumeVariant(sfm::Variant::Sfm4300_50_P);
    const auto r = dev.StartContinuous(sfm::Gas::CO2);
    CHECK(!r.ok() && r.error == sfm::DriverError::UnsupportedGas);
    CHECK(i2c.writes.empty());
    CHECK(sfm::SupportsGas(sfm::Variant::Sfm4300_50_P, sfm::Gas::Air));
    CHECK(sfm::SupportsGas(sfm::Variant::Sfm4300_50_P, sfm::Gas::AirO2Mix));
    CHECK(!sfm::SupportsGas(sfm::Variant::Sfm4300_50_P, sfm::Gas::N2O));
    CHECK(sfm::SupportsGas(sfm::Variant::Sfm4300_20_B, sfm::Gas::CO2O2Mix));

    // Mixture start carries the ‰ argument
    i2c.QueueWords({1000, static_cast<uint16_t>(-28672), sfm::kFlowUnitSlm20C});
    const auto mix = dev.StartContinuous(sfm::Gas::AirO2Mix, 400);
    CHECK(mix.ok());
    CHECK(i2c.writes.size() == 2 && i2c.writes[1].bytes.size() == 5);
    CHECK(i2c.writes[1].bytes[2] == 0x01 && i2c.writes[1].bytes[3] == 0x90);
    const auto bad = dev.UpdateConcentration(1001);
    CHECK(!bad.ok() && bad.error == sfm::DriverError::InvalidParameter);
}

void TestAveragingStopReset() {
    FakeI2c i2c;
    sfm::Driver<FakeI2c> dev(i2c);
    CHECK(dev.ConfigureAveraging(4).ok());
    CHECK(i2c.writes.back().bytes[0] == 0x36 && i2c.writes.back().bytes[1] == 0x6A);
    CHECK(i2c.writes.back().bytes[3] == 0x04);
    CHECK(!dev.ConfigureAveraging(129).ok());
    CHECK(dev.Stop().ok());
    CHECK(i2c.writes.back().bytes[0] == 0x3F && i2c.writes.back().bytes[1] == 0xF9);
    CHECK(dev.SoftReset().ok());
    CHECK(i2c.general_calls.size() == 1 && i2c.general_calls[0] == 0x06);
    CHECK(i2c.delayed_ms >= sfm::timing::kSoftResetMs);
}

// --- SFx6000 (SFM6000D) ------------------------------------------------------

void TestSfx6000Identity() {
    FakeI2c i2c;
    sfm::Driver<FakeI2c> dev(i2c, sfm::addr::kSfm6000Default);
    CHECK(dev.PartFamily() == sfm::Family::Sf06);  // constructor default until identified
    // SFM6000D-50, revision 0x84
    i2c.QueueWords({0x0602, 0x1184, 0x0000, 0x0000, 0x2341, 0x0001});
    const auto r = dev.ReadProductIdentifier();
    CHECK(r.ok());
    CHECK(r.value.variant == sfm::Variant::Sfm6000D_50);
    CHECK(dev.PartFamily() == sfm::Family::Sfx6000);
    CHECK(sfm::FullScaleSlm(sfm::Variant::Sfm6000D_20) == 20.0f);
    CHECK(sfm::VariantFromProductId(0x06021484U) == sfm::Variant::Sfm6000D_5);
    CHECK(sfm::VariantFromProductId(0x06020184U) == sfm::Variant::Unknown);  // SFC controller
    CHECK(i2c.writes[0].addr == 0x24);
}

/* The hazard this layer exists for: 0x3615 / 0x361E mean opposite gases. */
void TestSfx6000GasMapAndScaling() {
    CHECK(sfm::StartCommandFor(sfm::Family::Sf06, sfm::Gas::CO2) == 0x361E);
    CHECK(sfm::StartCommandFor(sfm::Family::Sfx6000, sfm::Gas::CO2) == 0x3615);
    CHECK(sfm::StartCommandFor(sfm::Family::Sf06, sfm::Gas::N2O) == 0x3615);
    CHECK(sfm::StartCommandFor(sfm::Family::Sfx6000, sfm::Gas::N2O) == 0x361E);
    CHECK(sfm::StartCommandFor(sfm::Family::Sfx6000, sfm::Gas::Ar) == 0x3624);
    CHECK(sfm::StartCommandFor(sfm::Family::Sf06, sfm::Gas::Ar) == 0);
    CHECK(sfm::StartCommandFor(sfm::Family::Sfx6000, sfm::Gas::CO2O2Mix) == 0);

    FakeI2c i2c;
    sfm::Driver<FakeI2c> dev(i2c, sfm::addr::kSfm6000Default);
    dev.AssumeVariant(sfm::Variant::Sfm6000D_50);
    // CO2 table on the 50 slm part: scale 2560, offset -28672, slm, FS 20 slm, gas id 0x0019
    const int16_t fs_raw = static_cast<int16_t>(20 * 2560 - 28672);
    i2c.QueueWords({2560, static_cast<uint16_t>(-28672), sfm::kFlowUnitSlm20C,
                    static_cast<uint16_t>(fs_raw), 0x0019});
    const auto s = dev.StartContinuous(sfm::Gas::CO2);
    CHECK(s.ok());
    // writes: [0x3661 + 0x3615 + crc], [0xE151 pointer], [0x3615 start]
    CHECK(i2c.writes.size() == 3);
    CHECK(i2c.writes[0].bytes[0] == 0x36 && i2c.writes[0].bytes[1] == 0x61);
    CHECK(i2c.writes[0].bytes[2] == 0x36 && i2c.writes[0].bytes[3] == 0x15);
    CHECK(i2c.writes[1].bytes.size() == 2 && i2c.writes[1].bytes[0] == 0xE1 &&
          i2c.writes[1].bytes[1] == 0x51);
    CHECK(i2c.writes[2].bytes[0] == 0x36 && i2c.writes[2].bytes[1] == 0x15);
    CHECK(dev.ActiveScaling().gas_id == 0x0019);
    const float fs = dev.ActiveScaling().FullScaleFlow();
    CHECK(fs > 19.999f && fs < 20.001f);

    // 5 slm CO2: raw = 5*2560 - 28672; word 2 reserved; status nibble 0b0010 = CO2
    const int16_t raw = static_cast<int16_t>(5 * 2560 - 28672);
    const uint16_t status = static_cast<uint16_t>((0x2U << 12) | sfm::kStatusPureGasFraction);
    i2c.QueueWords({static_cast<uint16_t>(raw), 0xBEEF, status});
    const auto m = dev.ReadMeasurement();
    CHECK(m.ok());
    CHECK(m.value.flow > 4.999f && m.value.flow < 5.001f);
    CHECK(!m.value.temperature_valid);
    CHECK(sfm::GasFromStatusNibble(sfm::Family::Sfx6000, m.value.status.command_nibble()) ==
          sfm::Gas::CO2);
    CHECK(sfm::GasFromStatusNibble(sfm::Family::Sf06, m.value.status.command_nibble()) ==
          sfm::Gas::N2O);  // what an SF06 decoder would wrongly have said

    // Temperature: pointer 0xE102, one word, pointer back 0xE000
    const std::size_t w0 = i2c.writes.size();
    i2c.QueueWords({static_cast<uint16_t>(30 * 200)});
    const auto t = dev.ReadTemperature();
    CHECK(t.ok() && t.value > 29.99f && t.value < 30.01f);
    CHECK(i2c.writes.size() == w0 + 2);
    CHECK(i2c.writes[w0].bytes[0] == 0xE1 && i2c.writes[w0].bytes[1] == 0x02);
    CHECK(i2c.writes[w0 + 1].bytes[0] == 0xE0 && i2c.writes[w0 + 1].bytes[1] == 0x00);
}

void TestSfx6000Gating() {
    FakeI2c i2c;
    sfm::Driver<FakeI2c> dev(i2c, sfm::addr::kSfm6000Default);
    // Identity unknown: CO2 refused (its code is N2O on the other family).
    const auto r = dev.StartContinuous(sfm::Gas::CO2);
    CHECK(!r.ok() && r.error == sfm::DriverError::VariantUnknown);
    CHECK(i2c.writes.empty());
    CHECK(sfm::SupportsGas(sfm::Variant::Unknown, sfm::Gas::Air));
    CHECK(!sfm::SupportsGas(sfm::Variant::Unknown, sfm::Gas::AirO2Mix));
    CHECK(sfm::SupportsGas(sfm::Variant::Sfm6000D_50, sfm::Gas::Ar));
    CHECK(!sfm::SupportsGas(sfm::Variant::Sfm6000D_50, sfm::Gas::CO2O2Mix));
    CHECK(!sfm::SupportsGas(sfm::Variant::Sfm4300_20_P, sfm::Gas::Ar));

    dev.AssumeVariant(sfm::Variant::Sfm6000D_50);
    const auto avg = dev.ConfigureAveraging(4);
    CHECK(!avg.ok() && avg.error == sfm::DriverError::NotSupported);
    CHECK(!dev.EnterSleep().ok());
    CHECK(dev.SoftReset().ok());
    CHECK(i2c.delayed_ms >= sfm::timing::kSfx6000SoftResetMs);
    // SF06 has no separate temperature read.
    FakeI2c i2c2;
    sfm::Driver<FakeI2c> sf06(i2c2);
    const auto t = sf06.ReadTemperature();
    CHECK(!t.ok() && t.error == sfm::DriverError::NotSupported);
}

}  // namespace

int main() {
    TestSfx6000Identity();
    TestSfx6000GasMapAndScaling();
    TestSfx6000Gating();
    TestCrc();
    TestProductIdentifier();
    TestStartAndMeasure();
    TestGasGating();
    TestAveragingStopReset();
    if (g_failures == 0) {
        std::printf("hf-sfm host tests: all passed (%s)\n", sfm::GetDriverVersion());
        return 0;
    }
    std::printf("hf-sfm host tests: %d failure(s)\n", g_failures);
    return 1;
}

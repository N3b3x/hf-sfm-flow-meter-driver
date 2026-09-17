// =============================================================================
// HF-SFM — flow demo: fixed-N averaging + 500 Hz polling, running statistics.
// =============================================================================
// I2C0 SDA=GPIO8 SCL=GPIO9 @400 kHz, address 0x2A. Selects CO2 when the part
// carries that table (SFM4300-20-x), otherwise Air. Averaging N=4 → one fresh
// sample every 2 ms, matched to the 500 Hz poll. Prints mean/min/max each second.
// =============================================================================

#include "hf_sfm_esp_i2c.hpp"
#include "sfm.hpp"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <cfloat>

namespace {

constexpr const char* TAG = "SfmDemo";
using I2c = hf_sfm_examples::SfmEspIdfI2c<I2C_NUM_0, 8, 9>;

}  // namespace

extern "C" void app_main(void) {
    ESP_ERROR_CHECK(I2c::Install());
    I2c i2c;
    sfm::Driver<I2c> dev(i2c);

    (void)dev.SoftReset();
    const auto id = dev.ReadProductIdentifier();
    const sfm::Variant variant = id.ok() ? id.value.variant : sfm::Variant::Unknown;
    const sfm::Gas gas = sfm::SupportsGas(variant, sfm::Gas::CO2) && variant != sfm::Variant::Unknown
                             ? sfm::Gas::CO2
                             : sfm::Gas::Air;
    ESP_LOGI(TAG, "variant %s → gas table %s", sfm::ToString(variant).data(), sfm::ToString(gas).data());

    if (!dev.StartContinuous(gas).ok()) {
        ESP_LOGE(TAG, "start failed");
        return;
    }
    (void)dev.ConfigureAveraging(4);

    float sum = 0.0f, mn = FLT_MAX, mx = -FLT_MAX;
    int n = 0, nodata = 0, crc = 0;
    TickType_t last_print = xTaskGetTickCount();

    while (true) {
        const auto m = dev.ReadMeasurement();
        if (m.ok()) {
            sum += m.value.flow;
            mn = m.value.flow < mn ? m.value.flow : mn;
            mx = m.value.flow > mx ? m.value.flow : mx;
            ++n;
        } else if (m.error == sfm::DriverError::NoData) {
            ++nodata;
        } else if (m.error == sfm::DriverError::Crc) {
            ++crc;
        }
        if (xTaskGetTickCount() - last_print >= pdMS_TO_TICKS(1000)) {
            if (n > 0) {
                ESP_LOGI(TAG, "n=%d mean=%.3f min=%.3f max=%.3f slm  nodata=%d crc=%d", n, sum / n, mn, mx,
                         nodata, crc);
            }
            sum = 0.0f; mn = FLT_MAX; mx = -FLT_MAX; n = 0; nodata = 0; crc = 0;
            last_print = xTaskGetTickCount();
        }
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}

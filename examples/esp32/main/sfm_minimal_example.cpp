// =============================================================================
// HF-SFM — minimal ESP-IDF example (SFM4300 data sheet §4 I2C)
// =============================================================================
// Default wiring (ESP32-S3): I2C0, SDA=GPIO8, SCL=GPIO9, 400 kHz, address 0x2A.
// Reads the product identifier (idle), starts the Air table, then prints one
// flow / temperature / status frame per second.
// =============================================================================

#include "hf_sfm_esp_i2c.hpp"
#include "sfm.hpp"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {

constexpr const char* TAG = "SfmMin";
using I2c = hf_sfm_examples::SfmEspIdfI2c<I2C_NUM_0, 8, 9>;

}  // namespace

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "HF-SFM minimal (I2C0 SDA=8 SCL=9 @400k, addr 0x%02X, driver %s)",
             sfm::addr::kSfm4300Default, sfm::GetDriverVersion());

    ESP_ERROR_CHECK(I2c::Install());

    I2c i2c;
    sfm::Driver<I2c> dev(i2c, sfm::addr::kSfm4300Default);

    (void)dev.SoftReset();

    const auto id = dev.ReadProductIdentifier();
    if (id.ok()) {
        ESP_LOGI(TAG, "product 0x%08lX (%s) serial %02X%02X%02X%02X%02X%02X%02X%02X",
                 static_cast<unsigned long>(id.value.product_id), sfm::ToString(id.value.variant).data(),
                 id.value.serial[0], id.value.serial[1], id.value.serial[2], id.value.serial[3],
                 id.value.serial[4], id.value.serial[5], id.value.serial[6], id.value.serial[7]);
    } else {
        ESP_LOGW(TAG, "product identifier failed: %s", sfm::ToString(id.error).data());
    }

    const auto st = dev.StartContinuous(sfm::Gas::Air);
    if (!st.ok()) {
        ESP_LOGE(TAG, "start failed: %s", sfm::ToString(st.error).data());
        return;
    }
    ESP_LOGI(TAG, "scaling: scale=%d offset=%d unit=0x%04X", dev.ActiveScaling().scale,
             dev.ActiveScaling().offset, dev.ActiveScaling().unit);

    while (true) {
        const auto m = dev.ReadMeasurement();
        if (!m.ok()) {
            ESP_LOGW(TAG, "read failed: %s", sfm::ToString(m.error).data());
        } else {
            ESP_LOGI(TAG, "flow=%.3f slm  T=%.2f C  status=0x%04X (%s%s)", m.value.flow,
                     m.value.temperature_c, m.value.status.raw,
                     sfm::ToString(sfm::GasFromStatusNibble(m.value.status.command_nibble())).data(),
                     m.value.status.fixed_n_averaging() ? ", fixed-N" : "");
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

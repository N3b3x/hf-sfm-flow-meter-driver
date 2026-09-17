/**
 * @file hf_sfm_esp_i2c.hpp
 * @brief Reusable ESP-IDF I2C transport for `sfm::Driver` (CRTP, zero virtual calls).
 */
#pragma once

#include "sfm.hpp"

#include "driver/i2c.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <cstddef>
#include <cstdint>

namespace hf_sfm_examples {

/**
 * @brief Template I2C bridge: one concrete type per (port, SDA, SCL, speed) tuple.
 *
 * Call `Install()` once before constructing `sfm::Driver<SfmEspIdfI2c<...>>`.
 * SF06 parts support 400 kHz (default here) up to 1 MHz Fm+. Pull-ups required.
 */
template <i2c_port_t kPort, int kSdaGpio, int kSclGpio, std::uint32_t kSpeedHz = 400000U,
          std::uint32_t kTimeoutMs = 20U>
class SfmEspIdfI2c
    : public sfm::I2cInterface<SfmEspIdfI2c<kPort, kSdaGpio, kSclGpio, kSpeedHz, kTimeoutMs>> {
public:
    static esp_err_t Install() noexcept {
        i2c_config_t cfg = {};
        cfg.mode = I2C_MODE_MASTER;
        cfg.sda_io_num = kSdaGpio;
        cfg.scl_io_num = kSclGpio;
        cfg.sda_pullup_en = GPIO_PULLUP_ENABLE;
        cfg.scl_pullup_en = GPIO_PULLUP_ENABLE;
        cfg.master.clk_speed = kSpeedHz;
        esp_err_t err = i2c_param_config(kPort, &cfg);
        if (err != ESP_OK) {
            return err;
        }
        err = i2c_driver_install(kPort, I2C_MODE_MASTER, 0, 0, 0);
        installed_ = (err == ESP_OK);
        return err;
    }

    bool Write(std::uint8_t addr7, const std::uint8_t* data, std::size_t len) noexcept {
        if (len == 0) {
            // Address-only probe (used by ExitSleep): START, addr+W, STOP.
            i2c_cmd_handle_t cmd = i2c_cmd_link_create();
            i2c_master_start(cmd);
            i2c_master_write_byte(cmd, static_cast<std::uint8_t>((addr7 << 1) | I2C_MASTER_WRITE), true);
            i2c_master_stop(cmd);
            const esp_err_t err = i2c_master_cmd_begin(kPort, cmd, pdMS_TO_TICKS(kTimeoutMs));
            i2c_cmd_link_delete(cmd);
            return err == ESP_OK;
        }
        return i2c_master_write_to_device(kPort, addr7, data, len, pdMS_TO_TICKS(kTimeoutMs)) == ESP_OK;
    }

    bool Read(std::uint8_t addr7, std::uint8_t* out, std::size_t len) noexcept {
        return i2c_master_read_from_device(kPort, addr7, out, len, pdMS_TO_TICKS(kTimeoutMs)) == ESP_OK;
    }

    void DelayMs(std::uint32_t ms) noexcept { vTaskDelay(pdMS_TO_TICKS(ms)); }

    bool EnsureInitialized() noexcept { return installed_; }

    /// General-call reset (0x06 to address 0x00) — enables `Driver::SoftReset()`.
    bool WriteGeneralCallImpl(std::uint8_t byte) noexcept {
        return i2c_master_write_to_device(kPort, sfm::addr::kGeneralCall, &byte, 1,
                                          pdMS_TO_TICKS(kTimeoutMs)) == ESP_OK;
    }

private:
    static inline bool installed_ = false;
};

}  // namespace hf_sfm_examples

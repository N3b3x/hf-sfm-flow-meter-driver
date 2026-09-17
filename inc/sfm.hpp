/**
 * @file sfm.hpp
 * @brief Umbrella header for the HF-SFM (Sensirion SF06-family) flow-meter driver.
 *
 * @details Pulls in `sfm::I2cInterface`, command codes and CRC-8, typed readings
 *          (`Measurement`, `Scaling`, `ProductInfo`, `StatusWord`), and
 *          `sfm::Driver<I2cT>` implementing the SFM4300 data sheet §4 command
 *          set. See `examples/esp32/` for an ESP-IDF I2C adapter.
 *
 * @copyright Copyright (c) 2026 HardFOC. All rights reserved.
 */
#pragma once

#include "sfm_commands.hpp"
#include "sfm_driver.hpp"
#include "sfm_i2c_interface.hpp"
#include "sfm_types.hpp"
#include "sfm_version.h"

namespace sfm {

/** @brief Driver version string (from generated @ref sfm_version.h). */
inline const char* GetDriverVersion() noexcept {
    return HF_SFM_VERSION_STRING;
}

}  // namespace sfm

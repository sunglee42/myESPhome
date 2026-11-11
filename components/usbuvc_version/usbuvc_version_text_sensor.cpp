// SPDX-FileCopyrightText: 2026 youkorr
// SPDX-License-Identifier: GPL-3.0-or-later
#include "esphome/core/defines.h"

#ifdef USE_TEXT_SENSOR

#include "usbuvc_version_text_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace usbuvc_version {

static const char *const TAG = "usbuvc_version.text_sensor";

void VisionTextSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "Vision detection text sensor:");
  ESP_LOGCONFIG(TAG, "  Last value: %s",
                this->last_published_.empty() ? "<no detections yet>"
                                              : this->last_published_.c_str());
}

}  // namespace usbuvc_version
}  // namespace esphome

#endif  // USE_TEXT_SENSOR

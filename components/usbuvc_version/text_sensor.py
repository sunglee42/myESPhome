# SPDX-FileCopyrightText: 2026 youkorr
# SPDX-License-Identifier: MIT
"""
text_sensor sub-platform for the vision component.

YAML usage:

  text_sensor:
    - platform: usbuvc_version
      detection:
        id: my_detection
        name: "current_detection"

The "detection" key publishes a string of the form:

    person:87,car:62,dog:55

(label:score%, comma-separated, max_detections entries).

It updates after every successful inference pass, no need to call any
action manually.
"""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import CONF_ID, CONF_NAME

from . import VisionComponent, usbuvc_version_ns

CONF_USBUVC_VERSION_ID = "usbuvc_version_id"
CONF_DETECTION = "detection"

VisionTextSensor = usbuvc_version_ns.class_(
    "VisionTextSensor", text_sensor.TextSensor, cg.Component
)

DETECTION_SCHEMA = text_sensor.text_sensor_schema(VisionTextSensor).extend(
    {
        cv.GenerateID(): cv.declare_id(VisionTextSensor),
    }
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_USBUVC_VERSION_ID): cv.use_id(VisionComponent),
        cv.Optional(CONF_DETECTION): DETECTION_SCHEMA,
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_USBUVC_VERSION_ID])

    if CONF_DETECTION in config:
        sub = config[CONF_DETECTION]
        var = await text_sensor.new_text_sensor(sub)
        await cg.register_component(var, sub)
        cg.add(parent.add_listener(var))

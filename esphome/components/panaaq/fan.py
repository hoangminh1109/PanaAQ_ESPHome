# Copyright 2025 Minh Hoang
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from esphome import pins
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import fan, i2c, text_sensor, button
from esphome.const import (
    CONF_OUTPUT_ID,
    CONF_ID,
    CONF_NAME,
    CONF_DISABLED_BY_DEFAULT
)

DEPENDENCIES = ["i2c"]
AUTO_LOAD = ["fan", "i2c", "text_sensor", "button"]

panaaq_ns = cg.esphome_ns.namespace('panaaq')
PanaAirPurifier = panaaq_ns.class_(
    "PanaAirPurifier",
    cg.Component,
    fan.Fan,
    i2c.I2CDevice,
    )
PanaAirNanoe = panaaq_ns.class_(
    'PanaAirNanoe',
    text_sensor.TextSensor,
    cg.Component
    )
PanaAirFilterCheck = panaaq_ns.class_(
    'PanaAirFilterCheck',
    text_sensor.TextSensor,
    cg.Component
    )
PanaAirQuality = panaaq_ns.class_(
    'PanaAirQuality',
    text_sensor.TextSensor,
    cg.Component
    )

CONF_AIRQUALITY_ID = "airquality_id"
CONF_NANOE_ID = "nanoe_id"
CONF_FILTERCHECK_ID = "filtercheck_id"
CONF_INTERVAL_MS = "interval"
CONF_PIN_LED0 = "pin_led0"
CONF_PIN_LED1 = "pin_led1"
CONF_PIN_LED23 = "pin_led23"

CONF_PINIDX_LED0 = 0
CONF_PINIDX_LED1 = 1
CONF_PINIDX_LED23 = 2

CONF_DEFAULT_PIN_LED0 = 14
CONF_DEFAULT_PIN_LED1 = 12
CONF_DEFAULT_PIN_LED23 = 13

CONFIG_SCHEMA = (fan.fan_schema(PanaAirPurifier).extend({
            cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(PanaAirPurifier),
            cv.GenerateID(CONF_AIRQUALITY_ID): cv.declare_id(PanaAirQuality),
            cv.GenerateID(CONF_NANOE_ID): cv.declare_id(PanaAirNanoe),
            cv.GenerateID(CONF_FILTERCHECK_ID): cv.declare_id(PanaAirFilterCheck),
            cv.Optional(CONF_INTERVAL_MS, default=1): cv.int_range(min=1),
            cv.Optional(CONF_PIN_LED0, default=CONF_DEFAULT_PIN_LED0): cv.All(pins.internal_gpio_input_pin_schema),
            cv.Optional(CONF_PIN_LED1, default=CONF_DEFAULT_PIN_LED1): cv.All(pins.internal_gpio_input_pin_schema),
            cv.Optional(CONF_PIN_LED23, default=CONF_DEFAULT_PIN_LED23): cv.All(pins.internal_gpio_input_pin_schema),
    })
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x20))
    )


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await cg.register_component(var, config)
    await fan.register_fan(var, config)
    await i2c.register_i2c_device(var, config)

    # pins
    pin_led0 = await cg.gpio_pin_expression(config[CONF_PIN_LED0])
    cg.add(var.set_pin_idx(CONF_PINIDX_LED0, pin_led0))

    pin_led1 = await cg.gpio_pin_expression(config[CONF_PIN_LED1])
    cg.add(var.set_pin_idx(CONF_PINIDX_LED1, pin_led1))

    pin_led23 = await cg.gpio_pin_expression(config[CONF_PIN_LED23])
    cg.add(var.set_pin_idx(CONF_PINIDX_LED23, pin_led23))

    # nanoe text_sensor
    nanoe_default_config = { CONF_ID: config[CONF_NANOE_ID],
                                CONF_NAME: "nanoe",
                                CONF_DISABLED_BY_DEFAULT: False}
    nanoe = cg.new_Pvariable(config[CONF_NANOE_ID])
    await text_sensor.register_text_sensor(nanoe, nanoe_default_config)
    await cg.register_component(nanoe, nanoe_default_config)
    cg.add(nanoe.set_parent_fan(var))
    cg.add(var.set_fan_nanoe(nanoe))

    # Filter check text_sensor
    filtercheck_default_config = { CONF_ID: config[CONF_FILTERCHECK_ID],
                                CONF_NAME: "Filter Check",
                                CONF_DISABLED_BY_DEFAULT: False}
    filtercheck = cg.new_Pvariable(config[CONF_FILTERCHECK_ID])
    await text_sensor.register_text_sensor(filtercheck, filtercheck_default_config)
    await cg.register_component(filtercheck, filtercheck_default_config)
    cg.add(filtercheck.set_parent_fan(var))
    cg.add(var.set_fan_filtercheck(filtercheck))

    # Air quality text_sensor
    airquality_default_config = { CONF_ID: config[CONF_AIRQUALITY_ID],
                                CONF_NAME: "Air Quality",
                                CONF_DISABLED_BY_DEFAULT: False}
    airquality = cg.new_Pvariable(config[CONF_AIRQUALITY_ID])
    await text_sensor.register_text_sensor(airquality, airquality_default_config)
    await cg.register_component(airquality, airquality_default_config)
    cg.add(airquality.set_parent_fan(var))
    cg.add(var.set_fan_airquality(airquality))

    # interval
    cg.add(var.set_interval_ms(config[CONF_INTERVAL_MS]))
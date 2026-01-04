/*
* Copyright 2025 Hoang Minh
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#pragma once

#include <deque>
#include <map>

#include "esphome/core/component.h"
#include "esphome/components/fan/fan.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/button/button.h"
#include "esphome/core/gpio.h"

namespace esphome {
namespace panaaq {

static const char *const TAG = "panaaq.fan";

enum FanSpeed {
  PANAAQ_OFF = 0,
  PANAAQ_LOW = 1,
  PANAAQ_MEDIUM = 2,
  PANAAQ_HIGH = 3,
  PANAAQ_INVALID = 100
};

enum FanMode {
  PANAAQ_MODE_OFF = 0,
  PANAAQ_MODE_MANUAL = 1,
  PANAAQ_MODE_AUTO = 2,
  PANAAQ_MODE_TURBO = 3,
  PANAAQ_MODE_SLEEP = 4,
  PANAAQ_MODE_INVALID = 100,
};

struct FanState {
  FanSpeed speed;
  FanMode mode;
  bool nanoe;
  bool filtercheck;
  uint8_t airquality;
};

#define STR_FANMODE_OFF     "---"
#define STR_FANMODE_MANUAL  "👆 Manual"
#define STR_FANMODE_AUTO  "🔁 Auto"
#define STR_FANMODE_TURBO  "🌀 Turbo"
#define STR_FANMODE_SLEEP   "😴 Sleep"

static std::map<uint8_t, std::string> FANMODE_STR = {
  {PANAAQ_MODE_OFF, STR_FANMODE_OFF},
  {PANAAQ_MODE_MANUAL, STR_FANMODE_MANUAL},
  {PANAAQ_MODE_AUTO, STR_FANMODE_AUTO},
  {PANAAQ_MODE_TURBO, STR_FANMODE_TURBO},
  {PANAAQ_MODE_SLEEP, STR_FANMODE_SLEEP}
};

static std::map<std::string, uint8_t> FANMODE_ID = {
  {STR_FANMODE_OFF, PANAAQ_MODE_OFF},
  {STR_FANMODE_MANUAL, PANAAQ_MODE_MANUAL},
  {STR_FANMODE_AUTO, PANAAQ_MODE_AUTO},
  {STR_FANMODE_TURBO, PANAAQ_MODE_TURBO},
  {STR_FANMODE_SLEEP, PANAAQ_MODE_SLEEP}
};

#define STR_FAN_SENSOR_OFF "Off"
#define STR_FAN_SENSOR_ON  "On"

#define STR_FAN_AIRQUALITY_NA_STR "---"
#define STR_FAN_AIRQUALITY_L0_STR "😊 Clean"
#define STR_FAN_AIRQUALITY_L1_STR "😐 Moderate"
#define STR_FAN_AIRQUALITY_L2_STR "😷 Polluted"

// Array using the defines
static const char *STR_FAN_AIRQUALITY_STR[] = {
    STR_FAN_AIRQUALITY_NA_STR,
    STR_FAN_AIRQUALITY_L0_STR,
    STR_FAN_AIRQUALITY_L1_STR,
    STR_FAN_AIRQUALITY_L2_STR,
};

#define FAN_PANAAQ_AIRQUALITY_INVALID 0
#define FAN_PANAAQ_AIRQUALITY_L0 1
#define FAN_PANAAQ_AIRQUALITY_L1 2
#define FAN_PANAAQ_AIRQUALITY_L2 3

// led bit position
#define PANAAQ_FAN_LED0        0
#define PANAAQ_FAN_LED1        1 
#define PANAAQ_FAN_LED23       2
#define PANAAQ_FAN_LED_COUNT   3

#define PANAAQ_BUTTON_ONOFF      0
#define PANAAQ_BUTTON_AIRVOLUME  1
#define PANAAQ_BUTTON_TURBO      2
#define PANAAQ_BUTTON_SLEEP      3

class PanaAirPurifier;

class PanaAirNanoe : public text_sensor::TextSensor, public Component
{
public:
    void setup() override;
    void dump_config() override;
    void set_parent_fan(PanaAirPurifier *fan) { this->fan_ = fan; }
    void set_fan_nanoe(bool nanoe);

private:
    PanaAirPurifier *fan_{nullptr};
};

class PanaAirFilterCheck : public text_sensor::TextSensor, public Component
{
public:
    void setup() override;
    void dump_config() override;
    void set_parent_fan(PanaAirPurifier *fan) { this->fan_ = fan; }
    void set_fan_filtercheck(bool filtercheck);

private:
    PanaAirPurifier *fan_{nullptr};
};

class PanaAirQuality : public text_sensor::TextSensor, public Component
{
public:
    void setup() override;
    void dump_config() override;
    void set_parent_fan(PanaAirPurifier *fan) { this->fan_ = fan; }
    void set_fan_airquality(uint8_t airquality);

private:
    PanaAirPurifier *fan_{nullptr};
};

class PanaAirPurifier : public Component,
                public fan::Fan,
                public i2c::I2CDevice
{
public:
  PanaAirPurifier() {}
  void setup() override;
  void dump_config() override;
  void loop() override;
  void set_interval_ms(int interval_ms) { this->interval_ms_ = interval_ms; }
  void set_fan_nanoe(PanaAirNanoe *fan_nanoe) { this->fan_nanoe_ = fan_nanoe; }
  void set_fan_filtercheck(PanaAirFilterCheck *fan_filtercheck) { this->fan_filtercheck_ = fan_filtercheck; }
  void set_fan_airquality(PanaAirQuality *fan_airquality) { this->fan_airquality_ = fan_airquality; }
  void set_pin_idx(uint8_t pin_idx, InternalGPIOPin *pin) { pins_[pin_idx] = pin; }
  fan::FanTraits get_traits() override { return this->traits_; }
  std::deque<uint8_t> button_queue{};
  void process_command();
  bool processing{false};
  void set_fan_mode(FanMode mode);

protected:
  void control(const fan::FanCall &call) override;

private:
  fan::FanTraits traits_;
  uint32_t last_run_{0};
  uint8_t led_on_counts[5][2] = {};
  uint8_t sample_step_state{0};
  uint8_t sample_step_aq{0};
  uint16_t aq_led_on_counts[3] = {};
  FanState fan_state_;

  int interval_ms_{0};

  PanaAirNanoe *fan_nanoe_{nullptr};
  PanaAirFilterCheck *fan_filtercheck_{nullptr};
  PanaAirQuality *fan_airquality_{nullptr};

  InternalGPIOPin *pins_[PANAAQ_FAN_LED_COUNT];

  void write_gpio(uint16_t value);
  void update_state();
};

}  // namespace panaaq
}  // namespace esphome

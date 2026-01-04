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

#include "panaaq.h"
#include "esphome/core/log.h"

#include <bitset>

namespace esphome {
namespace panaaq {

void PanaAirPurifier::setup()
{
  last_run_ = millis();

  auto restore = this->restore_state_();
  if (restore.has_value())
  {
    restore->apply(*this);
  }

  for (int i = 0; i < PANAAQ_FAN_LED_COUNT; i++)
  {
    this->pins_[i]->setup();
  }

  // Construct traits
  fan::FanTraits fan_traits(false, true, false, 3);
  fan_traits.set_supported_preset_modes({STR_FANMODE_AUTO, STR_FANMODE_TURBO, STR_FANMODE_SLEEP});
  this->traits_ = fan_traits;

  this->fan_state_.speed = PANAAQ_OFF;
  this->fan_state_.mode = PANAAQ_MODE_INVALID;
  this->fan_state_.nanoe = false;
  this->fan_state_.filtercheck = false;
  this->fan_state_.airquality = FAN_PANAAQ_AIRQUALITY_INVALID;

  this->fan_nanoe_->publish_state(STR_FAN_SENSOR_OFF);
  this->fan_filtercheck_->publish_state(STR_FAN_SENSOR_OFF);
  this->fan_airquality_->publish_state(STR_FAN_AIRQUALITY_NA_STR);
}

void PanaAirPurifier::dump_config()
{
  LOG_FAN(TAG, "Pana Air Purifier", this);
}

void PanaAirPurifier::set_fan_mode(FanMode mode)
{
  if (mode == PANAAQ_MODE_OFF || mode == PANAAQ_MODE_MANUAL)
  {
    this->clear_preset_mode_();
  }
  else
  {
    this->set_preset_mode_(FANMODE_STR[static_cast<uint8_t>(mode)]);
  }
}

void PanaAirPurifier::update_state()
{
  uint32_t now = millis();
  if (now - this->last_run_ >= this->interval_ms_)
  {
    this->last_run_ = now;

    // interval task
    uint8_t data[2];
    bool success = this->read_bytes_raw(data, 2);
    if (!success)
    {
      ESP_LOGW(TAG, "I2C read error!");
      return;
    }

    uint16_t led_status_i2c = ((uint16_t)data[0] | ((uint16_t)data[1] << 8)) & 0x07FF;

    bool led0 = this->pins_[PANAAQ_FAN_LED0]->digital_read();
    bool led1 = this->pins_[PANAAQ_FAN_LED1]->digital_read();
    bool led23 = this->pins_[PANAAQ_FAN_LED23]->digital_read();

    int led_scan_row[5] = {    (led_status_i2c >> 10) & 0x01, 
                               (led_status_i2c >> 4) & 0x01,
                               (led_status_i2c >> 5) & 0x01,
                               (led_status_i2c >> 6) & 0x01,
                               (led_status_i2c >> 7) & 0x01 };
    int led_scan_col[2] = {    (led_status_i2c >> 8) & 0x01,
                               (led_status_i2c >> 9) & 0x01 };

    for (uint8_t r = 0; r < 5; r++)
    {
      if (led_scan_row[r] == 0)
      {
        for (uint8_t c = 0; c < 2; c++)
        {
          if (led_scan_col[c] == 1)
            this->led_on_counts[r][c]++;
        }
      }
    }

    if (led0) aq_led_on_counts[0]++;
    if (led1) aq_led_on_counts[1]++;
    if (led23) aq_led_on_counts[2]++;

    sample_step_state++;
    if (sample_step_state >= 20)
    {
      bool led_state[5][2] = {};
      sample_step_state = 0;

      for (uint8_t r = 0; r < 5; r++)
      {
        for (uint8_t c = 0; c < 2; c++)
        {
          led_state[r][c] = (this->led_on_counts[r][c] > 2);
          this->led_on_counts[r][c] = 0;
        }
      }

      // fan speed
      FanSpeed speed = PANAAQ_OFF;
      if (led_state[2][1]) speed = PANAAQ_HIGH;
      if (led_state[2][0]) speed = PANAAQ_MEDIUM;
      if (led_state[4][1]) speed = PANAAQ_LOW;

      // fan mode
      FanMode mode = PANAAQ_MODE_OFF;
      if (led_state[1][0]) mode = PANAAQ_MODE_AUTO;
      if (led_state[1][1]) mode = PANAAQ_MODE_TURBO;
      if (led_state[3][0]) mode = PANAAQ_MODE_SLEEP;
      if (speed != PANAAQ_OFF) { mode = PANAAQ_MODE_MANUAL; }

      // nanoe
      bool nanoe = led_state[0][0];

      // filter check
      bool filtercheck = led_state[3][1];

      bool state_change = false;

      // fan mode
      if (this->fan_state_.mode != mode)
      {
        this->fan_state_.mode = mode;

        if (this->fan_state_.mode == PANAAQ_MODE_OFF)
        {
          this->state = false;
        }
        else
        {
          if (this->fan_state_.mode != PANAAQ_MODE_OFF && !this->state)
          {
            this->state = true;
          }

          if (this->fan_state_.mode != PANAAQ_MODE_MANUAL && this->fan_state_.mode != PANAAQ_MODE_AUTO)
          {
            this->speed = static_cast<int>(PANAAQ_OFF);
          }
          else
          {
            this->speed = static_cast<int>(this->fan_state_.speed);
          }
        }
        this->set_fan_mode(this->fan_state_.mode);
        state_change = true;
      }

      if (this->fan_state_.mode == PANAAQ_MODE_MANUAL || this->fan_state_.mode == PANAAQ_MODE_AUTO)
      {
        // fan speed
        if (this->fan_state_.speed != speed)
        {
          this->fan_state_.speed = speed;
          this->speed = static_cast<int>(this->fan_state_.speed);
          state_change = true;
        }
      }

      if (this->fan_state_.nanoe != nanoe)
      {
        this->fan_state_.nanoe = nanoe;
        this->fan_nanoe_->set_fan_nanoe(this->fan_state_.nanoe);
      }

      if (this->fan_state_.filtercheck != filtercheck)
      {
        this->fan_state_.filtercheck = filtercheck;
        this->fan_filtercheck_->set_fan_filtercheck(this->fan_state_.filtercheck);
      }

      if (state_change)
      {
        this->publish_state();
      }
    }

    // air quality
    sample_step_aq++;
    if (sample_step_aq >= 100)
    {
      sample_step_aq = 0;
      bool clean = false, moderate = false, polluted = false;

      if (aq_led_on_counts[0] >= 90) clean = true;
      if (aq_led_on_counts[1] >= 90) moderate = true;
      if (aq_led_on_counts[2] >= 90) polluted = true;

      aq_led_on_counts[0] = 0;
      aq_led_on_counts[1] = 0;
      aq_led_on_counts[2] = 0;

      uint8_t aq = FAN_PANAAQ_AIRQUALITY_INVALID;
      if (clean) aq = FAN_PANAAQ_AIRQUALITY_L0;
      if (moderate) aq = FAN_PANAAQ_AIRQUALITY_L1;
      if (polluted) aq = FAN_PANAAQ_AIRQUALITY_L2;

      if (this->fan_state_.mode != PANAAQ_MODE_MANUAL && this->fan_state_.mode != PANAAQ_MODE_AUTO)
      {
        aq = FAN_PANAAQ_AIRQUALITY_INVALID;
      }
      if (this->fan_state_.airquality != aq)
      {
        this->fan_state_.airquality = aq;
        this->fan_airquality_->set_fan_airquality(this->fan_state_.airquality);
      }
    }        
  }
}

void PanaAirPurifier::loop()
{
  // skip if it is processing a command
  if (this->processing)
  {
    return;
  }

  this->update_state();
}

void PanaAirPurifier::write_gpio(uint16_t value)
{
  uint8_t data[2];
  data[0] = ~value;
  data[1] = ~value >> 8;
  if (this->write(data, 2) != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "I2C write error!");
  }
}

void PanaAirPurifier::process_command()
{
  if (this->button_queue.size() == 0)
  {
    this->update_state();
    this->processing = false;
    return;
  }

  uint8_t button_pressed = this->button_queue.front();
  this->write_gpio( 1 << button_pressed );
  this->set_timeout("button_off", 
                    100,
                    [this]() {
                      this->write_gpio(0);
                      this->button_queue.pop_front();
                      if (this->button_queue.size() > 0)
                      {
                        this->set_timeout(
                          "button_on",
                          100,
                          [this]() {
                            this->process_command();
                          });
                      }
                      else
                      {
                        this->process_command();
                      }
                    });
}

void PanaAirPurifier::control(const fan::FanCall &call)
{
  // skip if it is processing a command
  if (this->processing)
  {
    return;
  }

  if (call.get_state().has_value())
  {
    bool newstate = *call.get_state();
    if (this->state != newstate)
    {
      this->button_queue.push_back(PANAAQ_BUTTON_ONOFF);
    }
  }

  if (call.get_speed().has_value())
  {
    uint8_t currspeed;

    if (!this->has_preset_mode()) // from off
    {
      currspeed = static_cast<int>(this->fan_state_.speed);
    }
    else // on
    {
      if (this->get_preset_mode() == std::string(STR_FANMODE_AUTO))
      {
        currspeed = 0;
      }
      else if (this->get_preset_mode() == std::string(STR_FANMODE_MANUAL))
      {
        currspeed = this->speed;
      }
      else // turbo / sleep
      {
        this->button_queue.push_back(PANAAQ_BUTTON_AIRVOLUME); // need to change to normal mode first
        currspeed = static_cast<int>(this->fan_state_.speed);
      }
    }

    uint8_t newspeed = *call.get_speed();
    newspeed = newspeed < currspeed ? newspeed + 4 : newspeed;
    this->button_queue.insert(this->button_queue.end(), newspeed-currspeed, PANAAQ_BUTTON_AIRVOLUME);
  }

  if (call.has_preset_mode())
  {
    if (!this->state) // from off, only allow change preset mode to AUTO
    {
      if (call.get_preset_mode() == std::string(STR_FANMODE_AUTO))
      {
        this->button_queue.push_back(PANAAQ_BUTTON_ONOFF); // need to push on/off once to turn on.
        uint8_t currspeed = static_cast<uint8_t>(this->fan_state_.speed);
        this->button_queue.insert(this->button_queue.end(), (4 - currspeed)%4, PANAAQ_BUTTON_AIRVOLUME);
      }
    }
    else
    {
      if (call.get_preset_mode() != this->get_preset_mode())
      {
        if (call.get_preset_mode() == std::string(STR_FANMODE_AUTO))
        {
          if (this->fan_state_.mode != PANAAQ_MODE_MANUAL)
          {
            this->button_queue.push_back(PANAAQ_BUTTON_AIRVOLUME); // need to push air volume once to change to manual mode.
          }
          uint8_t currspeed = static_cast<uint8_t>(this->fan_state_.speed);
          this->button_queue.insert(this->button_queue.end(), (4 - currspeed)%4, PANAAQ_BUTTON_AIRVOLUME);
        }
          
        if (call.get_preset_mode() == std::string(STR_FANMODE_TURBO))
        {
          this->button_queue.push_back(PANAAQ_BUTTON_TURBO);
        }
          
        if (call.get_preset_mode() == std::string(STR_FANMODE_SLEEP))
        {
          this->button_queue.push_back(PANAAQ_BUTTON_SLEEP);
        }
      }
    }

  }      

  if (this->button_queue.size() > 0)
  {
    // start processing
    this->processing = true;
    this->process_command();
  }

}

void PanaAirNanoe::dump_config()
{
    ESP_LOGCONFIG(TAG, "PanaAirNanoe:");
    LOG_TEXT_SENSOR("  Fan Nanoe: ", "fan_nanoe", this);
}

void PanaAirNanoe::setup()
{
  this->set_icon("mdi:timer-outline");
}

void PanaAirNanoe::set_fan_nanoe(bool nanoe)
{
  this->publish_state(nanoe ? STR_FAN_SENSOR_ON : STR_FAN_SENSOR_OFF);
}

void PanaAirFilterCheck::dump_config()
{
    ESP_LOGCONFIG(TAG, "PanaAirFilterCheck:");
    LOG_TEXT_SENSOR("  Fan Filter Check: ", "fan_filtercheck", this);
}

void PanaAirFilterCheck::setup()
{
  this->set_icon("mdi:timer-outline");
}

void PanaAirFilterCheck::set_fan_filtercheck(bool filtercheck)
{
  this->publish_state(filtercheck ? STR_FAN_SENSOR_ON : STR_FAN_SENSOR_OFF);
}

void PanaAirQuality::dump_config()
{
    ESP_LOGCONFIG(TAG, "PanaAirQuality:");
    LOG_TEXT_SENSOR("  Fan Air Quality: ", "fan_airquality", this);
}

void PanaAirQuality::setup()
{
  this->set_icon("mdi:timer-outline");
}

void PanaAirQuality::set_fan_airquality(uint8_t airquality)
{
  this->publish_state(STR_FAN_AIRQUALITY_STR[airquality]);
}


}  // namespace panaaq
}  // namespace esphome

#pragma once

#include <zephyr/device.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "ble_service.hpp"

// PWM period (1kHz = 1ms = 1000000ns for good LED control without flicker)
#define PWM_PERIOD_NS 1000000U

class LEDController
{
public:
  LEDController(
    const struct pwm_dt_spec * r_spec, const struct pwm_dt_spec * g_spec,
    const struct pwm_dt_spec * b_spec, const struct pwm_dt_spec * w_spec)
  : r_pwm_(r_spec), g_pwm_(g_spec), b_pwm_(b_spec), w_pwm_(w_spec), current_color_{0, 0, 0, 0}
  {
  }

  bool init()
  {
    if (
      !check_ready(r_pwm_, "Red PWM") || !check_ready(g_pwm_, "Green PWM") ||
      !check_ready(b_pwm_, "Blue PWM") || !check_ready(w_pwm_, "White PWM")) {
      return false;
    }
    return true;
  }

  void set_color(const rgbw_color_t & color)
  {
    current_color_ = color;
    apply_color();
  }

  void set_color(uint8_t r, uint8_t g, uint8_t b, uint8_t w)
  {
    current_color_ = {r, g, b, w};
    apply_color();
  }

  rgbw_color_t get_color() const { return current_color_; }

  void turn_off() { set_color(0, 0, 0, 0); }

private:
  const struct pwm_dt_spec * r_pwm_;
  const struct pwm_dt_spec * g_pwm_;
  const struct pwm_dt_spec * b_pwm_;
  const struct pwm_dt_spec * w_pwm_;

  rgbw_color_t current_color_;

  bool check_ready(const struct pwm_dt_spec * spec, const char * name)
  {
    if (!spec) {
      printk("Error: %s spec is null\n", name);
      return false;
    }
    if (!pwm_is_ready_dt(spec)) {
      printk("Error: %s device is not ready\n", name);
      return false;
    }
    return true;
  }

  void apply_color()
  {
    set_pwm_channel(r_pwm_, current_color_.r);
    set_pwm_channel(g_pwm_, current_color_.g);
    set_pwm_channel(b_pwm_, current_color_.b);
    set_pwm_channel(w_pwm_, current_color_.w);
  }

  void set_pwm_channel(const struct pwm_dt_spec * spec, uint8_t value)
  {
    if (!spec) return;

    // Convert 0-255 value to PWM pulse width (0-PWM_PERIOD_NS)
    uint32_t pulse_ns = (static_cast<uint32_t>(value) * PWM_PERIOD_NS) / 256;

    int ret = pwm_set_dt(spec, PWM_PERIOD_NS, pulse_ns);
    if (ret < 0) {
      printk("Error: PWM set failed with %d\n", ret);
    }
  }
};

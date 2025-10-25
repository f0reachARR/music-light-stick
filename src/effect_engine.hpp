#pragma once

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "color_utils.hpp"
#include "effect_mode.hpp"

// Effect engine that runs different animation effects
class EffectEngine
{
public:
  EffectEngine() : current_effect_(), running_(false), phase_(0), direction_(1) {}

  void set_effect(const Effect & effect)
  {
    current_effect_ = effect;
    phase_ = 0;
    direction_ = 1;
  }

  const Effect & get_effect() const { return current_effect_; }

  void start() { running_ = true; }

  void stop() { running_ = false; }

  bool is_running() const { return running_; }

  // Update effect and return current color
  // This should be called periodically (e.g., every 20-50ms)
  rgbw_color_t update()
  {
    if (!running_) {
      return {0, 0, 0, 0};
    }

    switch (current_effect_.mode) {
      case EffectMode::FIXED_COLOR:
        return update_fixed_color();
      case EffectMode::RAINBOW:
        return update_rainbow();
      case EffectMode::GRADIENT:
        return update_gradient();
      case EffectMode::BLINK:
        return update_blink();
      default:
        return {0, 0, 0, 0};
    }
  }

private:
  Effect current_effect_;
  bool running_;
  uint32_t phase_;      // General-purpose phase counter
  int8_t direction_;    // Direction for ping-pong effects (1 or -1)
  uint64_t last_tick_;  // For blink timing

  rgbw_color_t update_fixed_color()
  {
    return current_effect_.data.fixed_color.color;
  }

  rgbw_color_t update_rainbow()
  {
    const auto & rainbow = current_effect_.data.rainbow;

    // Calculate hue based on phase
    // Speed affects how fast we cycle through hues
    // Speed 1 = slowest, 255 = fastest
    uint16_t hue = (phase_ * rainbow.speed / 10) % 360;

    // Convert HSV to RGB with full saturation
    rgbw_color_t color = hsv_to_rgb(hue, 255, rainbow.brightness);

    phase_++;
    return color;
  }

  rgbw_color_t update_gradient()
  {
    const auto & gradient = current_effect_.data.gradient;

    // Calculate interpolation factor (0-255)
    // Speed affects how fast we transition
    uint8_t t = (phase_ * gradient.speed / 10) % 512;

    // Ping-pong between color1 and color2
    if (t > 255) {
      t = 511 - t;  // 256-511 maps to 255-0
    }

    rgbw_color_t color = lerp_color(gradient.color1, gradient.color2, t);

    phase_++;
    return color;
  }

  rgbw_color_t update_blink()
  {
    const auto & blink = current_effect_.data.blink;

    // Period is in 100ms units
    // We need to track time in milliseconds
    uint64_t now = k_uptime_get();
    uint32_t period_ms = blink.period * 100;
    uint32_t half_period_ms = period_ms / 2;

    // Calculate position in cycle
    uint32_t cycle_pos = now % period_ms;

    if (cycle_pos < half_period_ms) {
      // ON phase
      return blink.color;
    } else {
      // OFF phase
      return {0, 0, 0, 0};
    }
  }
};

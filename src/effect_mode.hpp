#pragma once

#include <zephyr/kernel.h>

#include <cstdint>
#include <cstring>

// Forward declaration to avoid circular dependency
struct rgbw_color_t;

#ifndef RGBW_COLOR_T_DEFINED
#define RGBW_COLOR_T_DEFINED
struct rgbw_color_t
{
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t w;
} __packed;
#endif

// Effect mode definitions as per spec v2.0
enum class EffectMode : uint8_t
{
  FIXED_COLOR = 0x00,  // Fixed RGBW color
  RAINBOW = 0x01,      // Cycling rainbow
  GRADIENT = 0x02,     // Gradient between two colors
  BLINK = 0x03,        // On/Off blinking
};

// Maximum preset data size (for gradient mode: 11 bytes)
constexpr size_t MAX_PRESET_DATA_SIZE = 11;

// Effect data structures
struct FixedColorEffect
{
  rgbw_color_t color;
} __packed;

struct RainbowEffect
{
  uint8_t speed;       // 1-255: speed of color cycling
  uint8_t brightness;  // 0-255: overall brightness
} __packed;

struct GradientEffect
{
  rgbw_color_t color1;
  rgbw_color_t color2;
  uint8_t speed;  // 1-255: speed of gradient transition
} __packed;

struct BlinkEffect
{
  rgbw_color_t color;
  uint8_t period;  // 1-255: period in 100ms units
} __packed;

// Union to hold any effect data
union EffectData
{
  FixedColorEffect fixed_color;
  RainbowEffect rainbow;
  GradientEffect gradient;
  BlinkEffect blink;
  uint8_t raw[MAX_PRESET_DATA_SIZE - 1];  // -1 for mode byte
};

// Complete effect structure
struct Effect
{
  EffectMode mode;
  EffectData data;

  Effect() : mode(EffectMode::FIXED_COLOR), data{} {}

  // Get data size for current mode
  size_t get_data_size() const
  {
    switch (mode) {
      case EffectMode::FIXED_COLOR:
        return sizeof(FixedColorEffect);
      case EffectMode::RAINBOW:
        return sizeof(RainbowEffect);
      case EffectMode::GRADIENT:
        return sizeof(GradientEffect);
      case EffectMode::BLINK:
        return sizeof(BlinkEffect);
      default:
        return 0;
    }
  }

  // Get total size including mode byte
  size_t get_total_size() const { return 1 + get_data_size(); }

  // Serialize to buffer (for BLE transmission and storage)
  size_t serialize(uint8_t * buffer, size_t buffer_size) const
  {
    size_t total_size = get_total_size();
    if (buffer_size < total_size) {
      return 0;
    }

    buffer[0] = static_cast<uint8_t>(mode);
    memcpy(buffer + 1, &data, get_data_size());
    return total_size;
  }

  // Deserialize from buffer (from BLE reception and storage)
  bool deserialize(const uint8_t * buffer, size_t buffer_size)
  {
    if (buffer_size < 1) {
      return false;
    }

    mode = static_cast<EffectMode>(buffer[0]);
    size_t data_size = get_data_size();

    if (buffer_size < 1 + data_size) {
      return false;
    }

    memcpy(&data, buffer + 1, data_size);
    return true;
  }

  // Helper factory methods
  static Effect create_fixed_color(const rgbw_color_t & color)
  {
    Effect effect;
    effect.mode = EffectMode::FIXED_COLOR;
    effect.data.fixed_color.color = color;
    return effect;
  }

  static Effect create_rainbow(uint8_t speed, uint8_t brightness)
  {
    Effect effect;
    effect.mode = EffectMode::RAINBOW;
    effect.data.rainbow.speed = speed;
    effect.data.rainbow.brightness = brightness;
    return effect;
  }

  static Effect create_gradient(
    const rgbw_color_t & color1, const rgbw_color_t & color2, uint8_t speed)
  {
    Effect effect;
    effect.mode = EffectMode::GRADIENT;
    effect.data.gradient.color1 = color1;
    effect.data.gradient.color2 = color2;
    effect.data.gradient.speed = speed;
    return effect;
  }

  static Effect create_blink(const rgbw_color_t & color, uint8_t period)
  {
    Effect effect;
    effect.mode = EffectMode::BLINK;
    effect.data.blink.color = color;
    effect.data.blink.period = period;
    return effect;
  }

  // Backward compatibility: parse legacy 4-byte RGBW format
  static Effect from_legacy_rgbw(const rgbw_color_t & color)
  {
    return create_fixed_color(color);
  }
};

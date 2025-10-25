#pragma once

#include <zephyr/kernel.h>

#include <cstdint>

#include "ble_service.hpp"

// Color utility functions

// Convert HSV to RGB
// H: 0-359 degrees
// S: 0-255 (0-100%)
// V: 0-255 (0-100%)
inline rgbw_color_t hsv_to_rgb(uint16_t h, uint8_t s, uint8_t v)
{
  // Normalize hue to 0-359
  h = h % 360;

  uint8_t region = h / 60;
  uint8_t remainder = (h - (region * 60)) * 256 / 60;

  uint8_t p = (v * (255 - s)) >> 8;
  uint8_t q = (v * (255 - ((s * remainder) >> 8))) >> 8;
  uint8_t t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;

  rgbw_color_t color{};

  switch (region) {
    case 0:
      color.r = v;
      color.g = t;
      color.b = p;
      break;
    case 1:
      color.r = q;
      color.g = v;
      color.b = p;
      break;
    case 2:
      color.r = p;
      color.g = v;
      color.b = t;
      break;
    case 3:
      color.r = p;
      color.g = q;
      color.b = v;
      break;
    case 4:
      color.r = t;
      color.g = p;
      color.b = v;
      break;
    default:  // case 5:
      color.r = v;
      color.g = p;
      color.b = q;
      break;
  }

  color.w = 0;  // No white component in HSV conversion
  return color;
}

// Linear interpolation between two RGBW colors
// t: 0-255 (0 = color1, 255 = color2)
inline rgbw_color_t lerp_color(const rgbw_color_t & color1, const rgbw_color_t & color2, uint8_t t)
{
  rgbw_color_t result;
  result.r = color1.r + ((int16_t)(color2.r - color1.r) * t) / 255;
  result.g = color1.g + ((int16_t)(color2.g - color1.g) * t) / 255;
  result.b = color1.b + ((int16_t)(color2.b - color1.b) * t) / 255;
  result.w = color1.w + ((int16_t)(color2.w - color1.w) * t) / 255;
  return result;
}

// Apply brightness to RGBW color
// brightness: 0-255
inline rgbw_color_t apply_brightness(const rgbw_color_t & color, uint8_t brightness)
{
  rgbw_color_t result;
  result.r = (static_cast<uint16_t>(color.r) * brightness) / 255;
  result.g = (static_cast<uint16_t>(color.g) * brightness) / 255;
  result.b = (static_cast<uint16_t>(color.b) * brightness) / 255;
  result.w = (static_cast<uint16_t>(color.w) * brightness) / 255;
  return result;
}

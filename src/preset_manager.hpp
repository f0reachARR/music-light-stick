#pragma once

#include <zephyr/drivers/flash.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/storage/flash_map.h>

#include "ble_service.hpp"

#define PRESET_COUNT 20

class PresetManager
{
public:
  PresetManager() : current_preset_(0)
  {
    // Initialize with default colors (all off)
    for (int i = 0; i < PRESET_COUNT; i++) {
      presets_[i] = {0, 0, 0, 0};
    }

    // Set some default presets for testing
    presets_[0] = {255, 0, 0, 0};    // Red
    presets_[1] = {0, 255, 0, 0};    // Green
    presets_[2] = {0, 0, 255, 0};    // Blue
    presets_[3] = {0, 0, 0, 255};    // White
    presets_[4] = {255, 255, 0, 0};  // Yellow
  }

  void write_preset(uint8_t preset_number, const rgbw_color_t & color)
  {
    if (preset_number >= PRESET_COUNT) {
      return;
    }

    // Check if the color is different to avoid unnecessary writes
    if (memcmp(&presets_[preset_number], &color, sizeof(rgbw_color_t)) != 0) {
      presets_[preset_number] = color;
      // TODO: Save to non-volatile storage (EEPROM/Flash)
      // save_to_flash();
    }
  }

  rgbw_color_t read_preset(uint8_t preset_number) const
  {
    if (preset_number >= PRESET_COUNT) {
      return {0, 0, 0, 0};
    }
    return presets_[preset_number];
  }

  uint8_t get_current_preset() const { return current_preset_; }

  void set_current_preset(uint8_t preset_number)
  {
    if (preset_number >= PRESET_COUNT) {
      return;
    }
    current_preset_ = preset_number;
  }

  void next_preset()
  {
    current_preset_ = (current_preset_ + 1) % PRESET_COUNT;
  }

  rgbw_color_t get_current_color() const { return presets_[current_preset_]; }

private:
  rgbw_color_t presets_[PRESET_COUNT];
  uint8_t current_preset_;

  // TODO: Implement flash storage
  // void save_to_flash() { }
  // void load_from_flash() { }
};

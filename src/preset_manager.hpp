#pragma once

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "ble_service.hpp"
#include "settings_manager.hpp"

class PresetManager
{
public:
  PresetManager() {}

  int init()
  {
    int ret = SettingsManager::instance().init();
    if (ret) {
      printk("Failed to initialize settings (err %d)\n", ret);
      return ret;
    }

    ret = SettingsManager::instance().load();
    if (ret) {
      printk("Failed to load settings (err %d)\n", ret);
      return ret;
    }

    return 0;
  }

  void write_preset(uint8_t preset_number, const rgbw_color_t & color)
  {
    SettingsManager::instance().write_preset(preset_number, color);
  }

  rgbw_color_t read_preset(uint8_t preset_number) const
  {
    return SettingsManager::instance().read_preset(preset_number);
  }

  uint8_t get_current_preset() const { return current_preset_; }

  void set_current_preset(uint8_t preset_number) { current_preset_ = preset_number; }

  void next_preset() { current_preset_ = (current_preset_ + 1) % PRESET_COUNT; }

  rgbw_color_t get_current_color() const
  {
    return SettingsManager::instance().read_preset(current_preset_);
  }

private:
  uint8_t current_preset_;
};

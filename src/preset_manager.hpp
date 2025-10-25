#pragma once

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "ble_service.hpp"
#include "effect_mode.hpp"
#include "settings_manager.hpp"

class PresetManager
{
public:
  PresetManager() : current_preset_(0) {}

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

    printk("Preset manager initialized\n");

    return 0;
  }

  void write_preset(uint8_t preset_number, const Effect & effect)
  {
    SettingsManager::instance().write_preset(preset_number, effect);
  }

  Effect read_preset(uint8_t preset_number) const
  {
    return SettingsManager::instance().read_preset(preset_number);
  }

  // Backward compatibility
  void write_preset_legacy(uint8_t preset_number, const rgbw_color_t & color)
  {
    SettingsManager::instance().write_preset_legacy(preset_number, color);
  }

  uint8_t get_current_preset() const { return current_preset_; }

  void set_current_preset(uint8_t preset_number) { current_preset_ = preset_number; }

  void next_preset() { current_preset_ = (current_preset_ + 1) % PRESET_COUNT; }

  void prev_preset()
  {
    if (current_preset_ == 0) {
      current_preset_ = PRESET_COUNT - 1;
    } else {
      current_preset_--;
    }
  }

  Effect get_current_effect() const
  {
    return SettingsManager::instance().read_preset(current_preset_);
  }

private:
  uint8_t current_preset_;
};

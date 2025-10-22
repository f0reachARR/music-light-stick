#pragma once

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>

#include <cstring>

#include "ble_service.hpp"

#define PRESET_COUNT 20
#define SETTINGS_NAME_PENLIGHT "penlight"
#define SETTINGS_KEY_PRESETS "presets"

class SettingsManager
{
public:
  static SettingsManager & instance()
  {
    static SettingsManager instance;
    return instance;
  }

  int init()
  {
    int ret = settings_subsys_init();
    if (ret) {
      printk("Settings subsystem init failed (err %d)\n", ret);
      return ret;
    }

    // Register settings handler
    ret = settings_register(&settings_handler_);
    if (ret) {
      printk("Settings register failed (err %d)\n", ret);
      return ret;
    }

    return 0;
  }

  int load()
  {
    int ret = settings_load_subtree(SETTINGS_NAME_PENLIGHT);
    if (ret) {
      printk("Settings load failed (err %d)\n", ret);
      return ret;
    }

    printk("Settings loaded: presets=%d\n", presets_loaded_);
    return 0;
  }

  void write_preset(uint8_t preset_number, const rgbw_color_t & color)
  {
    if (preset_number >= PRESET_COUNT) {
      return;
    }

    // Update in-memory data
    presets_[preset_number] = color;

    // Schedule delayed save
    schedule_save();
  }

  rgbw_color_t read_preset(uint8_t preset_number) const
  {
    if (preset_number >= PRESET_COUNT) {
      return {0, 0, 0, 0};
    }
    return presets_[preset_number];
  }

  bool are_settings_loaded() const { return presets_loaded_; }

private:
  rgbw_color_t presets_[PRESET_COUNT];
  bool presets_loaded_;
  bool save_pending_;

  struct k_timer save_timer_;

  SettingsManager() : presets_(), presets_loaded_(false), save_pending_(false)
  {
    k_timer_init(&save_timer_, save_timer_callback, nullptr);

    // Initialize default presets
    for (int i = 0; i < PRESET_COUNT; i++) {
      presets_[i] = {0, 0, 0, 0};
    }

    // Set some default presets
    presets_[0] = {255, 0, 0, 0};    // Red
    presets_[1] = {0, 255, 0, 0};    // Green
    presets_[2] = {0, 0, 255, 0};    // Blue
    presets_[3] = {0, 0, 0, 255};    // White
    presets_[4] = {255, 255, 0, 0};  // Yellow
  }

  void schedule_save()
  {
    save_pending_ = true;
    k_timer_start(&save_timer_, K_SECONDS(1), K_NO_WAIT);
  }

  void save_all()
  {
    if (!save_pending_) {
      return;
    }

    // Save all presets
    int ret = settings_save_one(
      SETTINGS_NAME_PENLIGHT "/" SETTINGS_KEY_PRESETS, presets_, sizeof(presets_));
    if (ret) {
      printk("Failed to save presets (err %d)\n", ret);
    } else {
      printk("Presets saved to flash\n");
    }

    save_pending_ = false;
  }

  static void save_timer_callback(struct k_timer * timer)
  {
    instance().save_all();
    k_timer_stop(timer);
  }

  // Settings callback functions
  static int settings_set(const char * name, size_t len, settings_read_cb read_cb, void * cb_arg)
  {
    auto & manager = instance();
    const char * next;
    int rc;

    printk("Setting %s\n", name);

    if (settings_name_steq(name, SETTINGS_KEY_PRESETS, &next) && !next) {
      if (len != sizeof(manager.presets_)) {
        printk("Invalid presets data size: %zu (expected %zu)\n", len, sizeof(manager.presets_));
        return -EINVAL;
      }

      rc = read_cb(cb_arg, manager.presets_, sizeof(manager.presets_));
      if (rc >= 0) {
        manager.presets_loaded_ = true;
        printk("Loaded presets from settings\n");
        return 0;
      }
      return rc;
    }

    return -ENOENT;
  }

  static int settings_export(int (*cb)(const char * name, const void * value, size_t val_len))
  {
    // Not implemented yet
    return 0;
  }

  struct settings_handler settings_handler_ = {
    .name = SETTINGS_NAME_PENLIGHT,
    .h_get = nullptr,
    .h_set = settings_set,
    .h_commit = nullptr,
    .h_export = settings_export,
  };

  friend class PresetManagerWithSettings;
};

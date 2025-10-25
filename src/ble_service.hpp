#pragma once

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <array>

#include "effect_mode.hpp"

// Penlight Control Service UUID: 0000ff00-0000-1000-8000-00805f9b34fb
#define BT_UUID_PENLIGHT_SERVICE_VAL \
  BT_UUID_128_ENCODE(0x0000ff00, 0x0000, 0x1000, 0x8000, 0x00805f9b34fb)
#define BT_UUID_PENLIGHT_SERVICE BT_UUID_DECLARE_128(BT_UUID_PENLIGHT_SERVICE_VAL)

// Preset Write Characteristic UUID: 0000ff01-...
#define BT_UUID_PRESET_WRITE_VAL \
  BT_UUID_128_ENCODE(0x0000ff01, 0x0000, 0x1000, 0x8000, 0x00805f9b34fb)
#define BT_UUID_PRESET_WRITE BT_UUID_DECLARE_128(BT_UUID_PRESET_WRITE_VAL)

// Preset Read Characteristic UUID: 0000ff02-...
#define BT_UUID_PRESET_READ_VAL \
  BT_UUID_128_ENCODE(0x0000ff02, 0x0000, 0x1000, 0x8000, 0x00805f9b34fb)
#define BT_UUID_PRESET_READ BT_UUID_DECLARE_128(BT_UUID_PRESET_READ_VAL)

// Preview Color Characteristic UUID: 0000ff03-...
#define BT_UUID_PREVIEW_COLOR_VAL \
  BT_UUID_128_ENCODE(0x0000ff03, 0x0000, 0x1000, 0x8000, 0x00805f9b34fb)
#define BT_UUID_PREVIEW_COLOR BT_UUID_DECLARE_128(BT_UUID_PREVIEW_COLOR_VAL)

// Current Preset Characteristic UUID: 0000ff04-...
#define BT_UUID_CURRENT_PRESET_VAL \
  BT_UUID_128_ENCODE(0x0000ff04, 0x0000, 0x1000, 0x8000, 0x00805f9b34fb)
#define BT_UUID_CURRENT_PRESET BT_UUID_DECLARE_128(BT_UUID_CURRENT_PRESET_VAL)

// Exit Preview Characteristic UUID: 0000ff05-...
#define BT_UUID_EXIT_PREVIEW_VAL \
  BT_UUID_128_ENCODE(0x0000ff05, 0x0000, 0x1000, 0x8000, 0x00805f9b34fb)
#define BT_UUID_EXIT_PREVIEW BT_UUID_DECLARE_128(BT_UUID_EXIT_PREVIEW_VAL)

// Battery Level Characteristic UUID: 0000ff06-...
#define BT_UUID_BATTERY_LEVEL_VAL \
  BT_UUID_128_ENCODE(0x0000ff06, 0x0000, 0x1000, 0x8000, 0x00805f9b34fb)
#define BT_UUID_BATTERY_LEVEL BT_UUID_DECLARE_128(BT_UUID_BATTERY_LEVEL_VAL)

// Preset write data structure
struct preset_write_t
{
  uint8_t preset_number;
  rgbw_color_t color;
} __packed;

// Forward declarations
class PenlightBLEService;

// Callback function types
using PresetWriteCallback = void (*)(uint8_t preset, const Effect & effect);
using PresetReadCallback = void (*)(uint8_t preset);
using PreviewColorCallback = void (*)(const Effect & effect);
using ExitPreviewCallback = void (*)();
using CurrentPresetReadCallback = uint8_t (*)();

class PenlightBLEService
{
public:
  // Singleton instance accessor
  static PenlightBLEService & instance()
  {
    static PenlightBLEService instance;
    return instance;
  }

  // Delete copy constructor and assignment operator
  PenlightBLEService(const PenlightBLEService &) = delete;
  PenlightBLEService & operator=(const PenlightBLEService &) = delete;

  void set_preset_write_callback(PresetWriteCallback cb) { preset_write_cb_ = cb; }
  void set_preset_read_callback(PresetReadCallback cb) { preset_read_cb_ = cb; }
  void set_preview_color_callback(PreviewColorCallback cb) { preview_color_cb_ = cb; }
  void set_exit_preview_callback(ExitPreviewCallback cb) { exit_preview_cb_ = cb; }
  void set_current_preset_read_callback(CurrentPresetReadCallback cb)
  {
    current_preset_read_cb_ = cb;
  }

  static void set_preset_read_data(const Effect & effect)
  {
    instance().preset_read_effect_ = effect;
    // Serialize effect to buffer for reading
    instance().preset_read_size_ =
      effect.serialize(instance().preset_read_data_, sizeof(instance().preset_read_data_));
  }

  static void notify_current_preset(uint8_t preset);

  static void ccc_current_preset_cfg_changed(const struct bt_gatt_attr * attr, uint16_t value)
  {
    instance().preset_notify_enabled_ = value == BT_GATT_CCC_NOTIFY;
  }

  static void notify_battery_level(uint8_t level);

  static void ccc_battery_level_cfg_changed(const struct bt_gatt_attr * attr, uint16_t value)
  {
    instance().battery_level_notify_enabled_ = value == BT_GATT_CCC_NOTIFY;
  }

  // Static callbacks for GATT operations
  static ssize_t write_preset_write(
    struct bt_conn * conn, const struct bt_gatt_attr * attr, const void * buf, uint16_t len,
    uint16_t offset, uint8_t flags)
  {
    if (offset != 0) {
      return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
    }

    // Check minimum length (preset number + mode byte)
    if (len < 2) {
      return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
    }

    const auto * data = static_cast<const uint8_t *>(buf);
    uint8_t preset_number = data[0];

    if (preset_number >= 20) {
      return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
    }

    // Parse effect from remaining data
    Effect effect;
    if (!effect.deserialize(data + 1, len - 1)) {
      // Try legacy 4-byte RGBW format for backward compatibility
      if (len == 5) {
        rgbw_color_t color;
        memcpy(&color, data + 1, sizeof(rgbw_color_t));
        effect = Effect::from_legacy_rgbw(color);
      } else {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
      }
    }

    if (instance().preset_write_cb_) {
      instance().preset_write_cb_(preset_number, effect);
    }

    return len;
  }

  static ssize_t write_preset_read(
    struct bt_conn * conn, const struct bt_gatt_attr * attr, const void * buf, uint16_t len,
    uint16_t offset, uint8_t flags)
  {
    if (offset + len > 1) {
      return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
    }

    const uint8_t preset = *static_cast<const uint8_t *>(buf);
    if (preset >= 20) {
      return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
    }

    if (instance().preset_read_cb_) {
      instance().preset_read_cb_(preset);
    }

    return len;
  }

  static ssize_t read_preset_read(
    struct bt_conn * conn, const struct bt_gatt_attr * attr, void * buf, uint16_t len,
    uint16_t offset)
  {
    return bt_gatt_attr_read(
      conn, attr, buf, len, offset, instance().preset_read_data_, instance().preset_read_size_);
  }

  static ssize_t write_preview_color(
    struct bt_conn * conn, const struct bt_gatt_attr * attr, const void * buf, uint16_t len,
    uint16_t offset, uint8_t flags)
  {
    if (offset != 0) {
      return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
    }

    // Check minimum length (mode byte)
    if (len < 1) {
      return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
    }

    const auto * data = static_cast<const uint8_t *>(buf);

    // Parse effect
    Effect effect;
    if (!effect.deserialize(data, len)) {
      // Try legacy 4-byte RGBW format for backward compatibility
      if (len == 4) {
        rgbw_color_t color;
        memcpy(&color, data, sizeof(rgbw_color_t));
        effect = Effect::from_legacy_rgbw(color);
      } else {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
      }
    }

    if (instance().preview_color_cb_) {
      instance().preview_color_cb_(effect);
    }

    return len;
  }

  static ssize_t read_current_preset(
    struct bt_conn * conn, const struct bt_gatt_attr * attr, void * buf, uint16_t len,
    uint16_t offset)
  {
    uint8_t current_preset = instance().current_preset_;
    if (instance().current_preset_read_cb_) {
      current_preset = instance().current_preset_read_cb_();
    }
    return bt_gatt_attr_read(conn, attr, buf, len, offset, &current_preset, sizeof(uint8_t));
  }

  static ssize_t write_exit_preview(
    struct bt_conn * conn, const struct bt_gatt_attr * attr, const void * buf, uint16_t len,
    uint16_t offset, uint8_t flags)
  {
    if (offset + len > 1) {
      return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
    }

    const uint8_t cmd = *static_cast<const uint8_t *>(buf);
    if (cmd == 0x01 && instance().exit_preview_cb_) {
      instance().exit_preview_cb_();
    }

    return len;
  }

  static ssize_t read_battery_level(
    struct bt_conn * conn, const struct bt_gatt_attr * attr, void * buf, uint16_t len,
    uint16_t offset)
  {
    return bt_gatt_attr_read(
      conn, attr, buf, len, offset, &instance().battery_level_, sizeof(uint8_t));
  }

private:
  // Private constructor for singleton
  PenlightBLEService()
  : preset_write_cb_(nullptr),
    preset_read_cb_(nullptr),
    preview_color_cb_(nullptr),
    exit_preview_cb_(nullptr),
    current_preset_read_cb_(nullptr),
    current_preset_(0),
    battery_level_(100),
    preset_read_effect_(),
    preset_read_size_(0),
    preset_notify_enabled_(false),
    battery_level_notify_enabled_(false)
  {
    memset(preset_read_data_, 0, sizeof(preset_read_data_));
  }

  PresetWriteCallback preset_write_cb_;
  PresetReadCallback preset_read_cb_;
  PreviewColorCallback preview_color_cb_;
  ExitPreviewCallback exit_preview_cb_;
  CurrentPresetReadCallback current_preset_read_cb_;

  uint8_t current_preset_;
  uint8_t battery_level_;
  Effect preset_read_effect_;
  uint8_t preset_read_data_[MAX_PRESET_DATA_SIZE];
  size_t preset_read_size_;

  bool preset_notify_enabled_, battery_level_notify_enabled_;
};

BT_GATT_SERVICE_DEFINE(
  penlight_service, BT_GATT_PRIMARY_SERVICE(BT_UUID_PENLIGHT_SERVICE),
  // Preset Write (Write only)
  BT_GATT_CHARACTERISTIC(
    BT_UUID_PRESET_WRITE, BT_GATT_CHRC_WRITE, BT_GATT_PERM_WRITE, nullptr,
    PenlightBLEService::write_preset_write, nullptr),
  // Preset Read (Read + Write)
  BT_GATT_CHARACTERISTIC(
    BT_UUID_PRESET_READ, BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
    BT_GATT_PERM_READ | BT_GATT_PERM_WRITE, PenlightBLEService::read_preset_read,
    PenlightBLEService::write_preset_read, nullptr),
  // Preview Color (Write only)
  BT_GATT_CHARACTERISTIC(
    BT_UUID_PREVIEW_COLOR, BT_GATT_CHRC_WRITE, BT_GATT_PERM_WRITE, nullptr,
    PenlightBLEService::write_preview_color, nullptr),
  // Current Preset (Read + Notify)
  BT_GATT_CHARACTERISTIC(
    BT_UUID_CURRENT_PRESET, BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_READ,
    PenlightBLEService::read_current_preset, nullptr, nullptr),
  BT_GATT_CCC(
    PenlightBLEService::ccc_current_preset_cfg_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
  // Exit Preview (Write only)
  BT_GATT_CHARACTERISTIC(
    BT_UUID_EXIT_PREVIEW, BT_GATT_CHRC_WRITE, BT_GATT_PERM_WRITE, nullptr,
    PenlightBLEService::write_exit_preview, nullptr),
  // Battery Level (Read + Notify)
  BT_GATT_CHARACTERISTIC(
    BT_UUID_BATTERY_LEVEL, BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_READ,
    PenlightBLEService::read_battery_level, nullptr, nullptr),
  BT_GATT_CCC(
    PenlightBLEService::ccc_battery_level_cfg_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE));

// Implementation of notify_current_preset
inline void PenlightBLEService::notify_current_preset(uint8_t preset)
{
  instance().current_preset_ = preset;
  if (instance().preset_notify_enabled_) {
    bt_gatt_notify(
      nullptr, &penlight_service.attrs[8], &instance().current_preset_,
      sizeof(instance().current_preset_));
  }
}

// Implementation of notify_battery_level
inline void PenlightBLEService::notify_battery_level(uint8_t level)
{
  instance().battery_level_ = level;
  if (instance().battery_level_notify_enabled_) {
    bt_gatt_notify(
      nullptr, &penlight_service.attrs[14], &instance().battery_level_,
      sizeof(instance().battery_level_));
  }
}

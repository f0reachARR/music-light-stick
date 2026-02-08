/*
 * BLE Service - Penlight Control BLE GATT Service
 */

#include "ble_service.h"

#include <string.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "effect_engine.h"
#include "effect_types.h"
#include "power_manager.h"
#include "preset_manager.h"
#include "preview_manager.h"

LOG_MODULE_REGISTER(ble_service, LOG_LEVEL_DBG);

/* Service UUID: 0000ff00-0000-1000-8000-00805f9b34fb */
#define BT_UUID_PENLIGHT_SERVICE_VAL \
  BT_UUID_128_ENCODE(0x0000ff00, 0x0000, 0x1000, 0x8000, 0x00805f9b34fb)
#define BT_UUID_PENLIGHT_SERVICE BT_UUID_DECLARE_128(BT_UUID_PENLIGHT_SERVICE_VAL)

/* Characteristic UUIDs */
#define BT_UUID_PRESET_WRITE_VAL \
  BT_UUID_128_ENCODE(0x0000ff01, 0x0000, 0x1000, 0x8000, 0x00805f9b34fb)
#define BT_UUID_PRESET_WRITE BT_UUID_DECLARE_128(BT_UUID_PRESET_WRITE_VAL)

#define BT_UUID_PRESET_READ_VAL \
  BT_UUID_128_ENCODE(0x0000ff02, 0x0000, 0x1000, 0x8000, 0x00805f9b34fb)
#define BT_UUID_PRESET_READ BT_UUID_DECLARE_128(BT_UUID_PRESET_READ_VAL)

#define BT_UUID_PRESET_SAVE_VAL \
  BT_UUID_128_ENCODE(0x0000ff03, 0x0000, 0x1000, 0x8000, 0x00805f9b34fb)
#define BT_UUID_PRESET_SAVE BT_UUID_DECLARE_128(BT_UUID_PRESET_SAVE_VAL)

#define BT_UUID_PREVIEW_COLOR_VAL \
  BT_UUID_128_ENCODE(0x0000ff04, 0x0000, 0x1000, 0x8000, 0x00805f9b34fb)
#define BT_UUID_PREVIEW_COLOR BT_UUID_DECLARE_128(BT_UUID_PREVIEW_COLOR_VAL)

#define BT_UUID_CURRENT_PRESET_VAL \
  BT_UUID_128_ENCODE(0x0000ff05, 0x0000, 0x1000, 0x8000, 0x00805f9b34fb)
#define BT_UUID_CURRENT_PRESET BT_UUID_DECLARE_128(BT_UUID_CURRENT_PRESET_VAL)

#define BT_UUID_EXIT_PREVIEW_VAL \
  BT_UUID_128_ENCODE(0x0000ff06, 0x0000, 0x1000, 0x8000, 0x00805f9b34fb)
#define BT_UUID_EXIT_PREVIEW BT_UUID_DECLARE_128(BT_UUID_EXIT_PREVIEW_VAL)

#define BT_UUID_BATTERY_LEVEL_VAL \
  BT_UUID_128_ENCODE(0x0000ff07, 0x0000, 0x1000, 0x8000, 0x00805f9b34fb)
#define BT_UUID_BATTERY_LEVEL BT_UUID_DECLARE_128(BT_UUID_BATTERY_LEVEL_VAL)

/* 状態 */
static struct bt_conn * current_conn;
static uint8_t preset_read_index;
static uint8_t current_preset_value;
static uint8_t battery_level_value;
static bool current_preset_notify_enabled;
static bool battery_level_notify_enabled;

/* BLE受信データからプリセットデータへ変換 */
static int parse_effect_data(
  const uint8_t * data, uint16_t len, struct preset_data * preset, bool has_preset_num)
{
  uint8_t offset = has_preset_num ? 1 : 0;

  if (len < offset + 1) {
    return -EINVAL;
  }

  uint8_t mode = data[offset];
  preset->mode = mode;

  switch (mode) {
    case EFFECT_MODE_SOLID:
      if (len < offset + 5) {
        return -EINVAL;
      }
      preset->data.solid.r = data[offset + 1];
      preset->data.solid.g = data[offset + 2];
      preset->data.solid.b = data[offset + 3];
      preset->data.solid.w = data[offset + 4];
      break;

    case EFFECT_MODE_RAINBOW:
      if (len < offset + 3) {
        return -EINVAL;
      }
      preset->data.rainbow.speed = data[offset + 1];
      preset->data.rainbow.brightness = data[offset + 2];
      break;

    case EFFECT_MODE_GRADIENT:
      if (len < offset + 10) {
        return -EINVAL;
      }
      preset->data.gradient.r1 = data[offset + 1];
      preset->data.gradient.g1 = data[offset + 2];
      preset->data.gradient.b1 = data[offset + 3];
      preset->data.gradient.w1 = data[offset + 4];
      preset->data.gradient.r2 = data[offset + 5];
      preset->data.gradient.g2 = data[offset + 6];
      preset->data.gradient.b2 = data[offset + 7];
      preset->data.gradient.w2 = data[offset + 8];
      preset->data.gradient.speed = data[offset + 9];
      break;

    case EFFECT_MODE_BLINK:
      if (len < offset + 6) {
        return -EINVAL;
      }
      preset->data.blink.r = data[offset + 1];
      preset->data.blink.g = data[offset + 2];
      preset->data.blink.b = data[offset + 3];
      preset->data.blink.w = data[offset + 4];
      preset->data.blink.period = data[offset + 5];
      break;

    default:
      return -EINVAL;
  }

  return 0;
}

/* プリセットデータを送信バッファに変換 */
static int encode_effect_data(const struct preset_data * preset, uint8_t * buf, size_t buf_size)
{
  int len = 0;

  buf[len++] = preset->mode;

  switch (preset->mode) {
    case EFFECT_MODE_SOLID:
      if (buf_size < 5) return -ENOMEM;
      buf[len++] = preset->data.solid.r;
      buf[len++] = preset->data.solid.g;
      buf[len++] = preset->data.solid.b;
      buf[len++] = preset->data.solid.w;
      break;

    case EFFECT_MODE_RAINBOW:
      if (buf_size < 3) return -ENOMEM;
      buf[len++] = preset->data.rainbow.speed;
      buf[len++] = preset->data.rainbow.brightness;
      break;

    case EFFECT_MODE_GRADIENT:
      if (buf_size < 10) return -ENOMEM;
      buf[len++] = preset->data.gradient.r1;
      buf[len++] = preset->data.gradient.g1;
      buf[len++] = preset->data.gradient.b1;
      buf[len++] = preset->data.gradient.w1;
      buf[len++] = preset->data.gradient.r2;
      buf[len++] = preset->data.gradient.g2;
      buf[len++] = preset->data.gradient.b2;
      buf[len++] = preset->data.gradient.w2;
      buf[len++] = preset->data.gradient.speed;
      break;

    case EFFECT_MODE_BLINK:
      if (buf_size < 6) return -ENOMEM;
      buf[len++] = preset->data.blink.r;
      buf[len++] = preset->data.blink.g;
      buf[len++] = preset->data.blink.b;
      buf[len++] = preset->data.blink.w;
      buf[len++] = preset->data.blink.period;
      break;

    default:
      return -EINVAL;
  }

  return len;
}

/* Preset Write callback */
static ssize_t preset_write_cb(
  struct bt_conn * conn, const struct bt_gatt_attr * attr, const void * buf, uint16_t len,
  uint16_t offset, uint8_t flags)
{
  const uint8_t * data = buf;

  if (len < 2) {
    return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
  }

  uint8_t preset_index = data[0];
  if (preset_index >= PRESET_COUNT) {
    return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
  }

  struct preset_data preset;
  int rc = parse_effect_data(data, len, &preset, true);
  if (rc < 0) {
    return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
  }

  rc = preset_set(preset_index, &preset);
  if (rc < 0) {
    return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
  }

  LOG_INF("Preset %d written: mode=%d", preset_index, preset.mode);
  return len;
}

/* Preset Read write callback (select preset) */
static ssize_t preset_read_write_cb(
  struct bt_conn * conn, const struct bt_gatt_attr * attr, const void * buf, uint16_t len,
  uint16_t offset, uint8_t flags)
{
  if (len != 1) {
    return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
  }

  const uint8_t * data = buf;
  if (data[0] >= PRESET_COUNT) {
    return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
  }

  preset_read_index = data[0];
  LOG_INF("Preset read index set: %d", preset_read_index);
  return len;
}

/* Preset Read read callback */
static ssize_t preset_read_cb(
  struct bt_conn * conn, const struct bt_gatt_attr * attr, void * buf, uint16_t len,
  uint16_t offset)
{
  struct preset_data preset;
  uint8_t data[PRESET_DATA_MAX_SIZE];

  int rc = preset_get(preset_read_index, &preset);
  if (rc < 0) {
    return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
  }

  int data_len = encode_effect_data(&preset, data, sizeof(data));
  if (data_len < 0) {
    return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
  }

  return bt_gatt_attr_read(conn, attr, buf, len, offset, data, data_len);
}

/* Preset Save callback */
static ssize_t preset_save_cb(
  struct bt_conn * conn, const struct bt_gatt_attr * attr, const void * buf, uint16_t len,
  uint16_t offset, uint8_t flags)
{
  int rc = preset_save_all();
  if (rc < 0) {
    LOG_ERR("Failed to save presets: %d", rc);
    return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
  }

  LOG_INF("All presets saved");
  return len;
}

/* Preview Color callback */
static ssize_t preview_color_cb(
  struct bt_conn * conn, const struct bt_gatt_attr * attr, const void * buf, uint16_t len,
  uint16_t offset, uint8_t flags)
{
  const uint8_t * data = buf;

  if (len < 1) {
    return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
  }

  struct preset_data preset;
  int rc = parse_effect_data(data, len, &preset, false);
  if (rc < 0) {
    return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
  }

  preview_start(&preset);
  LOG_INF("Preview started: mode=%d", preset.mode);
  return len;
}

/* Current Preset read callback */
static ssize_t current_preset_read_cb(
  struct bt_conn * conn, const struct bt_gatt_attr * attr, void * buf, uint16_t len,
  uint16_t offset)
{
  current_preset_value = preset_get_current_index();
  return bt_gatt_attr_read(
    conn, attr, buf, len, offset, &current_preset_value, sizeof(current_preset_value));
}

/* Current Preset CCC changed callback */
static void current_preset_ccc_changed(const struct bt_gatt_attr * attr, uint16_t value)
{
  current_preset_notify_enabled = (value == BT_GATT_CCC_NOTIFY);
  LOG_INF("Current Preset notify %s", current_preset_notify_enabled ? "enabled" : "disabled");
}

/* Exit Preview callback */
static ssize_t exit_preview_cb(
  struct bt_conn * conn, const struct bt_gatt_attr * attr, const void * buf, uint16_t len,
  uint16_t offset, uint8_t flags)
{
  if (len != 1) {
    return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
  }

  const uint8_t * data = buf;
  if (data[0] == 0x01) {
    preview_exit();
    LOG_INF("Preview exited via BLE");
  }

  return len;
}

/* Battery Level read callback */
static ssize_t battery_level_read_cb(
  struct bt_conn * conn, const struct bt_gatt_attr * attr, void * buf, uint16_t len,
  uint16_t offset)
{
  battery_level_value = power_get_battery_level();
  return bt_gatt_attr_read(
    conn, attr, buf, len, offset, &battery_level_value, sizeof(battery_level_value));
}

/* Battery Level CCC changed callback */
static void battery_level_ccc_changed(const struct bt_gatt_attr * attr, uint16_t value)
{
  battery_level_notify_enabled = (value == BT_GATT_CCC_NOTIFY);
  LOG_INF("Battery Level notify %s", battery_level_notify_enabled ? "enabled" : "disabled");
}

/* GATT Service Definition */
BT_GATT_SERVICE_DEFINE(
  penlight_svc, BT_GATT_PRIMARY_SERVICE(BT_UUID_PENLIGHT_SERVICE),

  /* Preset Write (0xff01) - Write */
  BT_GATT_CHARACTERISTIC(
    BT_UUID_PRESET_WRITE, BT_GATT_CHRC_WRITE, BT_GATT_PERM_WRITE, NULL, preset_write_cb, NULL),

  /* Preset Read (0xff02) - Read/Write */
  BT_GATT_CHARACTERISTIC(
    BT_UUID_PRESET_READ, BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
    BT_GATT_PERM_READ | BT_GATT_PERM_WRITE, preset_read_cb, preset_read_write_cb, NULL),

  /* Preset Save (0xff03) - Write */
  BT_GATT_CHARACTERISTIC(
    BT_UUID_PRESET_SAVE, BT_GATT_CHRC_WRITE, BT_GATT_PERM_WRITE, NULL, preset_save_cb, NULL),

  /* Preview Color (0xff04) - Write */
  BT_GATT_CHARACTERISTIC(
    BT_UUID_PREVIEW_COLOR, BT_GATT_CHRC_WRITE, BT_GATT_PERM_WRITE, NULL, preview_color_cb, NULL),

  /* Current Preset (0xff05) - Read/Notify */
  BT_GATT_CHARACTERISTIC(
    BT_UUID_CURRENT_PRESET, BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_READ,
    current_preset_read_cb, NULL, NULL),
  BT_GATT_CCC(current_preset_ccc_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),

  /* Exit Preview (0xff06) - Write */
  BT_GATT_CHARACTERISTIC(
    BT_UUID_EXIT_PREVIEW, BT_GATT_CHRC_WRITE, BT_GATT_PERM_WRITE, NULL, exit_preview_cb, NULL),

  /* Battery Level (0xff07) - Read/Notify */
  BT_GATT_CHARACTERISTIC(
    BT_UUID_BATTERY_LEVEL, BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_READ,
    battery_level_read_cb, NULL, NULL),
  BT_GATT_CCC(battery_level_ccc_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE), );

/* Advertising data */
static const struct bt_data ad[] = {
  BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
  BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_PENLIGHT_SERVICE_VAL),
};

/* Scan response data */
static const struct bt_data sd[] = {
  BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

/* Connection callbacks */
static void connected(struct bt_conn * conn, uint8_t err)
{
  if (err) {
    LOG_ERR("Connection failed (err %u)", err);
    return;
  }

  current_conn = bt_conn_ref(conn);
  LOG_INF("Connected");
}

static void disconnected(struct bt_conn * conn, uint8_t reason)
{
  LOG_INF("Disconnected (reason %u)", reason);

  if (current_conn) {
    bt_conn_unref(current_conn);
    current_conn = NULL;
  }

  /* 再広告 */
  ble_start_advertising();
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
  .connected = connected,
  .disconnected = disconnected,
};

int ble_service_init(void)
{
  int err;

  err = bt_enable(NULL);
  if (err) {
    LOG_ERR("Bluetooth init failed (err %d)", err);
    return err;
  }

  LOG_INF("Bluetooth initialized");
  return 0;
}

int ble_start_advertising(void)
{
  int err;

  err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
  if (err) {
    LOG_ERR("Advertising failed to start (err %d)", err);
    return err;
  }

  LOG_INF("Advertising started");
  return 0;
}

int ble_stop_advertising(void)
{
  int err;

  err = bt_le_adv_stop();
  if (err) {
    LOG_ERR("Advertising failed to stop (err %d)", err);
    return err;
  }

  LOG_INF("Advertising stopped");
  return 0;
}

void ble_notify_current_preset(uint8_t preset_index)
{
  if (!current_conn || !current_preset_notify_enabled) {
    return;
  }

  current_preset_value = preset_index;

  int err = bt_gatt_notify(
    current_conn, &penlight_svc.attrs[10], &current_preset_value, sizeof(current_preset_value));
  if (err) {
    LOG_ERR("Current Preset notify failed (err %d)", err);
  }
}

void ble_notify_battery_level(uint8_t level)
{
  if (!current_conn || !battery_level_notify_enabled) {
    return;
  }

  battery_level_value = level;

  int err = bt_gatt_notify(
    current_conn, &penlight_svc.attrs[15], &battery_level_value, sizeof(battery_level_value));
  if (err) {
    LOG_ERR("Battery Level notify failed (err %d)", err);
  }
}

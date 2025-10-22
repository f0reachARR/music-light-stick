/*
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file Penlight BLE Control Application
 */

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>

#include "ble_service.hpp"
#include "button_handler.hpp"
#include "led_controller.hpp"
#include "preset_manager.hpp"
#include "preview_manager.hpp"
#include "settings_manager.hpp"

LOG_MODULE_REGISTER(penlight, LOG_LEVEL_DBG);

// Device tree aliases for PWM LEDs
static const struct pwm_dt_spec pwm_r = PWM_DT_SPEC_GET(DT_ALIAS(pwm_led_r));
static const struct pwm_dt_spec pwm_g = PWM_DT_SPEC_GET(DT_ALIAS(pwm_led_g));
static const struct pwm_dt_spec pwm_b = PWM_DT_SPEC_GET(DT_ALIAS(pwm_led_b));
static const struct pwm_dt_spec pwm_w = PWM_DT_SPEC_GET(DT_ALIAS(pwm_led_w));

// Device tree spec for button
static const struct gpio_dt_spec button_spec = GPIO_DT_SPEC_GET(DT_ALIAS(button0), gpios);

// Global instances
static LEDController led_controller(&pwm_r, &pwm_g, &pwm_b, &pwm_w);
static PresetManager preset_manager;
static PreviewManager preview_manager;
static ButtonHandler button_handler(&button_spec);

// BLE Advertisement data
static const struct bt_data ad[] = {
  BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
  BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_PENLIGHT_SERVICE_VAL),
};

// BLE Scan response data
static const struct bt_data sd[] = {
  BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

// Forward declarations
static void update_led_display();
static void on_preset_write(uint8_t preset, const rgbw_color_t & color);
static void on_preset_read(uint8_t preset);
static void on_preview_color(const rgbw_color_t & color);
static void on_exit_preview();
static uint8_t on_current_preset_read();
static void on_button_pressed(void * user_data);
static void on_button_pressed_worker(struct k_work * work);
static void on_preview_timeout(void * user_data);

K_WORK_DEFINE(button_work, on_button_pressed_worker);

static void bt_ready(int err)
{
  if (err) {
    printk("Bluetooth init failed (err %d)\n", err);
    return;
  }

  printk("Bluetooth initialized\n");

  // Start advertising
  err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
  if (err) {
    printk("Advertising failed to start (err %d)\n", err);
    return;
  }

  printk("Advertising successfully started\n");
}

static void connected(struct bt_conn * conn, uint8_t err)
{
  if (err) {
    printk("Connection failed (err %d)\n", err);
  } else {
    printk("Connected\n");
  }
}

static void disconnected(struct bt_conn * conn, uint8_t reason)
{
  printk("Disconnected (reason %d)\n", reason);
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
  .connected = connected,
  .disconnected = disconnected,
};

int main(void)
{
  int err;

  printk("Starting Penlight BLE Application\n");

  // Initialize settings and preset manager
  err = preset_manager.init();
  if (err) {
    printk("Failed to initialize preset manager (err %d)\n", err);
    return 0;
  }

  // Initialize LED controller
  if (!led_controller.init()) {
    printk("Failed to initialize LED controller\n");
    return 0;
  }

  // Initialize button handler
  if (!button_handler.init()) {
    printk("Failed to initialize button handler\n");
    return 0;
  }

  // Initialize button work
  k_work_init(&button_work, on_button_pressed_worker);

  // Get singleton instance and set up callbacks
  auto & ble_service = PenlightBLEService::instance();
  ble_service.set_preset_write_callback(on_preset_write);
  ble_service.set_preset_read_callback(on_preset_read);
  ble_service.set_preview_color_callback(on_preview_color);
  ble_service.set_exit_preview_callback(on_exit_preview);
  ble_service.set_current_preset_read_callback(on_current_preset_read);

  button_handler.set_callback(on_button_pressed, nullptr);
  preview_manager.set_timeout_callback(on_preview_timeout, nullptr);

  // Initialize Bluetooth
  err = bt_enable(bt_ready);
  if (err) {
    printk("Bluetooth init failed (err %d)\n", err);
    return 0;
  }

  // Set initial LED color to preset 0
  update_led_display();

  printk("Penlight initialized successfully\n");

  // Main loop
  while (1) {
    k_sleep(K_MSEC(1000));
  }

  return 0;
}

static void update_led_display()
{
  if (preview_manager.is_in_preview_mode()) {
    led_controller.set_color(preview_manager.get_preview_color());
  } else {
    led_controller.set_color(preset_manager.get_current_color());
  }
}

static void on_preset_write(uint8_t preset, const rgbw_color_t & color)
{
  printk(
    "Preset write: preset=%d, R=%d, G=%d, B=%d, W=%d\n", preset, color.r, color.g, color.b,
    color.w);
  preset_manager.write_preset(preset, color);
}

static void on_preset_read(uint8_t preset)
{
  printk("Preset read request: preset=%d\n", preset);
  rgbw_color_t color = preset_manager.read_preset(preset);
  PenlightBLEService::instance().set_preset_read_data(color);
  printk("Preset read: R=%d, G=%d, B=%d, W=%d\n", color.r, color.g, color.b, color.w);
}

static void on_preview_color(const rgbw_color_t & color)
{
  printk("Preview color: R=%d, G=%d, B=%d, W=%d\n", color.r, color.g, color.b, color.w);
  preview_manager.enter_preview_mode(color);
  update_led_display();
}

static void on_exit_preview()
{
  printk("Exit preview mode\n");
  preview_manager.exit_preview_mode();
  update_led_display();
}

static uint8_t on_current_preset_read() { return preset_manager.get_current_preset(); }

static void on_button_pressed(void * user_data)
{
  printk("Button pressed (from callback)\n");
  k_work_submit(&button_work);
}

static void on_button_pressed_worker(struct k_work * work)
{
  printk("Button pressed\n");

  // If in preview mode, exit it
  if (preview_manager.is_in_preview_mode()) {
    preview_manager.exit_preview_mode();
  } else {
    // Otherwise, advance to next preset
    preset_manager.next_preset();
  }

  update_led_display();

  // Notify BLE clients of the new preset
  PenlightBLEService::instance().notify_current_preset(preset_manager.get_current_preset());
}

static void on_preview_timeout(void * user_data)
{
  printk("Preview timeout\n");
  update_led_display();
}

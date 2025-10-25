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
#include <zephyr/drivers/hwinfo.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/pm/pm.h>
#include <zephyr/sys/printk.h>

#include "ble_service.hpp"
#include "button_handler.hpp"
#include "effect_engine.hpp"
#include "led_controller.hpp"
#include "power_manager.hpp"
#include "preset_manager.hpp"
#include "preview_manager.hpp"
#include "settings_manager.hpp"

LOG_MODULE_REGISTER(penlight, LOG_LEVEL_DBG);

// Device tree aliases for PWM LEDs
static const struct pwm_dt_spec pwm_r = PWM_DT_SPEC_GET(DT_ALIAS(pwm_led_r));
static const struct pwm_dt_spec pwm_g = PWM_DT_SPEC_GET(DT_ALIAS(pwm_led_g));
static const struct pwm_dt_spec pwm_b = PWM_DT_SPEC_GET(DT_ALIAS(pwm_led_b));
static const struct pwm_dt_spec pwm_w = PWM_DT_SPEC_GET(DT_ALIAS(pwm_led_w));

// Device tree specs for buttons
static const struct gpio_dt_spec button_next_spec = GPIO_DT_SPEC_GET(DT_ALIAS(button0), gpios);
static const struct gpio_dt_spec button_prev_spec = GPIO_DT_SPEC_GET(DT_ALIAS(button1), gpios);

// Global instances
static LEDController led_controller(&pwm_r, &pwm_g, &pwm_b, &pwm_w);
static PresetManager preset_manager;
static PreviewManager preview_manager;
static DualButtonHandler button_handler(&button_next_spec, &button_prev_spec);
static EffectEngine effect_engine;
static PowerManager power_manager(&button_next_spec, &button_prev_spec);

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
static bool check_boot_button_long_press();
static void update_led_display();
static void effect_update_thread_func(void *, void *, void *);
static void on_preset_write(uint8_t preset, const Effect & effect);
static void on_preset_read(uint8_t preset);
static void on_preview_color(const Effect & effect);
static void on_exit_preview();
static uint8_t on_current_preset_read();
static void on_button_pressed(ButtonId button_id, void * user_data);
static void on_button_long_pressed(ButtonId button_id, void * user_data);
static void on_button_pressed_worker(struct k_work * work);
static void on_button_long_pressed_worker(struct k_work * work);

// Store which button was pressed for the work handler
static ButtonId last_button_pressed;
static ButtonId last_button_long_pressed;
static void on_preview_timeout(void * user_data);

K_WORK_DEFINE(button_work, on_button_pressed_worker);
K_WORK_DEFINE(button_long_press_work, on_button_long_pressed_worker);

// Effect update thread (runs every 30ms to update effects)
#define EFFECT_UPDATE_THREAD_STACK_SIZE 1024
#define EFFECT_UPDATE_THREAD_PRIORITY 7
K_THREAD_STACK_DEFINE(effect_update_thread_stack, EFFECT_UPDATE_THREAD_STACK_SIZE);
static struct k_thread effect_update_thread_data;

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

  // Check reset cause
  uint32_t reset_cause = 0;
  int ret = hwinfo_get_reset_cause(&reset_cause);
  if (ret == 0) {
    printk("Reset cause: 0x%08x\n", reset_cause);

    // If waking from deep sleep (RESET_PIN or similar), check if button is still pressed
    if (reset_cause & (RESET_LOW_POWER_WAKE)) {
      printk("Checking boot button state...\n");

      // Check if button is being held (long press to wake up)
      if (!check_boot_button_long_press()) {
        printk("Button not held long enough - returning to sleep\n");

        // Go back to deep sleep
        k_sleep(K_MSEC(100));
        power_manager.power_off();

        // Should never reach here
        return 0;
      }

      printk("Long press detected - booting normally\n");
    }
  }

  preview_manager.init();

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
  k_work_init(&button_long_press_work, on_button_long_pressed_worker);

  // Get singleton instance and set up callbacks
  auto & ble_service = PenlightBLEService::instance();
  ble_service.set_preset_write_callback(on_preset_write);
  ble_service.set_preset_read_callback(on_preset_read);
  ble_service.set_preview_color_callback(on_preview_color);
  ble_service.set_exit_preview_callback(on_exit_preview);
  ble_service.set_current_preset_read_callback(on_current_preset_read);

  button_handler.set_callback(on_button_pressed, nullptr);
  button_handler.set_long_press_callback(on_button_long_pressed, nullptr);
  preview_manager.set_timeout_callback(on_preview_timeout, nullptr);

  // Initialize Bluetooth
  err = bt_enable(bt_ready);
  if (err) {
    printk("Bluetooth init failed (err %d)\n", err);
    return 0;
  }

  // Set initial effect to preset 0
  update_led_display();

  // Start effect update thread
  k_thread_create(
    &effect_update_thread_data, effect_update_thread_stack,
    K_THREAD_STACK_SIZEOF(effect_update_thread_stack), effect_update_thread_func, NULL, NULL, NULL,
    EFFECT_UPDATE_THREAD_PRIORITY, 0, K_NO_WAIT);
  k_thread_name_set(&effect_update_thread_data, "effect_update");

  printk("Penlight initialized successfully\n");

  // Main loop
  while (1) {
    k_sleep(K_MSEC(1000));
  }

  return 0;
}

static void update_led_display()
{
  Effect effect;

  if (preview_manager.is_in_preview_mode()) {
    effect = preview_manager.get_preview_effect();
  } else {
    effect = preset_manager.get_current_effect();
  }

  // Set effect and start the effect engine
  effect_engine.set_effect(effect);
  effect_engine.start();
}

// Effect update thread function - runs every 30ms to update effects
static void effect_update_thread_func(void *, void *, void *)
{
  while (1) {
    // Update effect and get current color
    rgbw_color_t color = effect_engine.update();

    // Set LED color
    led_controller.set_color(color);

    // Wait 30ms before next update
    k_sleep(K_MSEC(30));
  }
}

static void on_preset_write(uint8_t preset, const Effect & effect)
{
  printk("Preset write: preset=%d, mode=%d\n", preset, static_cast<uint8_t>(effect.mode));
  preset_manager.write_preset(preset, effect);
}

static void on_preset_read(uint8_t preset)
{
  printk("Preset read request: preset=%d\n", preset);
  Effect effect = preset_manager.read_preset(preset);
  PenlightBLEService::instance().set_preset_read_data(effect);
  printk("Preset read: mode=%d\n", static_cast<uint8_t>(effect.mode));
}

static void on_preview_color(const Effect & effect)
{
  printk("Preview effect: mode=%d\n", static_cast<uint8_t>(effect.mode));
  preview_manager.enter_preview_mode(effect);
  update_led_display();
}

static void on_exit_preview()
{
  printk("Exit preview mode\n");
  preview_manager.exit_preview_mode();
  update_led_display();
}

static uint8_t on_current_preset_read() { return preset_manager.get_current_preset(); }

static void on_button_pressed(ButtonId button_id, void * user_data)
{
  printk(
    "Button pressed: %s (from callback)\n", button_id == ButtonId::BUTTON_NEXT ? "NEXT" : "PREV");

  // Store which button was pressed
  last_button_pressed = button_id;
  k_work_submit(&button_work);
}

static void on_button_long_pressed(ButtonId button_id, void * user_data)
{
  printk(
    "Button LONG pressed: %s (from callback)\n",
    button_id == ButtonId::BUTTON_NEXT ? "NEXT" : "PREV");

  // Store which button was long pressed
  last_button_long_pressed = button_id;
  k_work_submit(&button_long_press_work);
}

static void on_button_pressed_worker(struct k_work * work)
{
  printk(
    "Button pressed worker: %s\n", last_button_pressed == ButtonId::BUTTON_NEXT ? "NEXT" : "PREV");

  // If in preview mode, exit it
  if (preview_manager.is_in_preview_mode()) {
    preview_manager.exit_preview_mode();
  } else {
    // Otherwise, change preset based on button
    if (last_button_pressed == ButtonId::BUTTON_NEXT) {
      preset_manager.next_preset();
      printk("Next preset: %d\n", preset_manager.get_current_preset());
    } else {
      preset_manager.prev_preset();
      printk("Prev preset: %d\n", preset_manager.get_current_preset());
    }
  }

  update_led_display();

  // Notify BLE clients of the new preset
  PenlightBLEService::instance().notify_current_preset(preset_manager.get_current_preset());
}

static void on_button_long_pressed_worker(struct k_work * work)
{
  printk(
    "Button LONG pressed worker: %s\n",
    last_button_long_pressed == ButtonId::BUTTON_NEXT ? "NEXT" : "PREV");

  // Only NEXT button long press triggers power off
  if (last_button_long_pressed == ButtonId::BUTTON_NEXT) {
    printk("Powering off device...\n");

    // Turn off all LEDs before sleep
    led_controller.turn_off();
    button_handler.disable_interrupts();
    bt_disable();

    // Small delay to ensure LED is off
    k_sleep(K_MSEC(100));

    // Enter deep sleep
    power_manager.power_off();

    // This line should never be reached (device is in deep sleep)
  }
}

static void on_preview_timeout(void * user_data)
{
  printk("Preview timeout\n");
  update_led_display();
}

// Check if button is held long enough during boot (for wake from deep sleep)
static bool check_boot_button_long_press()
{
  // Configure button as input temporarily
  int ret = gpio_pin_configure_dt(&button_next_spec, GPIO_INPUT);
  if (ret < 0) {
    printk("Failed to configure button for boot check: %d\n", ret);
    return false;
  }

  // Wait a bit and check if button is still pressed
  // If button is held for at least 500ms, consider it a valid long press
  const int check_duration_ms = 500;
  const int check_interval_ms = 50;
  int checks_passed = 0;
  int total_checks = check_duration_ms / check_interval_ms;

  for (int i = 0; i < total_checks; i++) {
    bool is_pressed = gpio_pin_get_dt(&button_next_spec) != 0;
    if (is_pressed) {
      checks_passed++;
    }
    k_sleep(K_MSEC(check_interval_ms));
  }

  // Button must be held for at least 80% of the check duration
  bool long_press_detected = (checks_passed >= (total_checks * 8 / 10));

  printk(
    "Boot button check: %d/%d checks passed -> %s\n", checks_passed, total_checks,
    long_press_detected ? "LONG PRESS" : "SHORT PRESS");

  return long_press_detected;
}

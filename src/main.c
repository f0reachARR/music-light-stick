/*
 * Penlight Main Application
 *
 * RGBW LED Penlight with BLE control
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "ble_service.h"
#include "button.h"
#include "effect_engine.h"
#include "led_controller.h"
#include "power_manager.h"
#include "preset_manager.h"
#include "preview_manager.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

/* アプリケーション状態 */
enum app_state {
  APP_STATE_NORMAL,
  APP_STATE_PREVIEW,
  APP_STATE_SLEEP,
};

static enum app_state current_state = APP_STATE_NORMAL;

/* 現在のプリセットエフェクトを開始 */
static void start_current_preset(void)
{
  struct preset_data preset;
  uint8_t index = preset_get_current_index();

  if (preset_get(index, &preset) == 0) {
    effect_engine_set(&preset);
    LOG_INF("Started preset %d", index);
  }
}

/* プレビュー終了コールバック */
static void on_preview_exit(void)
{
  LOG_INF("Preview ended, returning to normal mode");
  current_state = APP_STATE_NORMAL;
  start_current_preset();
}

/* ボタンイベントコールバック */
static void on_button_event(enum button_id btn, enum button_event evt)
{
  LOG_INF("Button %d event: %s", btn, evt == BUTTON_EVT_SHORT_PRESS ? "short" : "long");

  /* プレビューモード中はボタンでプレビュー終了 */
  if (preview_is_active()) {
    preview_exit();
    return;
  }

  switch (evt) {
    case BUTTON_EVT_SHORT_PRESS:
      if (current_state == APP_STATE_NORMAL) {
        uint8_t new_index;

        if (btn == BUTTON_NEXT) {
          new_index = preset_next();
        } else {
          new_index = preset_prev();
        }

        /* プリセット変更 */
        start_current_preset();

        /* BLE通知 */
        ble_notify_current_preset(new_index);

        LOG_INF("Preset changed to %d", new_index);
      }
      break;

    case BUTTON_EVT_LONG_PRESS:
      if (btn == BUTTON_NEXT) {
        if (current_state == APP_STATE_NORMAL) {
          /* スリープモードへ移行 */
          LOG_INF("Entering sleep mode");
          current_state = APP_STATE_SLEEP;
          effect_engine_stop();
          ble_stop_advertising();
          power_enter_sleep();
          /* Note: power_enter_sleep()から戻らない */
        }
      }
      break;
  }
}

/* 初期化処理 */
static int app_init(void)
{
  int ret;

  LOG_INF("Penlight initializing...");

  /* Power Manager初期化 */
  ret = power_manager_init();
  if (ret < 0) {
    LOG_ERR("Power manager init failed: %d", ret);
    return ret;
  }

  /* Deep Sleep復帰時の長押し判定 */
  if (power_is_wakeup_from_sleep()) {
    LOG_INF("Wakeup from sleep detected, checking button...");
    if (!power_check_wakeup_condition()) {
      /* 長押し確認失敗、再スリープへ (この行には到達しない) */
      return 0;
    }
    LOG_INF("Wakeup confirmed, continuing boot");
  }

  /* LED Controller初期化 */
  ret = led_controller_init();
  if (ret < 0) {
    LOG_ERR("LED controller init failed: %d", ret);
    return ret;
  }

  /* Button Handler初期化 */
  ret = button_init();
  if (ret < 0) {
    LOG_ERR("Button handler init failed: %d", ret);
    return ret;
  }
  button_set_callback(on_button_event);

  /* Effect Engine初期化 */
  ret = effect_engine_init();
  if (ret < 0) {
    LOG_ERR("Effect engine init failed: %d", ret);
    return ret;
  }

  /* Preset Manager初期化 */
  ret = preset_manager_init();
  if (ret < 0) {
    LOG_ERR("Preset manager init failed: %d", ret);
    return ret;
  }

  /* Preview Manager初期化 */
  ret = preview_manager_init();
  if (ret < 0) {
    LOG_ERR("Preview manager init failed: %d", ret);
    return ret;
  }
  preview_set_exit_callback(on_preview_exit);

  /* BLE Service初期化 */
  ret = ble_service_init();
  if (ret < 0) {
    LOG_ERR("BLE service init failed: %d", ret);
    return ret;
  }

  LOG_INF("Penlight initialized");
  return 0;
}

int main(void)
{
  int ret;

  LOG_INF("Penlight starting...");

  /* アプリケーション初期化 */
  ret = app_init();
  if (ret < 0) {
    LOG_ERR("Application init failed: %d", ret);
    return ret;
  }

  /* BLE広告開始 */
  ret = ble_start_advertising();
  if (ret < 0) {
    LOG_ERR("BLE advertising start failed: %d", ret);
    /* 継続する (BLEなしでも動作可能) */
  }

  /* バッテリー監視開始 */
  power_start_battery_monitor();

  /* 初期プリセットでエフェクト開始 */
  current_state = APP_STATE_NORMAL;
  start_current_preset();

  LOG_INF("Penlight started, preset=%d", preset_get_current_index());

  /* メインループ (Zephyrのワークキューで処理されるため、ここでは何もしない) */
  while (1) {
    k_sleep(K_FOREVER);
  }

  return 0;
}

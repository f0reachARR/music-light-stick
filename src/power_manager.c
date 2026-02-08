/*
 * Power Manager - 電力管理
 */

#include "power_manager.h"

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/hwinfo.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/pm/device.h>
#include <zephyr/pm/device_runtime.h>
#include <zephyr/pm/pm.h>
#include <zephyr/sys/poweroff.h>

#include "button.h"
#include "led_controller.h"

LOG_MODULE_REGISTER(power_manager, LOG_LEVEL_DBG);

/* バッテリー監視間隔 (1分) */
#define BATTERY_MONITOR_INTERVAL_MS 60000

/* 長押し判定時間 */
#define WAKEUP_LONG_PRESS_MS 3000

/* 長押し確認ポーリング間隔 */
#define WAKEUP_POLL_INTERVAL_MS 50

/* 状態 */
struct power_state
{
  bool sleeping;
  bool wakeup_from_sleep;
  uint8_t battery_level;
  power_wakeup_callback_t wakeup_callback;
  struct k_work_delayable battery_work;
};

static struct power_state state = {};

/* NEXT button for wakeup */
static const struct gpio_dt_spec wakeup_button = GPIO_DT_SPEC_GET(DT_ALIAS(button_next), gpios);
#ifdef CONFIG_STDOUT_CONSOLE
static const struct device * const cons = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
#endif

/* リセット原因を保存 */
static uint32_t reset_cause;

/* バッテリー電圧読み取り (ダミー実装) */
static uint8_t read_battery_level(void)
{
  /* TODO: 実際のADC実装を追加 */
  /* 現在はダミー値を返す */
  return 100;
}

/* バッテリー監視ワークハンドラ */
static void battery_work_handler(struct k_work * work)
{
  state.battery_level = read_battery_level();
  LOG_DBG("Battery level: %d%%", state.battery_level);

  /* 次回監視をスケジュール */
  k_work_schedule(&state.battery_work, K_MSEC(BATTERY_MONITOR_INTERVAL_MS));
}

int power_manager_init(void)
{
  memset(&state, 0, sizeof(state));
  state.battery_level = 100;

  k_work_init_delayable(&state.battery_work, battery_work_handler);

  /* リセット原因を取得 */
  hwinfo_get_reset_cause(&reset_cause);

  /* GPIO/ピンリセットによる復帰かチェック */
  if (reset_cause & (RESET_PIN | RESET_DEBUG)) {
    state.wakeup_from_sleep = false;
  } else if (reset_cause & RESET_LOW_POWER_WAKE) {
    state.wakeup_from_sleep = true;
    LOG_INF("Wakeup from deep sleep detected");
  } else {
    state.wakeup_from_sleep = false;
  }

  LOG_INF("Power Manager initialized, reset_cause=0x%08x", reset_cause);
  return 0;
}

void power_set_wakeup_callback(power_wakeup_callback_t cb) { state.wakeup_callback = cb; }

void power_enter_sleep(void)
{
  LOG_INF("Entering sleep mode");

  state.sleeping = true;

  /* LED消灯 */
  led_off();

  /* バッテリー監視停止 */
  power_stop_battery_monitor();

  /* Wakeup button設定 */
  if (gpio_is_ready_dt(&wakeup_button)) {
    /* GPIOをwakeupソースとして設定 */
    gpio_pin_configure_dt(&wakeup_button, GPIO_INPUT);
    gpio_pin_interrupt_configure_dt(&wakeup_button, GPIO_INT_LEVEL_HIGH);
  }

  LOG_INF("System power off");

  /* Deep Sleepへ移行 */
#ifdef CONFIG_STDOUT_CONSOLE
  pm_device_action_run(cons, PM_DEVICE_ACTION_SUSPEND);
#endif

  hwinfo_clear_reset_cause();
  sys_poweroff();
}

bool power_is_wakeup_from_sleep(void) { return state.wakeup_from_sleep; }

bool power_check_wakeup_condition(void)
{
  if (!state.wakeup_from_sleep) {
    /* 通常起動の場合はそのまま継続 */
    return true;
  }

  LOG_INF("Checking wakeup button long press...");

  /* GPIOを初期化 */
  if (!gpio_is_ready_dt(&wakeup_button)) {
    LOG_ERR("Wakeup button GPIO not ready");
    return true; /* エラー時は通常起動として扱う */
  }

  gpio_pin_configure_dt(&wakeup_button, GPIO_INPUT);

  /* ボタンが押されているか確認 */
  int pressed = gpio_pin_get_dt(&wakeup_button);
  if (!pressed) {
    /* ボタンが押されていない場合、再スリープ */
    LOG_INF("Button not pressed, returning to sleep");
    power_enter_sleep();
    /* 戻らない */
    return false;
  }

  /* 長押し判定: WAKEUP_LONG_PRESS_MS の間ボタンが押され続けているか確認 */
  int64_t start_time = k_uptime_get();
  int64_t elapsed = 0;

  while (elapsed < WAKEUP_LONG_PRESS_MS) {
    k_sleep(K_MSEC(WAKEUP_POLL_INTERVAL_MS));

    pressed = gpio_pin_get_dt(&wakeup_button);
    if (!pressed) {
      /* ボタンが離された場合、再スリープ */
      LOG_INF("Button released before long press, returning to sleep");
      power_enter_sleep();
      /* 戻らない */
      return false;
    }

    elapsed = k_uptime_get() - start_time;
  }

  /* 長押し確認完了 */
  LOG_INF("Wakeup long press confirmed");
  state.wakeup_from_sleep = false; /* フラグをクリア */
  return true;
}

uint8_t power_get_battery_level(void) { return state.battery_level; }

void power_start_battery_monitor(void)
{
  /* 初回バッテリー読み取り */
  state.battery_level = read_battery_level();

  k_work_schedule(&state.battery_work, K_MSEC(BATTERY_MONITOR_INTERVAL_MS));
  LOG_INF("Battery monitor started");
}

void power_stop_battery_monitor(void)
{
  k_work_cancel_delayable(&state.battery_work);
  LOG_INF("Battery monitor stopped");
}

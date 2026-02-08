/*
 * Button Handler - ボタン入力処理
 */

#include "button.h"

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(button, LOG_LEVEL_DBG);

/* 定数定義 */
#define DEBOUNCE_MS 50
#define LONG_PRESS_MS 3000

/* GPIO button definitions from device tree */
static const struct gpio_dt_spec button_next = GPIO_DT_SPEC_GET(DT_ALIAS(button_next), gpios);
static const struct gpio_dt_spec button_prev = GPIO_DT_SPEC_GET(DT_ALIAS(button_prev), gpios);

/* ボタン状態 */
struct button_state
{
  const struct gpio_dt_spec * gpio;
  struct gpio_callback cb_data;
  struct k_work_delayable debounce_work;
  struct k_work_delayable long_press_work;
  enum button_id id;
  bool pressed;
  int64_t press_time;
};

static struct button_state buttons[BUTTON_COUNT] = {};
static button_callback_t user_callback;

/* デバウンス後のボタン処理 */
static void debounce_work_handler(struct k_work * work)
{
  struct k_work_delayable * dwork = k_work_delayable_from_work(work);
  struct button_state * btn = CONTAINER_OF(dwork, struct button_state, debounce_work);

  bool current_state = gpio_pin_get_dt(btn->gpio);

  if (current_state && !btn->pressed) {
    /* ボタン押下開始 */
    btn->press_time = k_uptime_get();
    /* 長押し検出タイマー開始 */
    k_work_schedule(&btn->long_press_work, K_MSEC(LONG_PRESS_MS));
    LOG_INF("Button %d pressed", btn->id);
  } else if (!current_state && btn->pressed) {
    /* ボタン離された */
    k_work_cancel_delayable(&btn->long_press_work);

    int64_t press_duration = k_uptime_get() - btn->press_time;
    if (press_duration < LONG_PRESS_MS) {
      /* 短押しイベント */
      LOG_INF("Button %d short press", btn->id);
      if (user_callback) {
        user_callback(btn->id, BUTTON_EVT_SHORT_PRESS);
      }
    }
  }

  btn->pressed = current_state;
}

/* 長押し検出ハンドラ */
static void long_press_work_handler(struct k_work * work)
{
  struct k_work_delayable * dwork = k_work_delayable_from_work(work);
  struct button_state * btn = CONTAINER_OF(dwork, struct button_state, long_press_work);

  if (btn->pressed) {
    LOG_INF("Button %d long press", btn->id);
    if (user_callback) {
      user_callback(btn->id, BUTTON_EVT_LONG_PRESS);
    }
  }
}

/* GPIO割り込みコールバック */
static void gpio_callback_handler(
  const struct device * dev, struct gpio_callback * cb, uint32_t pins)
{
  struct button_state * btn = CONTAINER_OF(cb, struct button_state, cb_data);
  k_work_schedule(&btn->debounce_work, K_MSEC(DEBOUNCE_MS));
}

static int button_setup(
  struct button_state * btn, const struct gpio_dt_spec * gpio, enum button_id id)
{
  int ret;

  btn->gpio = gpio;
  btn->id = id;
  btn->pressed = false;

  if (!gpio_is_ready_dt(gpio)) {
    LOG_ERR("Button %d GPIO not ready", id);
    return -ENODEV;
  }

  ret = gpio_pin_configure_dt(gpio, GPIO_INPUT);
  if (ret < 0) {
    LOG_ERR("Button %d configure failed: %d", id, ret);
    return ret;
  }

  ret = gpio_pin_interrupt_configure_dt(gpio, GPIO_INT_EDGE_BOTH);
  if (ret < 0) {
    LOG_ERR("Button %d interrupt configure failed: %d", id, ret);
    return ret;
  }

  gpio_init_callback(&btn->cb_data, gpio_callback_handler, BIT(gpio->pin));
  ret = gpio_add_callback(gpio->port, &btn->cb_data);
  if (ret < 0) {
    LOG_ERR("Button %d add callback failed: %d", id, ret);
    return ret;
  }

  k_work_init_delayable(&btn->debounce_work, debounce_work_handler);
  k_work_init_delayable(&btn->long_press_work, long_press_work_handler);

  return 0;
}

int button_init(void)
{
  int ret;

  ret = button_setup(&buttons[BUTTON_NEXT], &button_next, BUTTON_NEXT);
  if (ret < 0) {
    return ret;
  }

  ret = button_setup(&buttons[BUTTON_PREV], &button_prev, BUTTON_PREV);
  if (ret < 0) {
    return ret;
  }

  LOG_INF("Button handler initialized");
  return 0;
}

void button_set_callback(button_callback_t cb) { user_callback = cb; }

bool button_is_pressed(enum button_id btn)
{
  if (btn >= BUTTON_COUNT) {
    return false;
  }
  return buttons[btn].pressed;
}

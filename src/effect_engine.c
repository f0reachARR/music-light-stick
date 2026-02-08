/*
 * Effect Engine - エフェクト実行エンジン
 */

#include "effect_engine.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "color_utils.h"
#include "led_controller.h"

LOG_MODULE_REGISTER(effect_engine, LOG_LEVEL_DBG);

/* 基本更新間隔 */
#define BASE_INTERVAL_MS 1000
#define MIN_INTERVAL_MS 10
#define MAX_INTERVAL_MS 100

/* エフェクト状態 */
struct effect_state
{
  struct preset_data current;
  bool running;

  /* アニメーション状態 */
  uint16_t hue;        /* Rainbow: 現在のHue (0-359) */
  uint8_t gradient_t;  /* Gradient: 補間係数 (0-255) */
  int8_t gradient_dir; /* Gradient: 方向 (1 or -1) */
  bool blink_on;       /* Blink: 点灯状態 */

  struct k_work_delayable update_work;
};

static struct effect_state state = {};

/* 固定色エフェクト処理 */
static void effect_solid_update(void)
{
  const struct effect_solid * e = &state.current.data.solid;
  led_set_rgbw(e->r, e->g, e->b, e->w);
}

/* レインボーエフェクト処理 */
static void effect_rainbow_update(void)
{
  const struct effect_rainbow * e = &state.current.data.rainbow;

  struct rgb_color rgb = hsv_to_rgb(state.hue, 255, e->brightness);
  led_set_rgbw(rgb.r, rgb.g, rgb.b, 0);

  /* Hue更新 */
  state.hue = (state.hue + 1) % 360;
}

/* グラデーションエフェクト処理 */
static void effect_gradient_update(void)
{
  const struct effect_gradient * e = &state.current.data.gradient;

  struct rgbw_color c1 = {e->r1, e->g1, e->b1, e->w1};
  struct rgbw_color c2 = {e->r2, e->g2, e->b2, e->w2};

  struct rgbw_color result = rgbw_lerp(&c1, &c2, state.gradient_t);
  led_set_rgbw(result.r, result.g, result.b, result.w);

  /* 補間係数更新 (往復) */
  int16_t new_t = (int16_t)state.gradient_t + state.gradient_dir * 3;
  if (new_t >= 255) {
    new_t = 255;
    state.gradient_dir = -1;
  } else if (new_t <= 0) {
    new_t = 0;
    state.gradient_dir = 1;
  }
  state.gradient_t = (uint8_t)new_t;
}

/* 点滅エフェクト処理 */
static void effect_blink_update(void)
{
  const struct effect_blink * e = &state.current.data.blink;

  if (state.blink_on) {
    led_set_rgbw(e->r, e->g, e->b, e->w);
  } else {
    led_off();
  }

  state.blink_on = !state.blink_on;
}

/* 更新間隔計算 */
static uint32_t calculate_interval(void)
{
  uint8_t speed = 1;

  switch (state.current.mode) {
    case EFFECT_MODE_SOLID:
      return 0; /* 更新不要 */
    case EFFECT_MODE_RAINBOW:
      speed = state.current.data.rainbow.speed;
      break;
    case EFFECT_MODE_GRADIENT:
      speed = state.current.data.gradient.speed;
      break;
    case EFFECT_MODE_BLINK:
      /* Period × 100ms / 2 = 半周期 */
      return (state.current.data.blink.period * 100) / 2;
    default:
      return 0;
  }

  if (speed == 0) {
    speed = 1;
  }

  /* interval = BASE_INTERVAL / speed */
  uint32_t interval = BASE_INTERVAL_MS / speed;
  if (interval < MIN_INTERVAL_MS) {
    interval = MIN_INTERVAL_MS;
  } else if (interval > MAX_INTERVAL_MS) {
    interval = MAX_INTERVAL_MS;
  }

  return interval;
}

/* エフェクト更新ワークハンドラ */
static void effect_update_work_handler(struct k_work * work)
{
  if (!state.running) {
    return;
  }

  switch (state.current.mode) {
    case EFFECT_MODE_SOLID:
      effect_solid_update();
      break;
    case EFFECT_MODE_RAINBOW:
      effect_rainbow_update();
      break;
    case EFFECT_MODE_GRADIENT:
      effect_gradient_update();
      break;
    case EFFECT_MODE_BLINK:
      effect_blink_update();
      break;
    default:
      led_off();
      return;
  }

  /* 次回更新スケジュール */
  uint32_t interval = calculate_interval();
  if (interval > 0) {
    k_work_schedule(&state.update_work, K_MSEC(interval));
  }
}

int effect_engine_init(void)
{
  memset(&state, 0, sizeof(state));
  state.gradient_dir = 1;

  k_work_init_delayable(&state.update_work, effect_update_work_handler);

  LOG_INF("Effect Engine initialized");
  return 0;
}

void effect_engine_set(const struct preset_data * preset)
{
  /* 現在のエフェクトを停止 */
  k_work_cancel_delayable(&state.update_work);

  /* 新しいエフェクトを設定 */
  memcpy(&state.current, preset, sizeof(struct preset_data));

  /* アニメーション状態をリセット */
  state.hue = 0;
  state.gradient_t = 0;
  state.gradient_dir = 1;
  state.blink_on = true;
  state.running = true;

  LOG_INF("Effect set: mode=%d", preset->mode);

  /* 即座に最初の更新を実行 */
  effect_update_work_handler(NULL);
}

void effect_engine_stop(void)
{
  state.running = false;
  k_work_cancel_delayable(&state.update_work);
  led_off();
  LOG_INF("Effect stopped");
}

bool effect_engine_is_running(void) { return state.running; }

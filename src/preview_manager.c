/*
 * Preview Manager - プレビューモード管理
 */

#include "preview_manager.h"

#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "effect_engine.h"

LOG_MODULE_REGISTER(preview_manager, LOG_LEVEL_DBG);

/* プレビュータイムアウト (30秒) */
#define PREVIEW_TIMEOUT_MS 30000

/* プレビュー状態 */
struct preview_state
{
  bool active;
  struct preset_data current_preview;
  struct k_work_delayable timeout_work;
  preview_exit_callback_t exit_callback;
};

static struct preview_state state = {};

/* タイムアウトハンドラ */
static void preview_timeout_handler(struct k_work * work)
{
  if (state.active) {
    LOG_INF("Preview timeout, exiting preview mode");
    preview_exit();
  }
}

int preview_manager_init(void)
{
  memset(&state, 0, sizeof(state));

  k_work_init_delayable(&state.timeout_work, preview_timeout_handler);

  LOG_INF("Preview Manager initialized");
  return 0;
}

void preview_set_exit_callback(preview_exit_callback_t cb) { state.exit_callback = cb; }

void preview_start(const struct preset_data * preset)
{
  if (preset == NULL) {
    return;
  }

  /* プレビューモードに入る */
  state.active = true;
  memcpy(&state.current_preview, preset, sizeof(struct preset_data));

  /* エフェクトを開始 */
  effect_engine_set(preset);

  /* タイムアウトタイマーを開始/リセット */
  k_work_reschedule(&state.timeout_work, K_MSEC(PREVIEW_TIMEOUT_MS));

  LOG_INF("Preview started: mode=%d", preset->mode);
}

void preview_exit(void)
{
  if (!state.active) {
    return;
  }

  /* タイムアウトタイマーをキャンセル */
  k_work_cancel_delayable(&state.timeout_work);

  state.active = false;

  LOG_INF("Preview exited");

  /* コールバックを呼び出し (通常モードに復帰) */
  if (state.exit_callback) {
    state.exit_callback();
  }
}

void preview_reset_timeout(void)
{
  if (state.active) {
    k_work_reschedule(&state.timeout_work, K_MSEC(PREVIEW_TIMEOUT_MS));
  }
}

bool preview_is_active(void) { return state.active; }

int preview_get_current(struct preset_data * preset)
{
  if (!state.active || preset == NULL) {
    return -EINVAL;
  }

  memcpy(preset, &state.current_preview, sizeof(struct preset_data));
  return 0;
}

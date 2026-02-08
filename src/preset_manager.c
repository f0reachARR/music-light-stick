/*
 * Preset Manager - プリセット永続化管理
 */

#include "preset_manager.h"

#include <stdio.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>

#include "effect_types.h"

LOG_MODULE_REGISTER(preset_manager, LOG_LEVEL_INF);

/* プリセットデータ保存用 */
static struct preset_data presets[PRESET_COUNT] = {};
static bool presets_modified[PRESET_COUNT] = {};
static uint8_t current_preset_index = 0;

/* Settings handler */
static int preset_settings_set(
  const char * name, size_t len, settings_read_cb read_cb, void * cb_arg)
{
  const char * next;
  int rc;

  LOG_INF("Loading setting: %s (len=%zu)", name, len);
  /* Parse preset index from name (e.g., "p0", "p1", ..., "p19") */
  if (name[0] == 'p') {
    int index = atoi(&name[1]);
    if (index >= 0 && index < PRESET_COUNT) {
      if (len > sizeof(struct preset_data)) {
        return -EINVAL;
      }
      rc = read_cb(cb_arg, &presets[index], len);
      if (rc < 0) {
        return rc;
      }
      return 0;
    }
  }

  return -ENOENT;
}

static int preset_settings_export(int (*cb)(const char * name, const void * value, size_t val_len))
{
  static char name[16] = {0};
  int rc;

  for (int i = 0; i < PRESET_COUNT; i++) {
    if (!presets_modified[i]) {
      continue;
    }
    snprintf(name, sizeof(name), "penlight/p%d", i);
    LOG_INF("Exporting preset %d", i);
    rc = cb(name, &presets[i], sizeof(struct preset_data));
    if (rc) {
      return rc;
    }

    presets_modified[i] = false;
  }

  return 0;
}

SETTINGS_STATIC_HANDLER_DEFINE(
  penlight, "penlight", NULL, preset_settings_set, NULL, preset_settings_export);

/* デフォルトプリセット初期化 */
static void init_default_presets(void)
{
  /* プリセット0: 赤固定色 */
  presets[0].mode = EFFECT_MODE_SOLID;
  presets[0].data.solid.r = 255;
  presets[0].data.solid.g = 0;
  presets[0].data.solid.b = 0;
  presets[0].data.solid.w = 0;

  /* プリセット1: 緑固定色 */
  presets[1].mode = EFFECT_MODE_SOLID;
  presets[1].data.solid.r = 0;
  presets[1].data.solid.g = 255;
  presets[1].data.solid.b = 0;
  presets[1].data.solid.w = 0;

  /* プリセット2: 青固定色 */
  presets[2].mode = EFFECT_MODE_SOLID;
  presets[2].data.solid.r = 0;
  presets[2].data.solid.g = 0;
  presets[2].data.solid.b = 255;
  presets[2].data.solid.w = 0;

  /* プリセット3: 白固定色 */
  presets[3].mode = EFFECT_MODE_SOLID;
  presets[3].data.solid.r = 0;
  presets[3].data.solid.g = 0;
  presets[3].data.solid.b = 0;
  presets[3].data.solid.w = 255;

  /* プリセット4: レインボー */
  presets[4].mode = EFFECT_MODE_RAINBOW;
  presets[4].data.rainbow.speed = 128;
  presets[4].data.rainbow.brightness = 255;

  /* プリセット5: 赤→青グラデーション */
  presets[5].mode = EFFECT_MODE_GRADIENT;
  presets[5].data.gradient.r1 = 255;
  presets[5].data.gradient.g1 = 0;
  presets[5].data.gradient.b1 = 0;
  presets[5].data.gradient.w1 = 0;
  presets[5].data.gradient.r2 = 0;
  presets[5].data.gradient.g2 = 0;
  presets[5].data.gradient.b2 = 255;
  presets[5].data.gradient.w2 = 0;
  presets[5].data.gradient.speed = 64;

  /* プリセット6: 赤点滅 */
  presets[6].mode = EFFECT_MODE_BLINK;
  presets[6].data.blink.r = 255;
  presets[6].data.blink.g = 0;
  presets[6].data.blink.b = 0;
  presets[6].data.blink.w = 0;
  presets[6].data.blink.period = 10; /* 1秒周期 */

  /* プリセット7-19: 白固定色 (デフォルト) */
  for (int i = 7; i < PRESET_COUNT; i++) {
    presets[i].mode = EFFECT_MODE_SOLID;
    presets[i].data.solid.r = 0;
    presets[i].data.solid.g = 0;
    presets[i].data.solid.b = 0;
    presets[i].data.solid.w = 0;
  }
}

int preset_manager_init(void)
{
  int rc;

  /* デフォルト値で初期化 */
  init_default_presets();
  current_preset_index = 0;

  /* 保存済みデータを読み込み */
  rc = settings_subsys_init();
  if (rc) {
    LOG_ERR("Settings init failed: %d", rc);
    return rc;
  }

  rc = settings_load();
  if (rc) {
    LOG_ERR("Settings load failed: %d", rc);
    return rc;
  }

  LOG_INF("Preset Manager initialized, current=%d", current_preset_index);
  return 0;
}

int preset_get(uint8_t index, struct preset_data * preset)
{
  if (index >= PRESET_COUNT || preset == NULL) {
    return -EINVAL;
  }

  memcpy(preset, &presets[index], sizeof(struct preset_data));
  return 0;
}

int preset_set(uint8_t index, const struct preset_data * preset)
{
  if (index >= PRESET_COUNT || preset == NULL) {
    return -EINVAL;
  }

  memcpy(&presets[index], preset, sizeof(struct preset_data));
  presets_modified[index] = true;
  LOG_INF("Preset %d set: mode=%d", index, preset->mode);
  return 0;
}

int preset_save_all(void)
{
  int rc;

  rc = settings_save();
  if (rc) {
    LOG_ERR("Settings save failed: %d", rc);
    return rc;
  }

  LOG_INF("All presets saved");
  return 0;
}

uint8_t preset_get_current_index(void) { return current_preset_index; }

void preset_set_current_index(uint8_t index)
{
  if (index < PRESET_COUNT) {
    current_preset_index = index;
  }
}

bool preset_is_zero(uint8_t index)
{
  if (index >= PRESET_COUNT) {
    return false;
  }

  const struct preset_data * p = &presets[index];
  if (
    p->mode == EFFECT_MODE_SOLID && p->data.solid.r == 0 && p->data.solid.g == 0 &&
    p->data.solid.b == 0 && p->data.solid.w == 0) {
    return true;
  }

  return false;
}

uint8_t preset_next(void)
{
  while (true) {
    current_preset_index = (current_preset_index + 1) % PRESET_COUNT;
    if (!preset_is_zero(current_preset_index)) {
      break;
    }
  }
  return current_preset_index;
}

uint8_t preset_prev(void)
{
  while (true) {
    if (current_preset_index == 0) {
      current_preset_index = PRESET_COUNT - 1;
    } else {
      current_preset_index--;
    }
    if (!preset_is_zero(current_preset_index)) {
      break;
    }
  }
  return current_preset_index;
}

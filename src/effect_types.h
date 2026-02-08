/*
 * Effect Types - エフェクトデータ型定義
 */

#ifndef EFFECT_TYPES_H
#define EFFECT_TYPES_H

#include <stdbool.h>
#include <stdint.h>

/* プリセット数 */
#define PRESET_COUNT 20

/* エフェクトモードID */
enum effect_mode {
  EFFECT_MODE_SOLID = 0x00,    /* 固定色 */
  EFFECT_MODE_RAINBOW = 0x01,  /* レインボー */
  EFFECT_MODE_GRADIENT = 0x02, /* グラデーション */
  EFFECT_MODE_BLINK = 0x03,    /* 点滅 */
};

/* 固定色エフェクトデータ */
struct effect_solid
{
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t w;
};

/* レインボーエフェクトデータ */
struct effect_rainbow
{
  uint8_t speed;      /* 変化速度 (1-255) */
  uint8_t brightness; /* 明るさ (0-255) */
};

/* グラデーションエフェクトデータ */
struct effect_gradient
{
  uint8_t r1, g1, b1, w1; /* 開始色 */
  uint8_t r2, g2, b2, w2; /* 終了色 */
  uint8_t speed;          /* 変化速度 (1-255) */
};

/* 点滅エフェクトデータ */
struct effect_blink
{
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t w;
  uint8_t period; /* 点滅周期 (1-255, 単位:100ms) */
};

/* エフェクトデータ共用体 */
union effect_data {
  struct effect_solid solid;
  struct effect_rainbow rainbow;
  struct effect_gradient gradient;
  struct effect_blink blink;
};

/* プリセットデータ構造体 */
struct preset_data
{
  uint8_t mode; /* effect_mode */
  union effect_data data;
};

/* 最大プリセットデータサイズ (mode + gradient = 1 + 9 = 10 bytes) */
#define PRESET_DATA_MAX_SIZE 10

#endif /* EFFECT_TYPES_H */

/*
 * Color Utilities - 色変換ユーティリティ
 */

#ifndef COLOR_UTILS_H
#define COLOR_UTILS_H

#include <stdint.h>

/* RGB構造体 */
struct rgb_color {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

/* RGBW構造体 */
struct rgbw_color {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t w;
};

/**
 * @brief HSVからRGBに変換
 * @param h 色相 (0-359)
 * @param s 彩度 (0-255)
 * @param v 明度 (0-255)
 * @return RGB値
 */
struct rgb_color hsv_to_rgb(uint16_t h, uint8_t s, uint8_t v);

/**
 * @brief 2つのRGBW色を線形補間
 * @param c1 開始色
 * @param c2 終了色
 * @param t 補間係数 (0-255, 0=c1, 255=c2)
 * @return 補間された色
 */
struct rgbw_color rgbw_lerp(const struct rgbw_color *c1,
                            const struct rgbw_color *c2,
                            uint8_t t);

#endif /* COLOR_UTILS_H */

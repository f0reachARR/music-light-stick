/*
 * Color Utilities - 色変換ユーティリティ
 */

#include "color_utils.h"

struct rgb_color hsv_to_rgb(uint16_t h, uint8_t s, uint8_t v)
{
    struct rgb_color rgb;

    if (s == 0) {
        /* 彩度0は無彩色 (白/グレー) */
        rgb.r = v;
        rgb.g = v;
        rgb.b = v;
        return rgb;
    }

    /* h: 0-359, 60度ごとにセクタ分割 */
    h = h % 360;
    uint8_t sector = h / 60;
    uint8_t pos = h % 60;

    /* 中間値計算 */
    uint8_t p = (v * (255 - s)) / 255;
    uint8_t q = (v * (255 - (s * pos) / 60)) / 255;
    uint8_t t = (v * (255 - (s * (60 - pos)) / 60)) / 255;

    switch (sector) {
    case 0:
        rgb.r = v;
        rgb.g = t;
        rgb.b = p;
        break;
    case 1:
        rgb.r = q;
        rgb.g = v;
        rgb.b = p;
        break;
    case 2:
        rgb.r = p;
        rgb.g = v;
        rgb.b = t;
        break;
    case 3:
        rgb.r = p;
        rgb.g = q;
        rgb.b = v;
        break;
    case 4:
        rgb.r = t;
        rgb.g = p;
        rgb.b = v;
        break;
    default: /* sector 5 */
        rgb.r = v;
        rgb.g = p;
        rgb.b = q;
        break;
    }

    return rgb;
}

struct rgbw_color rgbw_lerp(const struct rgbw_color *c1,
                            const struct rgbw_color *c2,
                            uint8_t t)
{
    struct rgbw_color result;

    /* Linear interpolation: result = c1 + (c2 - c1) * t / 255 */
    result.r = c1->r + ((int16_t)(c2->r - c1->r) * t) / 255;
    result.g = c1->g + ((int16_t)(c2->g - c1->g) * t) / 255;
    result.b = c1->b + ((int16_t)(c2->b - c1->b) * t) / 255;
    result.w = c1->w + ((int16_t)(c2->w - c1->w) * t) / 255;

    return result;
}

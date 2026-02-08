/*
 * LED Controller - RGBW PWM LED制御
 */

#ifndef LED_CONTROLLER_H
#define LED_CONTROLLER_H

#include <stdint.h>

/**
 * @brief LED Controllerを初期化
 * @return 0: 成功, 負値: エラー
 */
int led_controller_init(void);

/**
 * @brief RGBW値でLEDを設定
 * @param r 赤 (0-255)
 * @param g 緑 (0-255)
 * @param b 青 (0-255)
 * @param w 白 (0-255)
 */
void led_set_rgbw(uint8_t r, uint8_t g, uint8_t b, uint8_t w);

/**
 * @brief LEDを消灯
 */
void led_off(void);

#endif /* LED_CONTROLLER_H */

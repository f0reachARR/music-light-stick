/*
 * LED Controller - RGBW PWM LED制御
 */

#include "led_controller.h"

#include <math.h>
#include <zephyr/device.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(led_controller, LOG_LEVEL_DBG);

#define PWM_PERIOD_NS 1000000U

/* PWM LED definitions from device tree */
static const struct pwm_dt_spec pwm_led_r = PWM_DT_SPEC_GET(DT_ALIAS(pwm_led_r));
static const struct pwm_dt_spec pwm_led_g = PWM_DT_SPEC_GET(DT_ALIAS(pwm_led_g));
static const struct pwm_dt_spec pwm_led_b = PWM_DT_SPEC_GET(DT_ALIAS(pwm_led_b));
static const struct pwm_dt_spec pwm_led_w = PWM_DT_SPEC_GET(DT_ALIAS(pwm_led_w));

/* Gamma correction table (gamma = 2.2) */
static const float led_gamma = 2.2f;
static uint32_t gamma_table[256] = {0};

int led_controller_init(void)
{
  if (!pwm_is_ready_dt(&pwm_led_r)) {
    LOG_ERR("PWM LED R not ready");
    return -ENODEV;
  }
  if (!pwm_is_ready_dt(&pwm_led_g)) {
    LOG_ERR("PWM LED G not ready");
    return -ENODEV;
  }
  if (!pwm_is_ready_dt(&pwm_led_b)) {
    LOG_ERR("PWM LED B not ready");
    return -ENODEV;
  }
  if (!pwm_is_ready_dt(&pwm_led_w)) {
    LOG_ERR("PWM LED W not ready");
    return -ENODEV;
  }

  /* Set initial duty cycle to 0 (off) */
  pwm_set_dt(&pwm_led_r, PWM_PERIOD_NS, 1);
  pwm_set_dt(&pwm_led_g, PWM_PERIOD_NS, 1);
  pwm_set_dt(&pwm_led_b, PWM_PERIOD_NS, 1);
  pwm_set_dt(&pwm_led_w, PWM_PERIOD_NS, 1);

  /* Generate gamma correction table */
  for (int i = 0; i < 256; i++) {
    gamma_table[i] = (uint32_t)((powf(i / 255.0f, led_gamma)) * PWM_PERIOD_NS);
    if (gamma_table[i] > 1) {
      gamma_table[i] -= 1;
    }
  }

  led_off();
  LOG_INF("LED Controller initialized");
  return 0;
}

void led_set_rgbw(uint8_t r, uint8_t g, uint8_t b, uint8_t w)
{
  pwm_set_dt(&pwm_led_r, PWM_PERIOD_NS, gamma_table[r]);
  pwm_set_dt(&pwm_led_g, PWM_PERIOD_NS, gamma_table[g]);
  pwm_set_dt(&pwm_led_b, PWM_PERIOD_NS, gamma_table[b]);
  pwm_set_dt(&pwm_led_w, PWM_PERIOD_NS, gamma_table[w]);
}

void led_off(void) { led_set_rgbw(0, 0, 0, 0); }

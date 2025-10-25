#pragma once

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/pm/device.h>
#include <zephyr/pm/device_runtime.h>
#include <zephyr/pm/pm.h>

#include "zephyr/drivers/hwinfo.h"
#include "zephyr/sys/poweroff.h"

class PowerManager
{
public:
  PowerManager(
    const struct gpio_dt_spec * button_next_spec, const struct gpio_dt_spec * button_prev_spec)
  : button_next_spec_(button_next_spec), button_prev_spec_(button_prev_spec)
  {
  }

  void power_off()
  {
    const struct device * const cons = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));

    printk("Powering off... Entering deep sleep\n");

    // Give some time for the message to be printed
    k_sleep(K_MSEC(100));

    gpio_pin_configure_dt(button_next_spec_, GPIO_INPUT);
    gpio_pin_configure_dt(button_prev_spec_, GPIO_INPUT);
    gpio_pin_interrupt_configure_dt(button_next_spec_, GPIO_INT_LEVEL_LOW);
    gpio_pin_interrupt_configure_dt(button_prev_spec_, GPIO_INT_DISABLE);

    // Enter deep sleep using Nordic's power management
    // Force system to enter deep sleep state
    pm_device_action_run(cons, PM_DEVICE_ACTION_SUSPEND);

    hwinfo_clear_reset_cause();
    sys_poweroff();

    // This line should never be reached
    printk("ERROR: Failed to enter deep sleep\n");
  }

private:
  const struct gpio_dt_spec * button_next_spec_;
  const struct gpio_dt_spec * button_prev_spec_;
};

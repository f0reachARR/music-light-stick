#pragma once

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

class ButtonHandler
{
public:
  ButtonHandler(const struct gpio_dt_spec * button_spec)
  : button_(button_spec), callback_func_(nullptr), callback_data_(nullptr)
  {
  }

  bool init()
  {
    if (!button_) {
      printk("Error: Button spec is null\n");
      return false;
    }

    if (!device_is_ready(button_->port)) {
      printk("Error: Button device %s is not ready\n", button_->port->name);
      return false;
    }

    int ret = gpio_pin_configure_dt(button_, GPIO_INPUT);
    if (ret < 0) {
      printk("Error: Failed to configure button pin: %d\n", ret);
      return false;
    }

    ret = gpio_pin_interrupt_configure_dt(button_, GPIO_INT_EDGE_TO_ACTIVE);
    if (ret < 0) {
      printk("Error: Failed to configure button interrupt: %d\n", ret);
      return false;
    }

    gpio_init_callback(&button_cb_data_, button_pressed_callback, BIT(button_->pin));
    gpio_add_callback(button_->port, &button_cb_data_);

    return true;
  }

  void set_callback(void (*callback)(void *), void * user_data)
  {
    callback_func_ = callback;
    callback_data_ = user_data;
  }

  bool is_pressed() const
  {
    if (!button_) return false;
    return gpio_pin_get_dt(button_) != 0;
  }

private:
  const struct gpio_dt_spec * button_;
  struct gpio_callback button_cb_data_;

  void (*callback_func_)(void *);
  void * callback_data_;

  static void button_pressed_callback(
    const struct device * dev, struct gpio_callback * cb, uint32_t pins)
  {
    auto * handler = CONTAINER_OF(cb, ButtonHandler, button_cb_data_);
    if (handler && handler->callback_func_) {
      handler->callback_func_(handler->callback_data_);
    }
  }
};

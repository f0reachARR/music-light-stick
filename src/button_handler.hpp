#pragma once

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

enum class ButtonId
{
  BUTTON_NEXT = 0,  // Button for next preset
  BUTTON_PREV = 1,  // Button for previous preset
};

class DualButtonHandler
{
public:
  DualButtonHandler(
    const struct gpio_dt_spec * button_next_spec, const struct gpio_dt_spec * button_prev_spec)
  : button_next_(button_next_spec),
    button_prev_(button_prev_spec),
    callback_func_(nullptr),
    callback_data_(nullptr)
  {
  }

  bool init()
  {
    // Initialize next button
    if (!init_button(button_next_, &button_next_cb_data_, "Next")) {
      return false;
    }

    // Initialize prev button
    if (!init_button(button_prev_, &button_prev_cb_data_, "Prev")) {
      return false;
    }

    return true;
  }

  void set_callback(void (*callback)(ButtonId, void *), void * user_data)
  {
    callback_func_ = callback;
    callback_data_ = user_data;
  }

  bool is_next_pressed() const
  {
    if (!button_next_) return false;
    return gpio_pin_get_dt(button_next_) != 0;
  }

  bool is_prev_pressed() const
  {
    if (!button_prev_) return false;
    return gpio_pin_get_dt(button_prev_) != 0;
  }

private:
  const struct gpio_dt_spec * button_next_;
  const struct gpio_dt_spec * button_prev_;
  struct gpio_callback button_next_cb_data_;
  struct gpio_callback button_prev_cb_data_;

  void (*callback_func_)(ButtonId, void *);
  void * callback_data_;

  bool init_button(
    const struct gpio_dt_spec * button, struct gpio_callback * cb_data, const char * name)
  {
    if (!button) {
      printk("Error: %s button spec is null\n", name);
      return false;
    }

    if (!device_is_ready(button->port)) {
      printk("Error: %s button device %s is not ready\n", name, button->port->name);
      return false;
    }

    int ret = gpio_pin_configure_dt(button, GPIO_INPUT);
    if (ret < 0) {
      printk("Error: Failed to configure %s button pin: %d\n", name, ret);
      return false;
    }

    ret = gpio_pin_interrupt_configure_dt(button, GPIO_INT_EDGE_TO_ACTIVE);
    if (ret < 0) {
      printk("Error: Failed to configure %s button interrupt: %d\n", name, ret);
      return false;
    }

    // Determine which button this is and set callback accordingly
    if (button == button_next_) {
      gpio_init_callback(cb_data, button_next_pressed_callback, BIT(button->pin));
    } else {
      gpio_init_callback(cb_data, button_prev_pressed_callback, BIT(button->pin));
    }

    gpio_add_callback(button->port, cb_data);

    return true;
  }

  static void button_next_pressed_callback(
    const struct device * dev, struct gpio_callback * cb, uint32_t pins)
  {
    auto * handler = CONTAINER_OF(cb, DualButtonHandler, button_next_cb_data_);
    if (handler && handler->callback_func_) {
      handler->callback_func_(ButtonId::BUTTON_NEXT, handler->callback_data_);
    }
  }

  static void button_prev_pressed_callback(
    const struct device * dev, struct gpio_callback * cb, uint32_t pins)
  {
    auto * handler = CONTAINER_OF(cb, DualButtonHandler, button_prev_cb_data_);
    if (handler && handler->callback_func_) {
      handler->callback_func_(ButtonId::BUTTON_PREV, handler->callback_data_);
    }
  }
};

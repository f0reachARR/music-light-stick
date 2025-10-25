#pragma once

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

// Long press threshold: 2 seconds
#define BUTTON_LONG_PRESS_MS 2000

enum class ButtonId {
  BUTTON_NEXT = 0,  // Button for next preset
  BUTTON_PREV = 1,  // Button for previous preset
};

enum class ButtonEvent {
  PRESS,      // Normal press (button released before long press threshold)
  LONG_PRESS  // Long press (button held for BUTTON_LONG_PRESS_MS)
};

class DualButtonHandler
{
public:
  DualButtonHandler(
    const struct gpio_dt_spec * button_next_spec, const struct gpio_dt_spec * button_prev_spec)
  : button_next_(button_next_spec),
    button_prev_(button_prev_spec),
    callback_func_(nullptr),
    callback_data_(nullptr),
    long_press_callback_func_(nullptr),
    long_press_callback_data_(nullptr)
  {
  }

  bool init()
  {
    // Initialize next button (with both edge detection for press and release)
    if (!init_button(button_next_, &button_next_cb_data_, "Next")) {
      return false;
    }

    // Initialize prev button
    if (!init_button(button_prev_, &button_prev_cb_data_, "Prev")) {
      return false;
    }

    k_timer_init(&long_press_timer_, long_press_timer_callback, nullptr);
    k_timer_user_data_set(&long_press_timer_, this);

    return true;
  }

  void set_callback(void (*callback)(ButtonId, void *), void * user_data)
  {
    callback_func_ = callback;
    callback_data_ = user_data;
  }

  void set_long_press_callback(void (*callback)(ButtonId, void *), void * user_data)
  {
    long_press_callback_func_ = callback;
    long_press_callback_data_ = user_data;
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

  void disable_interrupts()
  {
    if (button_next_) {
      gpio_pin_interrupt_configure_dt(button_next_, GPIO_INT_DISABLE);
    }
    if (button_prev_) {
      gpio_pin_interrupt_configure_dt(button_prev_, GPIO_INT_DISABLE);
    }
    gpio_remove_callback(button_next_->port, &button_next_cb_data_);
    gpio_remove_callback(button_prev_->port, &button_prev_cb_data_);
  }

private:
  const struct gpio_dt_spec * button_next_;
  const struct gpio_dt_spec * button_prev_;
  struct gpio_callback button_next_cb_data_;
  struct gpio_callback button_prev_cb_data_;

  void (*callback_func_)(ButtonId, void *);
  void * callback_data_;
  void (*long_press_callback_func_)(ButtonId, void *);
  void * long_press_callback_data_;

  struct k_timer long_press_timer_;
  ButtonId button_being_held_;
  bool long_press_triggered_;

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

    // Use both edge detection to detect press and release
    ret = gpio_pin_interrupt_configure_dt(button, GPIO_INT_EDGE_BOTH);
    if (ret < 0) {
      printk("Error: Failed to configure %s button interrupt: %d\n", name, ret);
      return false;
    }

    // Determine which button this is and set callback accordingly
    if (button == button_next_) {
      gpio_init_callback(cb_data, button_next_callback, BIT(button->pin));
    } else {
      gpio_init_callback(cb_data, button_prev_callback, BIT(button->pin));
    }

    gpio_add_callback(button->port, cb_data);

    return true;
  }

  static void button_next_callback(
    const struct device * dev, struct gpio_callback * cb, uint32_t pins)
  {
    auto * handler = CONTAINER_OF(cb, DualButtonHandler, button_next_cb_data_);
    if (handler) {
      handler->handle_button_event(ButtonId::BUTTON_NEXT);
    }
  }

  static void button_prev_callback(
    const struct device * dev, struct gpio_callback * cb, uint32_t pins)
  {
    auto * handler = CONTAINER_OF(cb, DualButtonHandler, button_prev_cb_data_);
    if (handler) {
      handler->handle_button_event(ButtonId::BUTTON_PREV);
    }
  }

  void handle_button_event(ButtonId button_id)
  {
    const struct gpio_dt_spec * button =
      (button_id == ButtonId::BUTTON_NEXT) ? button_next_ : button_prev_;

    bool is_pressed = gpio_pin_get_dt(button) != 0;

    if (is_pressed) {
      // Button pressed - start long press timer
      button_being_held_ = button_id;
      long_press_triggered_ = false;
      k_timer_start(&long_press_timer_, K_MSEC(BUTTON_LONG_PRESS_MS), K_NO_WAIT);
    } else {
      // Button released
      k_timer_stop(&long_press_timer_);

      // If long press was not triggered, this is a normal press
      if (!long_press_triggered_ && callback_func_) {
        callback_func_(button_id, callback_data_);
      }
    }
  }

  static void long_press_timer_callback(struct k_timer * timer)
  {
    auto * handler = static_cast<DualButtonHandler *>(k_timer_user_data_get(timer));
    if (handler) {
      handler->handle_long_press();
    }
  }

  void handle_long_press()
  {
    long_press_triggered_ = true;

    if (long_press_callback_func_) {
      long_press_callback_func_(button_being_held_, long_press_callback_data_);
    }
  }
};

#pragma once

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "ble_service.hpp"

// Preview timeout: 30 seconds
#define PREVIEW_TIMEOUT_MS 30000

class PreviewManager
{
public:
  PreviewManager() : in_preview_mode_(false), preview_color_{0, 0, 0, 0}
  {
    k_timer_init(&timeout_timer_, timeout_callback, nullptr);
    k_timer_user_data_set(&timeout_timer_, this);
  }

  void enter_preview_mode(const rgbw_color_t & color)
  {
    preview_color_ = color;
    in_preview_mode_ = true;

    // Start or restart the timeout timer
    k_timer_start(&timeout_timer_, K_MSEC(PREVIEW_TIMEOUT_MS), K_NO_WAIT);
  }

  void exit_preview_mode()
  {
    in_preview_mode_ = false;
    k_timer_stop(&timeout_timer_);
  }

  bool is_in_preview_mode() const { return in_preview_mode_; }

  rgbw_color_t get_preview_color() const { return preview_color_; }

  void set_timeout_callback(void (*callback)(void *), void * user_data)
  {
    timeout_callback_func_ = callback;
    timeout_callback_data_ = user_data;
  }

private:
  bool in_preview_mode_;
  rgbw_color_t preview_color_;
  struct k_timer timeout_timer_;

  void (*timeout_callback_func_)(void *) = nullptr;
  void * timeout_callback_data_ = nullptr;

  static void timeout_callback(struct k_timer * timer)
  {
    auto * manager = static_cast<PreviewManager *>(k_timer_user_data_get(timer));
    if (manager) {
      manager->handle_timeout();
    }
  }

  void handle_timeout()
  {
    if (in_preview_mode_) {
      exit_preview_mode();
      if (timeout_callback_func_) {
        timeout_callback_func_(timeout_callback_data_);
      }
    }
  }
};

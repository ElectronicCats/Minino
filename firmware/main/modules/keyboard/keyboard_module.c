#include "keyboard_module.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "menus_module.h"
#include "preferences.h"

static int IDLE_TIMEOUT_S = 30;

static const char* TAG = "keyboard";
static input_callback_t input_callback = NULL;
esp_timer_handle_t idle_timer;
static bool is_idle = false;
static bool lock_input = false;

static void button_event_cb(void* arg, void* data);

void keyboard_module_reset_idle_timer() {
  esp_timer_stop(idle_timer);
  esp_timer_start_once(idle_timer, IDLE_TIMEOUT_S * 1000 * 1000);
}

void keyboard_module_set_lock(bool lock) {
  lock_input = lock;
}

void button_init(uint32_t button_num, uint8_t mask) {
  button_config_t btn_cfg = {
      .type = BUTTON_TYPE_GPIO,
      .gpio_button_config =
          {
              .gpio_num = button_num,
              .active_level = BUTTON_ACTIVE_LEVEL,
          },
  };
  button_handle_t btn = iot_button_create(&btn_cfg);
  assert(btn);
  esp_err_t err =
      iot_button_register_cb(btn, BUTTON_PRESS_DOWN, button_event_cb,
                             (void*) (BUTTON_PRESS_DOWN | mask));
  err |= iot_button_register_cb(btn, BUTTON_PRESS_UP, button_event_cb,
                                (void*) (BUTTON_PRESS_UP | mask));
  err |= iot_button_register_cb(btn, BUTTON_PRESS_REPEAT, button_event_cb,
                                (void*) (BUTTON_PRESS_REPEAT | mask));
  err |= iot_button_register_cb(btn, BUTTON_PRESS_REPEAT_DONE, button_event_cb,
                                (void*) (BUTTON_PRESS_REPEAT_DONE | mask));
  err |= iot_button_register_cb(btn, BUTTON_SINGLE_CLICK, button_event_cb,
                                (void*) (BUTTON_SINGLE_CLICK | mask));
  err |= iot_button_register_cb(btn, BUTTON_DOUBLE_CLICK, button_event_cb,
                                (void*) (BUTTON_DOUBLE_CLICK | mask));
  err |= iot_button_register_cb(btn, BUTTON_LONG_PRESS_START, button_event_cb,
                                (void*) (BUTTON_LONG_PRESS_START | mask));
  err |= iot_button_register_cb(btn, BUTTON_LONG_PRESS_HOLD, button_event_cb,
                                (void*) (BUTTON_LONG_PRESS_HOLD | mask));
  err |= iot_button_register_cb(btn, BUTTON_LONG_PRESS_UP, button_event_cb,
                                (void*) (BUTTON_LONG_PRESS_UP | mask));
  ESP_ERROR_CHECK(err);
}

/**
 * @brief Keyboard button event callback
 *
 * @param void* arg
 * @param void* data
 *
 * @return void
 */
static void button_event_cb(void* arg, void* data) {
  uint8_t button_name =
      (((button_event_t) data) >> 4);  // >> 4 to get the button number
  uint8_t button_event =
      ((button_event_t) data) &
      0x0F;  // & 0x0F to get the event number without the mask
  // esp_timer_stop(idle_timer);

  // If we have an app with a custom handler, we call it

  if (lock_input) {
    return;
  }
  if (input_callback) {
    input_callback(button_name, button_event);
    return;
  }

  // IDLE_TIMEOUT_S = preferences_get_int("dp_time", 30);
  // esp_timer_start_once(idle_timer, IDLE_TIMEOUT_S * 1000 * 1000);
  // if (button_event != BUTTON_PRESS_DOWN) {
  //   return;
  // }

  // if (is_idle) {
  //   is_idle = false;
  //   return;
  // }
}

void keyboard_module_set_input_callback(input_callback_t input_cb) {
  input_callback = input_cb;
}
void keyboard_module_begin() {
#if !defined(CONFIG_KEYBOARD_DEBUG)
  esp_log_level_set(TAG, ESP_LOG_NONE);
#endif
  button_init(BOOT_BUTTON_PIN, BOOT_BUTTON_MASK);
  button_init(LEFT_BUTTON_PIN, LEFT_BUTTON_MASK);
  button_init(RIGHT_BUTTON_PIN, RIGHT_BUTTON_MASK);
  button_init(UP_BUTTON_PIN, UP_BUTTON_MASK);
  button_init(DOWN_BUTTON_PIN, DOWN_BUTTON_MASK);
}

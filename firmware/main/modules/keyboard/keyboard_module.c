#include "keyboard_module.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "menus_module.h"
#include "preferences.h"

static int IDLE_TIMEOUT_S = 30;

static const char* TAG = "keyboard";
static input_callback_t input_callback = NULL;
esp_timer_handle_t idle_timer = NULL;
static bool lock_input = false;

static const char* button_to_name[] = {
    "BOOT", "LEFT", "RIGHT", "UP", "DOWN",
};

static const char* event_to_name[] = {
    "PRESS_DOWN",      "PRESS_UP",      "PRESS_REPEAT",   "PRESS_REPEAT_DONE",
    "SINGLE_CLICK",    "DOUBLE_CLICK",  "MULTIPLE_CLICK", "LONG_PRESS_START",
    "LONG_PRESS_HOLD", "LONG_PRESS_UP", "PRESS_END",      "NONE_PRESS",
};

static void button_event_cb(void* arg, void* data);

static void keyboard_idle_timer_cb(void* arg) {
  // Idle callback placeholder
}

void keyboard_module_reset_idle_timer() {
  if (idle_timer != NULL) {
    esp_timer_stop(idle_timer);
    esp_timer_start_once(idle_timer, (uint64_t) IDLE_TIMEOUT_S * 1000 * 1000);
  }
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
  if (!btn) {
    ESP_LOGE("keyboard_module", "Failed to create button %" PRIu32, button_num);
    return;
  }
  iot_button_register_cb(btn, BUTTON_PRESS_DOWN, button_event_cb,
                         (void*) (uintptr_t) (BUTTON_PRESS_DOWN | mask));
  iot_button_register_cb(btn, BUTTON_PRESS_UP, button_event_cb,
                         (void*) (uintptr_t) (BUTTON_PRESS_UP | mask));
  iot_button_register_cb(btn, BUTTON_PRESS_REPEAT, button_event_cb,
                         (void*) (uintptr_t) (BUTTON_PRESS_REPEAT | mask));
  iot_button_register_cb(btn, BUTTON_PRESS_REPEAT_DONE, button_event_cb,
                         (void*) (uintptr_t) (BUTTON_PRESS_REPEAT_DONE | mask));
  iot_button_register_cb(btn, BUTTON_SINGLE_CLICK, button_event_cb,
                         (void*) (uintptr_t) (BUTTON_SINGLE_CLICK | mask));
  iot_button_register_cb(btn, BUTTON_DOUBLE_CLICK, button_event_cb,
                         (void*) (uintptr_t) (BUTTON_DOUBLE_CLICK | mask));
  iot_button_register_cb(btn, BUTTON_LONG_PRESS_START, button_event_cb,
                         (void*) (uintptr_t) (BUTTON_LONG_PRESS_START | mask));
  iot_button_register_cb(btn, BUTTON_LONG_PRESS_HOLD, button_event_cb,
                         (void*) (uintptr_t) (BUTTON_LONG_PRESS_HOLD | mask));
  iot_button_register_cb(btn, BUTTON_LONG_PRESS_UP, button_event_cb,
                         (void*) (uintptr_t) (BUTTON_LONG_PRESS_UP | mask));
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
  // >> 4 to get the button number
  uint8_t button_name = (((button_event_t) data) >> 4);
  // & 0x0F to get the event number without the mask
  uint8_t button_event = ((button_event_t) data) & 0x0F;
  // DO NOT REMOVE THIS LOG
  ESP_LOGI(TAG, "Button %s event %s", button_to_name[button_name],
           event_to_name[button_event]);

  if (lock_input) {
    return;
  }

  if (input_callback) {
    input_callback(button_name, button_event);
    return;
  }
}

void keyboard_module_set_input_callback(input_callback_t input_cb) {
  input_callback = input_cb;
}
void keyboard_module_begin() {
#if !defined(CONFIG_KEYBOARD_DEBUG)
  esp_log_level_set(TAG, ESP_LOG_NONE);
#endif
  if (idle_timer == NULL) {
    const esp_timer_create_args_t timer_args = {
        .callback = keyboard_idle_timer_cb,
        .arg = NULL,
        .name = "kbd_idle_timer",
    };
    esp_timer_create(&timer_args, &idle_timer);
  }
  button_init(BOOT_BUTTON_PIN, BOOT_BUTTON_MASK);
  button_init(LEFT_BUTTON_PIN, LEFT_BUTTON_MASK);
  button_init(RIGHT_BUTTON_PIN, RIGHT_BUTTON_MASK);
  button_init(UP_BUTTON_PIN, UP_BUTTON_MASK);
  button_init(DOWN_BUTTON_PIN, DOWN_BUTTON_MASK);
}

#include "keyboard_module.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "menu_screens_modules.h"
#include "preferences.h"

static int IDLE_TIMEOUT_S = 30;

static const char* TAG = "keyboard";
app_state_t app_state;
esp_timer_handle_t idle_timer;
static bool is_idle = false;
static bool lock_input = false;

void timer_callback() {
  screen_module_menu_t menu = menu_screens_get_current_menu();
  if (menu == MENU_WIFI_ANALYZER_RUN || menu == MENU_WIFI_ANALYZER_SUMMARY ||
      menu == MENU_GPS_DATE_TIME || menu == MENU_GPS_LOCATION ||
      menu == MENU_GPS_SPEED) {
    return;
  }

  is_idle = true;
  run_screen_saver();
}
void keyboard_module_reset_idle_timer() {
  esp_timer_stop(idle_timer);
  esp_timer_start_once(idle_timer, IDLE_TIMEOUT_S * 1000 * 1000);
}

void keyboard_module_set_lock(bool lock) {
  lock_input = lock;
}

static void button_event_cb(void* arg, void* data);
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
  const char* button_name_str = button_names[button_name];
  const char* button_event_str = button_events_name[button_event];

  ESP_LOGI(TAG, "Button: %s, Event: %s", button_name_str, button_event_str);

  stop_screen_saver();
  esp_timer_stop(idle_timer);

  // If we have an app with a custom handler, we call it

  if (lock_input) {
    return;
  }
  app_state = menu_screens_get_app_state();
  if (app_state.in_app) {
    app_state.app_handler(button_name, button_event);
    return;
  }

  IDLE_TIMEOUT_S = preferences_get_int("dp_time", 30);
  esp_timer_start_once(idle_timer, IDLE_TIMEOUT_S * 1000 * 1000);
  if (button_event != BUTTON_PRESS_DOWN) {
    return;
  }

  if (is_idle) {
    is_idle = false;
    menu_screens_display_menu();
    return;
  }

  switch (button_name) {
    case BUTTON_BOOT:
      break;
    case BUTTON_LEFT:
      menu_screens_exit_submenu();
      break;
    case BUTTON_RIGHT:
      int is_main = preferences_get_int("MENUNUMBER", MENU_MAIN);
      if (preferences_get_int("logo_show", 1) == 1 && is_main == MENU_MAIN) {
        preferences_put_int("logo_show", 0);
        menu_screens_decrement_selected_item();
        break;
      }
      menu_screens_enter_submenu();
      break;
    case BUTTON_UP:
      menu_screens_decrement_selected_item();
      break;
    case BUTTON_DOWN:
      menu_screens_ingrement_selected_item();
      break;
    default:
      break;
  }
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
  esp_timer_create_args_t timer_args = {.callback = timer_callback,
                                        .arg = NULL,

                                        .name = "one_shot_timer"};
  esp_err_t err = esp_timer_create(&timer_args, &idle_timer);
}

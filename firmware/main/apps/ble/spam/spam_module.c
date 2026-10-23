#include "spam_module.h"
#include "animations_task.h"
#include "bt_spam.h"
#include "esp_log.h"
#include "hid_module.h"
#include "led_events.h"
#include "menus_module.h"
#include "oled_screen.h"
#include "spam_screens.h"
extern SemaphoreHandle_t text_mutex;
#include "wifi_controller.h"
#include "app_manager.h"


// Selected spam mode (set by wrapper before entering app)
static bt_spam_mode_t s_selected_spam_mode = BT_SPAM_MODE_APPLE;

static void ble_module_state_machine(uint8_t button_name, uint8_t button_event);

static void ble_module_state_machine(uint8_t button_name,
                                     uint8_t button_event) {
  if (button_event != BUTTON_PRESS_DOWN &&
      button_event != BUTTON_LONG_PRESS_HOLD) {
    return;
  }
  switch (button_name) {
    case BUTTON_LEFT:
      ble_spam_stop();
      oled_screen_clear();
      menus_module_exit_app();
      break;
    case BUTTON_RIGHT:
    case BUTTON_UP:
    case BUTTON_DOWN:
    case BUTTON_BOOT:
    default:
      break;
  }
}

static void ble_spam_start_mode(bt_spam_mode_t mode, const char* title) {
  if (bt_spam_is_running()) {
    bt_spam_app_stop();
  }
  hid_module_stop();
  wifi_driver_deinit_if_started();
  menus_module_set_app_state(true, ble_module_state_machine);
  oled_screen_clear();
  ble_screens_start_scanning_animation(title);
  bt_spam_register_cb(ble_screens_display_scanning_text);
  bt_spam_register_connect_cb(ble_screens_display_peer_connected);
  bt_spam_app_main(mode);
}

void ble_spam_apple_begin(void) {
  ble_spam_start_mode(BT_SPAM_MODE_APPLE, "Apple Spam");
}

void ble_spam_android_begin(void) {
  ble_spam_start_mode(BT_SPAM_MODE_ANDROID, "Android Spam");
}

void ble_spam_windows_begin(void) {
  ble_spam_start_mode(BT_SPAM_MODE_WINDOWS, "Windows Spam");
}

void ble_spam_samsung_begin(void) {
  ble_spam_start_mode(BT_SPAM_MODE_SAMSUNG, "Samsung Spam");
}

void ble_spam_all_begin(void) {
  ble_spam_start_mode(BT_SPAM_MODE_ALL, "Random Spam");
}

void ble_spam_stop(void) {
  bt_spam_register_cb(NULL);
  bt_spam_register_connect_cb(NULL);
  animations_task_stop();
  bt_spam_app_deinit();
}

void ble_module_begin(void) {
  ble_spam_all_begin();
}


// ---------------------------------------------------------------------------
// app_manager callbacks
// ---------------------------------------------------------------------------
static void ble_spam_on_enter(void) {
  // Start the selected spam mode
  if (bt_spam_is_running()) {
    bt_spam_app_stop();
  }
  hid_module_stop();
  wifi_driver_deinit_if_started();
  menus_module_set_app_state(true, ble_module_state_machine);
  oled_screen_clear();
  ble_screens_start_scanning_animation(NULL);  // Title already set by wrapper
  bt_spam_register_cb(ble_screens_display_scanning_text);
  bt_spam_register_connect_cb(ble_screens_display_peer_connected);
  bt_spam_app_main(s_selected_spam_mode);
}

static void ble_spam_on_input(uint8_t button_name, uint8_t button_event) {
  // Handled by ble_module_state_machine
  (void)button_name;
  (void)button_event;
}

static void ble_spam_on_exit(void) {
  bt_spam_register_cb(NULL);
  bt_spam_register_connect_cb(NULL);
  animations_task_stop();
  bt_spam_app_deinit();
  void* tm = ble_screens_get_text_mutex();
  if (tm != NULL) {
    vSemaphoreDelete(tm);
  }
}

// Wrapper functions for each menu entry
void app_manager_enter_wrapper_ble_spam_apple(void) {
  s_selected_spam_mode = BT_SPAM_MODE_APPLE;
  extern const minino_app_t ble_spam_app;
  app_manager_enter(&ble_spam_app);
}

void app_manager_enter_wrapper_ble_spam_android(void) {
  s_selected_spam_mode = BT_SPAM_MODE_ANDROID;
  extern const minino_app_t ble_spam_app;
  app_manager_enter(&ble_spam_app);
}

void app_manager_enter_wrapper_ble_spam_windows(void) {
  s_selected_spam_mode = BT_SPAM_MODE_WINDOWS;
  extern const minino_app_t ble_spam_app;
  app_manager_enter(&ble_spam_app);
}

void app_manager_enter_wrapper_ble_spam_samsung(void) {
  s_selected_spam_mode = BT_SPAM_MODE_SAMSUNG;
  extern const minino_app_t ble_spam_app;
  app_manager_enter(&ble_spam_app);
}

void app_manager_enter_wrapper_ble_spam_all(void) {
  s_selected_spam_mode = BT_SPAM_MODE_ALL;
  extern const minino_app_t ble_spam_app;
  app_manager_enter(&ble_spam_app);
}

// App definition for app_manager
const minino_app_t ble_spam_app = {
  .name = "ble_spam",
  .on_enter = ble_spam_on_enter,
  .on_input = ble_spam_on_input,
  .on_exit = ble_spam_on_exit,
};


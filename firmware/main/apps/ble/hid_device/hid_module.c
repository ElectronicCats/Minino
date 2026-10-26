#include "hid_module.h"
#include <stddef.h>
#include <stdio.h>
#include "app_manager.h"
#include "ble_hidd_main.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "general_submenu.h"
#include "hid_screens.h"
#include "menus_module.h"
#include "oled_screen.h"
#include "spam_module.h"

static uint16_t current_item = 0;
static volatile bool s_hid_connected = false;
static TaskHandle_t s_connected_task_handle = NULL;

static void hid_module_cb_event(uint8_t button_name, uint8_t button_event);
static void hid_module_cb_connection_handler(bool connection);
static void hid_module_display_action_menu();

static void hid_module_reset_menu() {
  // current_item = 0;
  hid_module_register_menu(GENERAL_TREE_APP_MENU);
  hid_module_display_menu(current_item);
  menus_module_set_app_state(true, hid_module_cb_event);
}

static void hid_module_reset_start_menu() {
  s_hid_connected = false;
  if (s_connected_task_handle != NULL) {
    vTaskDelete(s_connected_task_handle);
    s_connected_task_handle = NULL;
  }
  ble_hid_end();
  oled_screen_clear();
  hid_module_reset_menu();
}

/* Maps an action to the window that confirms it. Kept next to the send switch
   so a new action cannot forget to add its confirmation. */
typedef void (*hid_notify_fn)(void);
static const hid_notify_fn k_hid_notify[HID_DEVICE_COUNT] = {
    [HID_DEVICE_VOL_UP] = hid_module_display_notify_volumen_up,
    [HID_DEVICE_VOL_DOWN] = hid_module_display_notify_volumen_down,
    [HID_DEVICE_PLAY] = hid_module_display_notify_play_pause,
    [HID_DEVICE_PAUSE] = hid_module_display_notify_pause,
    [HID_DEVICE_STOP] = hid_module_display_notify_stop,
    [HID_DEVICE_MUTE] = hid_module_display_notify_mute,
    [HID_DEVICE_NEXT_TRACK] = hid_module_display_notify_next_track,
    [HID_DEVICE_PREV_TRACK] = hid_module_display_notify_prev_track,
};

static void hid_device_selection(uint8_t selection) {
  if (!s_hid_connected) {
    ESP_LOGW("hid_module", "HID action ignored: not connected");
    hid_module_display_device_pairing();
    return;
  }

  /* Press/release the consumer key and keep the verdict of the send itself.
     The confirmation window is drawn only when the report really left the
     device: a "Volume Up" flashed for a report the stack dropped (host gone
     between the pairing event and the button press) is a lie, and it is exactly
     what made this menu look broken. */
  bool sent = false;
  switch (selection) {
    case HID_DEVICE_VOL_UP:
      sent = ble_hid_volume_up(true);
      vTaskDelay(pdMS_TO_TICKS(50));
      ble_hid_volume_up(false);
      break;
    case HID_DEVICE_VOL_DOWN:
      sent = ble_hid_volume_down(true);
      vTaskDelay(pdMS_TO_TICKS(50));
      ble_hid_volume_down(false);
      break;
    case HID_DEVICE_PLAY:
      sent = ble_hid_play(true);
      vTaskDelay(pdMS_TO_TICKS(50));
      ble_hid_play(false);
      break;
    case HID_DEVICE_PAUSE:
      sent = ble_hid_pause(true);
      vTaskDelay(pdMS_TO_TICKS(50));
      ble_hid_pause(false);
      break;
    case HID_DEVICE_STOP:
      sent = ble_hid_stop(true);
      vTaskDelay(pdMS_TO_TICKS(50));
      ble_hid_stop(false);
      break;
    case HID_DEVICE_MUTE:
      sent = ble_hid_mute(true);
      vTaskDelay(pdMS_TO_TICKS(50));
      ble_hid_mute(false);
      break;
    case HID_DEVICE_NEXT_TRACK:
      sent = ble_hid_next_track(true);
      vTaskDelay(pdMS_TO_TICKS(50));
      ble_hid_next_track(false);
      break;
    case HID_DEVICE_PREV_TRACK:
      sent = ble_hid_prev_track(true);
      vTaskDelay(pdMS_TO_TICKS(50));
      ble_hid_prev_track(false);
      break;
    default:
      break;
  }

  if (selection >= HID_DEVICE_COUNT || k_hid_notify[selection] == NULL) {
    return;
  }
  if (!sent) {
    ESP_LOGW("hid_module", "action %u was dropped by the stack", selection);
    hid_module_display_device_pairing();
    return;
  }
  k_hid_notify[selection]();

  general_submenu_menu_t hid_menu_filter = {0};
  hid_menu_filter.options = (const char**) hid_device_items;
  hid_menu_filter.options_count = HID_DEVICE_COUNT;
  hid_menu_filter.select_cb = hid_device_selection;
  hid_menu_filter.exit_cb = hid_module_reset_menu;
  hid_menu_filter.selected_option = selection;
  general_submenu(hid_menu_filter);
}

static void hid_module_display_action_menu() {
  general_submenu_menu_t hid_menu_filter = {0};
  hid_menu_filter.options = (const char**) hid_device_items;
  hid_menu_filter.options_count = HID_DEVICE_COUNT;
  hid_menu_filter.select_cb = hid_device_selection;
  hid_menu_filter.exit_cb = hid_module_reset_menu;
  general_submenu(hid_menu_filter);
}

/*
 * hid_module_cb_connection_handler() is raised from ESP_GATTS_CONNECT_EVT,
 * which runs on Bluedroid's BTC_TASK. Sleeping there stalls every GAP/GATTS
 * event in the system for the whole second, so the "connected" screen is
 * rendered from a short-lived task instead.
 */
static void hid_module_show_connected_screen(void* arg
                                             __attribute__((unused))) {
  vTaskDelay(pdMS_TO_TICKS(1000));
  s_connected_task_handle = NULL;
  if (s_hid_connected) {
    hid_module_display_action_menu();
  }
  vTaskDelete(NULL);
}

static void hid_module_cb_connection_handler(bool connection) {
  s_hid_connected = connection;
  if (connection) {
    hid_module_display_device_connection(true);
    if (s_connected_task_handle == NULL) {
      if (xTaskCreate(hid_module_show_connected_screen, "hid_connected", 3072,
                      NULL, 4, &s_connected_task_handle) != pdPASS) {
        s_connected_task_handle = NULL;
        ESP_LOGW("hid_module",
                 "could not spawn the connected-screen task, showing it now");
        hid_module_display_action_menu();
      }
    }
  } else {
    if (s_connected_task_handle != NULL) {
      vTaskDelete(s_connected_task_handle);
      s_connected_task_handle = NULL;
    }
    hid_module_display_device_connection(false);
    vTaskDelay(pdMS_TO_TICKS(500));
    hid_module_display_device_pairing();
  }
}

static void hid_module_cb_event(uint8_t button_name, uint8_t button_event) {
  if (button_event != BUTTON_PRESS_DOWN) {
    return;
  }
  switch (button_name) {
    case BUTTON_UP:
      current_item = current_item == 0 ? HID_MENU_COUNT - 1 : current_item - 1;
      hid_module_display_menu(current_item);
      break;
    case BUTTON_DOWN:
      current_item = current_item + 1 >= HID_MENU_COUNT ? 0 : current_item + 1;
      hid_module_display_menu(current_item);
      break;
    case BUTTON_RIGHT:
      if (current_item == HID_CONFIG_NAME) {
        current_item = 0;
        char hid_name[32];
        ble_hid_get_device_name(hid_name);
        general_screen_display_card_information_handler("Device Name", hid_name,
                                                        hid_module_reset_menu,
                                                        hid_module_cb_event);
      } else if (current_item == HID_CONFIG_MAC) {
        current_item = 0;
        uint8_t hid_mac[8] = {0};
        esp_read_mac(hid_mac, ESP_MAC_BT);
        char mac_address[20];
        snprintf(mac_address, sizeof(mac_address),
                 "%02X:%02X:%02X:%02X:%02X:%02X", hid_mac[0], hid_mac[1],
                 hid_mac[2], hid_mac[3], hid_mac[4], hid_mac[5]);
        general_screen_display_card_information_handler(
            "Device MAC", mac_address, hid_module_reset_menu,
            hid_module_cb_event);
      } else if (current_item == HID_CONFIG_START) {
        current_item = 0;
        general_screen_display_card_information_handler(
            "HID", "HID control", hid_module_reset_start_menu,
            hid_module_reset_start_menu);
        hid_module_display_device_pairing();
        ble_spam_stop();
        ble_hid_register_callback(hid_module_cb_connection_handler);
        ble_hid_begin();
        /* Give input back to the app right away: the card handler only
         * installs its own modal, so a LEFT press on the pairing screen used
         * to route to hid_module_reset_start_menu() and silently tear the
         * just-started HID down. hid_module_cb_event already handles LEFT
         * with a proper exit. */
        menus_module_set_app_state(true, hid_module_cb_event);
      }
      break;
    case BUTTON_LEFT:
      s_hid_connected = false;
      if (s_connected_task_handle != NULL) {
        vTaskDelete(s_connected_task_handle);
        s_connected_task_handle = NULL;
      }
      ble_hid_end();
      menus_module_set_default_input();
      oled_screen_clear();
      app_manager_exit();
      break;
    default:
      break;
  }
}

void hid_module_begin() {
  hid_module_register_menu(GENERAL_TREE_APP_MENU);
  hid_module_display_menu(current_item);
  menus_module_set_app_state(true, hid_module_cb_event);
}

void hid_module_stop(void) {
  s_hid_connected = false;
  if (s_connected_task_handle != NULL) {
    vTaskDelete(s_connected_task_handle);
    s_connected_task_handle = NULL;
  }
  ble_hid_end();
  oled_screen_clear();
}

// ---------------------------------------------------------------------------
// app_manager callbacks
// ---------------------------------------------------------------------------
static void hid_on_enter(void) {
  hid_module_register_menu(GENERAL_TREE_APP_MENU);
  hid_module_display_menu(current_item);
  menus_module_set_app_state(true, hid_module_cb_event);
}

static void hid_on_input(uint8_t button_name, uint8_t button_event) {
  // Reuse the existing input handler
  hid_module_cb_event(button_name, button_event);
}

static void hid_on_exit(void) {
  s_hid_connected = false;
  if (s_connected_task_handle != NULL) {
    vTaskDelete(s_connected_task_handle);
    s_connected_task_handle = NULL;
  }
  ble_hid_end();
  oled_screen_clear();
}

// Wrapper for menu entry
void app_manager_enter_wrapper_hid(void) {
  extern const minino_app_t hid_app;
  app_manager_enter(&hid_app);
}

// App definition for app_manager
const minino_app_t hid_app = {
    .name = "hid",
    .on_enter = hid_on_enter,
    .on_input = hid_on_input,
    .on_exit = hid_on_exit,
};

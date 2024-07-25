#include <stdint.h>
#include <string.h>
#include "animations_timer.h"
#include "apps/wifi/deauth/deauth_screens.h"
#include "captive_portal.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "menu_screens_modules.h"
#include "wifi_attacks.h"
#include "wifi_controller.h"
#include "wifi_scanner.h"

#define DEFAULT_SCAN_LIST_SIZE CONFIG_SCAN_MAX_AP
typedef enum {
  DEAUTH_STATE_IDLE = 0,
  DEAUTH_STATE_MENU,
  DEAUTH_STATE_RUNNING,
  DEAUTH_STATE_STOP,
  DEAUTH_STATE_MAX
} deauth_state_t;

typedef struct {
  deauth_state_t state;
  wifi_config_t wifi_config;
} wifi_module_t;

typedef struct {
  uint16_t count;
  wifi_ap_record_t records[CONFIG_SCAN_MAX_AP];
} scanned_ap_records_t;

static wifi_module_t current_wifi_state;
static scanned_ap_records_t* ap_records;
static uint16_t current_item = 0;
static menu_stadistics_t menu_stadistics;

static void deauth_module_cb_event(button_event_state_t button_pressed);
static void deauth_module_cb_event_select_ap(
    button_event_state_t button_pressed);
static void deauth_module_cb_event_attacks(button_event_state_t button_pressed);
static void deauth_module_cb_event_run(button_event_state_t button_pressed);
static void deauth_module_cb_event_captive_portal(
    button_event_state_t button_pressed);

static void scanning_task(void* pvParameters);
static void deauth_run_scan_task();
static void deauth_increment_item();
static void deauth_decrement_item();
static void deauth_handle_attacks();

static void scanning_task(void* pvParameters) {
  while (ap_records->count < (DEFAULT_SCAN_LIST_SIZE / 2)) {
    wifi_scanner_module_scan();
    vTaskDelay(5000 / portTICK_PERIOD_MS);
  }
  ap_records = wifi_scanner_get_ap_records();
  menu_stadistics.count = ap_records->count;
  animations_timer_stop();
  deauth_display_menu(current_item, menu_stadistics);
  current_wifi_state.state = DEAUTH_STATE_MENU;
  vTaskDelete(NULL);
}

static void deauth_run_scan_task() {
  ap_records = wifi_scanner_get_ap_records();
  menu_stadistics.count = ap_records->count;
  while (ap_records->count < (DEFAULT_SCAN_LIST_SIZE / 2)) {
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

static void deauth_handle_attacks() {
  wifi_attacks_module_stop();
  switch (menu_stadistics.attack) {
    case BROADCAST:
    case ROGUE_AP:
    case COMBINED:
      animations_timer_run(&deauth_display_attacking_animation, 300);
      wifi_attack_handle_attacks(current_item, &menu_stadistics.selected_ap);
      menu_screens_set_app_state(true, deauth_module_cb_event_run);
      current_item = 0;
      break;
    case CAPTIVEPORTAL:
      current_item = 0;
      menu_screens_set_app_state(true, deauth_module_cb_event_captive_portal);
      deauth_display_captive_portals(current_item, menu_stadistics);
      break;
    default:
      break;
  }
}

static void deauth_increment_item() {
  current_item++;
}

static void deauth_decrement_item() {
  current_item--;
}

void deauth_module_begin() {
  deauth_clear_screen();
  animations_timer_run(&deauth_display_scanning, 300);

  current_wifi_state.state = DEAUTH_STATE_IDLE;

  memset(&current_wifi_state.wifi_config, 0, sizeof(wifi_config_t));
  current_wifi_state.wifi_config = wifi_driver_access_point_begin();

  xTaskCreate(scanning_task, "wifi_scan", 4096, NULL, 5, NULL);
  deauth_run_scan_task();

  menu_screens_set_app_state(true, deauth_module_cb_event);

  menu_stadistics.attack = 99;
}

static void deauth_module_cb_event(button_event_state_t button_pressed) {
  if (button_pressed.button_event != BUTTON_PRESS_DOWN) {
    return;
  }
  if (current_wifi_state.state == DEAUTH_STATE_IDLE) {
    return;
  }

  switch (button_pressed.button_pressed) {
    case BUTTON_UP:
      deauth_decrement_item();
      if (current_item < 0) {
        current_item = MENUCOUNT - 1;
      }
      deauth_display_menu(current_item, menu_stadistics);
      break;
    case BUTTON_DOWN:
      deauth_increment_item();
      if (current_item > MENUCOUNT - 1) {
        current_item = 0;
      }
      deauth_display_menu(current_item, menu_stadistics);
      break;
    case BUTTON_RIGHT:
      switch (current_item) {
        case SCAN:
          deauth_clear_screen();
          animations_timer_run(&deauth_display_scanning, 300);
          xTaskCreate(scanning_task, "wifi_scan", 4096, NULL, 5, NULL);
          deauth_run_scan_task();
          menu_screens_set_app_state(true, deauth_module_cb_event);
          current_item = 0;
          break;
        case SELECT:
          current_item = 0;
          menu_screens_set_app_state(true, deauth_module_cb_event_select_ap);
          deauth_display_scanned_ap(ap_records->records, ap_records->count,
                                    current_item);
          break;
        case DEAUTH:
          current_item = 0;
          menu_screens_set_app_state(true, deauth_module_cb_event_attacks);
          deauth_display_attacks(current_item, menu_stadistics);
          break;
        case RUN:
          if (menu_stadistics.selected_ap.bssid[0] == 0) {
            deauth_display_warning_not_ap_selected();
            vTaskDelay(1500 / portTICK_PERIOD_MS);
            deauth_display_menu(current_item, menu_stadistics);
            break;
          }
          if (menu_stadistics.attack == 99) {
            deauth_display_warning_not_attack_selected();
            vTaskDelay(1500 / portTICK_PERIOD_MS);
            deauth_display_menu(current_item, menu_stadistics);
            break;
          }
          deauth_clear_screen();
          deauth_handle_attacks();
          break;
        default:
          current_item = 0;
          break;
      }
      break;
    case BUTTON_LEFT:
      menu_screens_set_app_state(false, NULL);
      menu_screens_exit_submenu();
      break;
    default:
      break;
  }
}

static void deauth_module_cb_event_select_ap(
    button_event_state_t button_pressed) {
  if (button_pressed.button_event != BUTTON_PRESS_DOWN) {
    return;
  }
  switch (button_pressed.button_pressed) {
    case BUTTON_UP:
      deauth_decrement_item();
      if (current_item < 0) {
        current_item = ap_records->count;
      }
      deauth_display_scanned_ap(ap_records->records, ap_records->count,
                                current_item);
      break;
    case BUTTON_DOWN:
      deauth_increment_item();
      if (current_item > ap_records->count) {
        current_item = 0;
      }
      deauth_display_scanned_ap(ap_records->records, ap_records->count,
                                current_item);
      break;
    case BUTTON_RIGHT:
      menu_stadistics.selected_ap = ap_records->records[current_item];
      menu_screens_set_app_state(true, deauth_module_cb_event);
      deauth_display_menu(current_item, menu_stadistics);
      current_item = 0;
      break;
    case BUTTON_LEFT:
      current_item = 0;
      menu_screens_set_app_state(true, deauth_module_cb_event);
      deauth_display_menu(current_item, menu_stadistics);
      break;
    default:
      break;
  }
}

static void deauth_module_cb_event_attacks(
    button_event_state_t button_pressed) {
  if (button_pressed.button_event != BUTTON_PRESS_DOWN) {
    return;
  }
  switch (button_pressed.button_pressed) {
    case BUTTON_UP:
      deauth_decrement_item();
      deauth_display_attacks(current_item, menu_stadistics);
      break;
    case BUTTON_DOWN:
      deauth_increment_item();
      deauth_display_attacks(current_item, menu_stadistics);
      break;
    case BUTTON_RIGHT:
      menu_stadistics.attack = current_item;
      menu_screens_set_app_state(true, deauth_module_cb_event);

      current_item = 0;
      deauth_display_menu(current_item, menu_stadistics);
      break;
    case BUTTON_LEFT:
      current_item = 0;
      menu_screens_set_app_state(true, deauth_module_cb_event);
      deauth_display_menu(current_item, menu_stadistics);
      break;
    default:
      break;
  }
}

static void deauth_module_cb_event_run(button_event_state_t button_pressed) {
  if (button_pressed.button_event != BUTTON_PRESS_DOWN) {
    return;
  }
  switch (button_pressed.button_pressed) {
    case BUTTON_LEFT:
      current_item = 0;
      wifi_attacks_module_stop();
      animations_timer_stop();
      menu_screens_set_app_state(true, deauth_module_cb_event);
      deauth_display_menu(current_item, menu_stadistics);
      break;
    case BUTTON_UP:
    case BUTTON_DOWN:
    case BUTTON_RIGHT:
    default:
      break;
  }
}

static void deauth_module_cb_event_captive_portal(
    button_event_state_t button_pressed) {
  if (button_pressed.button_event != BUTTON_PRESS_DOWN) {
    return;
  }
  ESP_LOGI("asdsad", "Event");
  switch (button_pressed.button_pressed) {
    case BUTTON_UP:
      deauth_decrement_item();
      if (current_item < 0) {
        current_item = CAPTIVEPORTALCOUNT - 1;
      }
      deauth_display_captive_portals(current_item, menu_stadistics);
      break;
    case BUTTON_DOWN:
      deauth_increment_item();
      if (current_item > CAPTIVEPORTALCOUNT - 1) {
        current_item = 0;
      }
      deauth_display_captive_portals(current_item, menu_stadistics);
      break;
    case BUTTON_LEFT:
      current_item = 0;
      captive_portal_stop();
      menu_screens_set_app_state(true, deauth_module_cb_event);
      deauth_display_menu(current_item, menu_stadistics);
      break;
    case BUTTON_RIGHT:
      captive_portal_set_portal(current_item);
      captive_portal_set_config_ssid(menu_stadistics.selected_ap);
      captive_portal_register_cb(deauth_display_captive_portal_creds);
      deauth_display_captive_waiting();
      xTaskCreate(captive_portal_begin, "captive_portal_start", 4096, NULL, 5,
                  NULL);
      menu_screens_set_app_state(true, deauth_module_cb_event_run);
      current_item = 0;
      break;
    default:
      break;
  }
}
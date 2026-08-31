// SPDX-License-Identifier: GPL-3.0-or-later
#include "trackers_module.h"
#include <math.h>
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gps_hw.h"
#include "gps_module.h"
#include "menus_module.h"
#include "oled_screen.h"
#include "trackers_scanner.h"
#include "trackers_screens.h"

#define TAG "trackers_app"

static bool s_app_running = false;
static bool s_scanning = false;
static bool s_in_help = false;
static uint8_t s_help_page = 0;
static TaskHandle_t s_gui_task = NULL;

static tracker_profile_t* s_trackers = NULL;
static uint16_t s_tracker_count = 0;
static uint16_t s_last_index = 0;

static gps_t s_latest_gps;
static bool s_have_gps = false;
static portMUX_TYPE s_gps_mux = portMUX_INITIALIZER_UNLOCKED;

static void trackers_gps_event_cb(gps_t* gps) {
  if (gps != NULL) {
    portENTER_CRITICAL(&s_gps_mux);
    s_latest_gps = *gps;
    s_have_gps = true;
    portEXIT_CRITICAL(&s_gps_mux);
  }
}

static void on_tracker_found(tracker_profile_t record) {
  if (!s_app_running) {
    return;
  }

  record.gps_latitude = 0.0;
  record.gps_longitude = 0.0;
  record.gps_valid = false;

  portENTER_CRITICAL(&s_gps_mux);
  if (s_have_gps) {
    record.gps_latitude = s_latest_gps.latitude;
    record.gps_longitude = s_latest_gps.longitude;
    record.gps_valid = true;
  }
  portEXIT_CRITICAL(&s_gps_mux);

  record.distance = trackers_scanner_rssi_to_distance(record.rssi);

  int idx = trackers_scanner_find_profile_by_mac(s_trackers, s_tracker_count,
                                                  record.mac_address);
  if (idx == -1) {
    trackers_scanner_add_tracker_profile(&s_trackers, &s_tracker_count, record);
    s_last_index = s_tracker_count - 1;
    ESP_LOGI(TAG, "New tracker: %s (%s) %.1fm", record.name, record.vendor,
             record.distance);
  } else {
    s_trackers[idx].rssi = record.rssi;
    s_trackers[idx].distance = record.distance;
    s_trackers[idx].gps_latitude = record.gps_latitude;
    s_trackers[idx].gps_longitude = record.gps_longitude;
    s_trackers[idx].gps_valid = record.gps_valid;
    s_last_index = idx;
  }
}

static void gui_update_task(void* arg) {
  (void) arg;
  while (s_app_running) {
    if (!s_in_help) {
      const char* name = NULL;
      const char* vendor = NULL;
      int8_t rssi = 0;
      float distance = -1.0f;
      bool gps_valid = false;
      double gps_lat = 0.0;
      double gps_lon = 0.0;

      if (s_tracker_count > 0 && s_last_index < s_tracker_count) {
        name = s_trackers[s_last_index].name;
        vendor = s_trackers[s_last_index].vendor;
        rssi = s_trackers[s_last_index].rssi;
        distance = s_trackers[s_last_index].distance;
        gps_valid = s_trackers[s_last_index].gps_valid;
        gps_lat = s_trackers[s_last_index].gps_latitude;
        gps_lon = s_trackers[s_last_index].gps_longitude;
      }

      trackers_screens_show_status(s_tracker_count, name, vendor, rssi,
                                   distance, gps_valid, gps_lat, gps_lon,
                                   s_scanning);
    }
    vTaskDelay(pdMS_TO_TICKS(250));
  }
  s_gui_task = NULL;
  vTaskDelete(NULL);
}

static void trackers_input_cb(uint8_t button_name, uint8_t button_event) {
  if (button_event != BUTTON_PRESS_DOWN) {
    return;
  }

  if (s_in_help) {
    if (button_name == BUTTON_LEFT || button_name == BUTTON_BOOT ||
        button_name == BUTTON_UP) {
      s_in_help = false;
      oled_screen_clear();
    } else if (button_name == BUTTON_DOWN) {
      s_help_page = (s_help_page + 1) % 2;
      trackers_screens_show_help(s_help_page);
    }
    return;
  }

  switch (button_name) {
    case BUTTON_LEFT:
      trackers_module_stop();
      break;
    case BUTTON_UP:
      s_in_help = true;
      s_help_page = 0;
      trackers_screens_show_help(s_help_page);
      break;
    case BUTTON_DOWN:
      if (s_scanning) {
        trackers_scanner_stop();
        s_scanning = false;
        if (gps_hw_get_state()) {
          gps_module_stop_read();
          gps_module_unregister_cb();
        }
      } else {
        if (gps_hw_get_state()) {
          gps_module_register_cb(trackers_gps_event_cb);
          gps_module_start_scan();
        }
        trackers_scanner_register_cb(on_tracker_found);
        trackers_scanner_start();
        s_scanning = true;
      }
      break;
    default:
      break;
  }
}

void trackers_module_begin(void) {
  s_app_running = true;
  s_in_help = false;
  s_scanning = false;
  s_tracker_count = 0;
  s_last_index = 0;
  s_have_gps = false;

  if (s_trackers != NULL) {
    free(s_trackers);
    s_trackers = NULL;
  }

  menus_module_set_app_state(true, trackers_input_cb);

  if (gps_hw_get_state()) {
    gps_module_register_cb(trackers_gps_event_cb);
    gps_module_start_scan();
  }
  trackers_scanner_register_cb(on_tracker_found);
  trackers_scanner_start();
  s_scanning = true;

  if (xTaskCreate(gui_update_task, "trk_gui", 4096, NULL, 4, &s_gui_task) !=
      pdPASS) {
    ESP_LOGE(TAG, "Failed to create gui update task");
  }
}

void trackers_module_stop(void) {
  s_app_running = false;
  ESP_LOGI(TAG, "stop: exit via back");

  while (s_gui_task != NULL) {
    vTaskDelay(pdMS_TO_TICKS(10));
  }

  if (s_scanning) {
    trackers_scanner_stop();
    s_scanning = false;
  }

  if (gps_hw_get_state()) {
    gps_module_stop_read();
    gps_module_unregister_cb();
  }
  portENTER_CRITICAL(&s_gps_mux);
  s_have_gps = false;
  portEXIT_CRITICAL(&s_gps_mux);

  if (s_trackers != NULL) {
    free(s_trackers);
    s_trackers = NULL;
  }
  s_tracker_count = 0;

  menus_module_set_default_input();
  oled_screen_clear();
  menus_module_restart();
}

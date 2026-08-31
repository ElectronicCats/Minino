// SPDX-License-Identifier: GPL-3.0-or-later
#include "trackers_module.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "gps_hw.h"
#include "gps_module.h"
#include "menus_module.h"
#include "oled_screen.h"
#include "sd_card.h"
#include "trackers_scanner.h"
#include "trackers_screens.h"

#define TAG               "trackers_app"
#define TRACKERS_DIR_NAME SD_CARD_PATH "/trackers"
#define TRACKERS_CSV_NAME TRACKERS_DIR_NAME "/trackers.csv"
#define TRACKERS_CSV_HEADER                                                    \
  "MAC,Name,Vendor,RSSI,DistanceMeters,FirstSeen,Latitude,Longitude,Altitude," \
  "ValidGPS\n"

static bool s_app_running = false;
static bool s_scanning = false;
static bool s_in_help = false;
static uint8_t s_help_page = 0;
static TaskHandle_t s_gui_task = NULL;

static tracker_profile_t* s_trackers = NULL;
static uint16_t s_tracker_count = 0;
static uint16_t s_last_index = 0;
static SemaphoreHandle_t s_trackers_mutex = NULL;
static bool s_sd_logging = false;

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

static const char* tracker_date_str(const gps_t* gps) {
  static char buf[32];
  if (gps != NULL && (gps->valid || gps->date.year > 0)) {
    uint16_t year = gps->date.year;
    if (year < 100) {
      year += 2000;
    }
    snprintf(buf, sizeof(buf), "%04u-%02u-%02u %02u:%02u:%02u", year,
             gps->date.month, gps->date.day, gps->tim.hour, gps->tim.minute,
             gps->tim.second);
    return buf;
  }
  snprintf(buf, sizeof(buf), "uptime_%llu",
           (unsigned long long) (esp_timer_get_time() / 1000));
  return buf;
}

static void log_tracker_to_sd(const tracker_profile_t* record,
                              const gps_t* gps) {
  if (!s_sd_logging || record == NULL) {
    return;
  }
  // Guardar datos únicamente cuando el GPS haya triangulado una posición válida
  if (gps == NULL || !gps->valid ||
      (gps->latitude == 0.0f && gps->longitude == 0.0f)) {
    return;
  }

  char lat[16] = "";
  char lon[16] = "";
  double alt = gps->altitude;

  snprintf(lat, sizeof(lat), "%.6f", gps->latitude);
  snprintf(lon, sizeof(lon), "%.6f", gps->longitude);

  char line[256];
  snprintf(line, sizeof(line),
           "%02X:%02X:%02X:%02X:%02X:%02X,%s,%s,%d,%.1f,%s,%s,%s,%.1f,%s\n",
           record->mac_address[0], record->mac_address[1],
           record->mac_address[2], record->mac_address[3],
           record->mac_address[4], record->mac_address[5],
           record->name ? record->name : "Unknown",
           record->vendor ? record->vendor : "Unknown", record->rssi,
           record->distance, tracker_date_str(gps), lat, lon, alt, "true");

  sd_card_append_to_file(TRACKERS_CSV_NAME, line);
}

static void on_tracker_found(tracker_profile_t record) {
  if (!s_app_running) {
    return;
  }

  record.gps_latitude = 0.0;
  record.gps_longitude = 0.0;
  record.gps_valid = false;

  gps_t gps_snap;
  bool have_gps = false;

  portENTER_CRITICAL(&s_gps_mux);
  if (s_have_gps) {
    gps_snap = s_latest_gps;
    have_gps = true;
    record.gps_latitude = s_latest_gps.latitude;
    record.gps_longitude = s_latest_gps.longitude;
    record.gps_valid = s_latest_gps.valid;
  }
  portEXIT_CRITICAL(&s_gps_mux);

  record.distance = trackers_scanner_rssi_to_distance(record.rssi);

  if (s_trackers_mutex != NULL &&
      xSemaphoreTake(s_trackers_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    int idx = trackers_scanner_find_profile_by_mac(s_trackers, s_tracker_count,
                                                   record.mac_address);
    if (idx == -1) {
      trackers_scanner_add_tracker_profile(&s_trackers, &s_tracker_count,
                                           record);
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
    xSemaphoreGive(s_trackers_mutex);
  }

  log_tracker_to_sd(&record, have_gps ? &gps_snap : NULL);
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
      uint16_t count = 0;

      if (s_trackers_mutex != NULL &&
          xSemaphoreTake(s_trackers_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        count = s_tracker_count;
        if (s_tracker_count > 0 && s_last_index < s_tracker_count &&
            s_trackers != NULL) {
          name = s_trackers[s_last_index].name;
          vendor = s_trackers[s_last_index].vendor;
          rssi = s_trackers[s_last_index].rssi;
          distance = s_trackers[s_last_index].distance;
          gps_valid = s_trackers[s_last_index].gps_valid;
          gps_lat = s_trackers[s_last_index].gps_latitude;
          gps_lon = s_trackers[s_last_index].gps_longitude;
        }
        xSemaphoreGive(s_trackers_mutex);
      }

      trackers_screens_show_status(count, name, vendor, rssi, distance,
                                   gps_valid, gps_lat, gps_lon, s_scanning);
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
  s_sd_logging = false;

  if (s_trackers_mutex == NULL) {
    s_trackers_mutex = xSemaphoreCreateMutex();
  }

  if (s_trackers_mutex != NULL) {
    xSemaphoreTake(s_trackers_mutex, portMAX_DELAY);
    if (s_trackers != NULL) {
      free(s_trackers);
      s_trackers = NULL;
    }
    xSemaphoreGive(s_trackers_mutex);
  }

  // Mount SD card and prepare log file
  if (sd_card_mount() == ESP_OK) {
    if (sd_card_create_dir(TRACKERS_DIR_NAME) == ESP_OK) {
      struct stat st;
      if (stat(TRACKERS_CSV_NAME, &st) != 0 || st.st_size == 0) {
        sd_card_append_to_file(TRACKERS_CSV_NAME, (char*) TRACKERS_CSV_HEADER);
      }
      s_sd_logging = true;
      ESP_LOGI(TAG, "Trackers logging to SD initialized at %s",
               TRACKERS_CSV_NAME);
    }
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

  if (s_trackers_mutex != NULL) {
    xSemaphoreTake(s_trackers_mutex, portMAX_DELAY);
    if (s_trackers != NULL) {
      free(s_trackers);
      s_trackers = NULL;
    }
    s_tracker_count = 0;
    xSemaphoreGive(s_trackers_mutex);
  }

  s_sd_logging = false;

  menus_module_set_default_input();
  oled_screen_clear();
  menus_module_restart();
}

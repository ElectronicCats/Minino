#include "esp_log.h"
#include "stdint.h"

#include "gps_module.h"
#include "gps_screens.h"
#include "menus_module.h"
#include "oled_screen.h"
#include "preferences.h"
#include "wardriving_module.h"

// #define TIME_ZONE (+3)    // Mexico City time zone
#define YEAR_BASE (2000)  // date in GPS starts from 2000

static const char* TAG = "gps_module";

nmea_parser_handle_t nmea_hdl = NULL;
gps_event_callback_t gps_event_callback = NULL;
static bool is_uart_installed = false;

/**
 * @brief Signal strength levels based on the number of satellites in use
 */
const char* GPS_SIGNAL_STRENGTH[] = {"None", "Weak", "Moderate", "Strong"};

/**
 * @brief Time zones in UTC
 *
 * @note Source: List of UTC offsets
 * https://en.wikipedia.org/wiki/List_of_UTC_offsets
 */
const float TIME_ZONES[] = {-12.0, -11.0, -10.0, -9.5,  -9.0, -8.0, -7.0, -6.0,
                            -5.0,  -4.0,  -3.5,  -3.0,  -2.0, -1.0, 0.0,  1.0,
                            2.0,   3.0,   3.5,   4.0,   4.5,  5.0,  5.5,  5.75,
                            6.0,   6.5,   7.0,   8.0,   8.75, 9.0,  9.5,  10.0,
                            10.5,  11.0,  12.0,  12.75, 13.0, 14.0};
static void gps_module_general_data_input_cb(uint8_t button_name,
                                             uint8_t button_event);
/**
 * @brief GPS Event Handler
 *
 * @param event_handler_arg handler specific arguments
 * @param event_base event base, here is fixed to ESP_NMEA_EVENT
 * @param event_id event id
 * @param event_data event specific arguments
 */
static void gps_event_handler(void* event_handler_arg,
                              esp_event_base_t event_base,
                              int32_t event_id,
                              void* event_data) {
  switch (event_id) {
    case GPS_UPDATE:
      /* update GPS information */
      gps_t* gps = gps_module_get_instance(event_data);
      if (gps_event_callback != NULL) {
        gps_event_callback(gps);
        return;
      }
      gps_screens_update_handler(gps);
      break;
    case GPS_UNKNOWN:
      /* print unknown statements */
      ESP_LOGW(TAG, "Unknown statement:%s", (char*) event_data);
      break;
    default:
      break;
  }
}

void gps_module_start_scan() {
#if !defined(CONFIG_GPS_MODULE_DEBUG)
  esp_log_level_set(TAG, ESP_LOG_NONE);
#endif
  if (is_uart_installed) {
    return;
  }
  is_uart_installed = true;

  ESP_LOGI(TAG, "Start reading GPS");
  /* NMEA parser configuration */
  nmea_parser_config_t config = NMEA_PARSER_CONFIG_DEFAULT();
  /* init NMEA parser library */
  nmea_hdl = nmea_parser_init(&config);
  /* register event handler for NMEA parser library */
  nmea_parser_add_handler(nmea_hdl, gps_event_handler, NULL);
  gps_screens_show_waiting_signal();
}

void gps_module_stop_read() {
  is_uart_installed = false;
  ESP_LOGI(TAG, "Stop reading GPS");
  /* unregister event handler */
  nmea_parser_remove_handler(nmea_hdl, gps_event_handler);
  /* deinit NMEA parser library */
  nmea_parser_deinit(nmea_hdl);
}

char* gps_module_get_signal_strength(gps_t* gps) {
  if (gps->sats_in_use == 0) {
    return (char*) GPS_SIGNAL_STRENGTH[0];
  } else if (gps->sats_in_use >= 1 && gps->sats_in_use <= 4) {
    return (char*) GPS_SIGNAL_STRENGTH[1];
  } else if (gps->sats_in_use >= 5 && gps->sats_in_use <= 8) {
    return (char*) GPS_SIGNAL_STRENGTH[2];
  } else {
    return (char*) GPS_SIGNAL_STRENGTH[3];
  }
}

gps_t* gps_module_get_instance(void* event_data) {
  gps_t* gps = (gps_t*) event_data;

  uint16_t year = gps->date.year + YEAR_BASE;
  uint8_t month = gps->date.month;
  uint8_t day = gps->date.day;

  uint8_t time_zone = gps_module_get_time_zone();
  float timeZoneValue = TIME_ZONES[time_zone];
  int8_t hour_offset = (int8_t) timeZoneValue;
  int8_t minute_offset = (int8_t) ((timeZoneValue - (float) hour_offset) * 60);

  uint8_t hour = gps->tim.hour;
  if (hour_offset < 0 && hour < abs(hour_offset)) {
    day--;
    hour = 24 + hour_offset + hour;
  } else {
    hour += hour_offset;
  }

  uint8_t minute = gps->tim.minute;
  if (minute_offset < 0 && minute < abs(minute_offset)) {
    hour--;
    minute = 60 + minute_offset + minute;
  } else {
    minute += minute_offset;
  }

  uint8_t second = gps->tim.second;
  second = second > 60 ? second - 60 : second;

  gps->tim.hour = hour;
  gps->tim.minute = minute;
  gps->tim.second = second;
  gps->date.year = year;
  gps->date.month = month;
  gps->date.day = day;

  ESP_LOGI(TAG,
           "%d/%d/%d %d:%d:%d => \r\n"
           "\t\t\t\t\t\tSats in use: %d\r\n"
           "\t\t\t\t\t\tSats in view: %d\r\n"
           "\t\t\t\t\t\tValid: %s\r\n"
           "\t\t\t\t\t\tlatitude   = %.05f°N\r\n"
           "\t\t\t\t\t\tlongitude = %.05f°E\r\n"
           "\t\t\t\t\t\taltitude   = %.02fm\r\n"
           "\t\t\t\t\t\tspeed      = %.02fm/s",
           gps->date.year, gps->date.month, gps->date.day, gps->tim.hour,
           gps->tim.minute, gps->tim.second, gps->sats_in_use,
           gps->sats_in_view, gps->valid ? "true" : "false", gps->latitude,
           gps->longitude, gps->altitude, gps->speed);

  return gps;
}

uint8_t gps_module_get_time_zone() {
  uint8_t default_time_zone = 14;  // UTC+0
  return preferences_get_uint("time_zone", default_time_zone);
}

void gps_module_set_time_zone(uint8_t time_zone) {
  preferences_put_uint("time_zone", time_zone);
}

void gps_module_register_cb(gps_event_callback_t callback) {
  gps_module_unregister_cb();
  gps_event_callback = callback;
}

void gps_module_unregister_cb() {
  gps_event_callback = NULL;
}

void gps_module_general_data_run() {
  menus_module_set_app_state(true, gps_module_general_data_input_cb);
  gps_module_start_scan();
}

static void gps_module_general_data_input_cb(uint8_t button_name,
                                             uint8_t button_event) {
  if (button_event != BUTTON_PRESS_DOWN || button_name != BUTTON_LEFT) {
    return;
  }
  gps_module_stop_read();
  menus_module_exit_app();
}
#include "esp_log.h"
#include "stdint.h"

#include "gps_module.h"
#include "menu_screens_modules.h"
#include "nmea_parser.h"
#include "oled_screen.h"
#include "preferences.h"

// #define TIME_ZONE (+3)    // Mexico City time zone
#define YEAR_BASE (2000)  // date in GPS starts from 2000

static const char* TAG = "gps_module";

nmea_parser_handle_t nmea_hdl = NULL;

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
  screen_module_menu_t current_menu = menu_screens_get_current_menu();

  if (current_menu != MENU_GPS_DATE_TIME && current_menu != MENU_GPS_LOCATION) {
    return;
  }

  gps_t* gps = NULL;
  switch (event_id) {
    case GPS_UPDATE:
      gps = (gps_t*) event_data;
      /* print information parsed from GPS statements */
      ESP_LOGI(TAG,
               "%d/%d/%d %d:%d:%d => \r\n"
               "\t\t\t\t\t\tlatitude   = %.05f°N\r\n"
               "\t\t\t\t\t\tlongitude = %.05f°E\r\n"
               "\t\t\t\t\t\taltitude   = %.02fm\r\n"
               "\t\t\t\t\t\tspeed      = %fm/s",
               gps->date.year + YEAR_BASE, gps->date.month, gps->date.day,
               gps->tim.hour, gps->tim.minute, gps->tim.second, gps->latitude,
               gps->longitude, gps->altitude, gps->speed);

      uint16_t year = gps->date.year + YEAR_BASE;
      uint8_t month = gps->date.month;
      uint8_t day = gps->date.day;

      uint8_t time_zone = gps_module_get_time_zone();
      ESP_LOGI(TAG, "Time zone: %d", time_zone);
      float timeZoneValue = TIME_ZONES[time_zone];
      ESP_LOGI(TAG, "Time zone value: %f", timeZoneValue);
      int8_t hour_offset = (int8_t) timeZoneValue;
      ESP_LOGI(TAG, "Hour offset: %d", hour_offset);
      int8_t minute_offset =
          (int8_t) ((timeZoneValue - (float) hour_offset) * 60);
      ESP_LOGI(TAG, "Minute offset: %d", minute_offset);

      uint8_t hour = gps->tim.hour + hour_offset;
      hour = hour > 24 ? hour - 24 : hour;
      uint8_t minute = gps->tim.minute;
      if (minute_offset < 0 && minute < abs(minute_offset)) {
        hour--;
        minute = 60 + minute_offset + minute;
      } else {
        minute += minute_offset;
      }
      // minute = minute > 60 ? minute - 60 : minute;
      uint8_t second = gps->tim.second;
      second = second > 60 ? second - 60 : second;

      ESP_LOGI(TAG, "Satellites in use: %d", gps->sats_in_use);
      ESP_LOGI(TAG, "Satellites in view: %d", gps->sats_in_view);
      ESP_LOGI(TAG,
               "%d/%d/%d %d:%d:%d => \r\n"
               "\t\t\t\t\t\tlatitude   = %.05f°N\r\n"
               "\t\t\t\t\t\tlongitude = %.05f°E\r\n"
               "\t\t\t\t\t\taltitude   = %.02fm\r\n"
               "\t\t\t\t\t\tspeed      = %.02fm/s",
               year, month, day, hour, minute, second, gps->latitude,
               gps->longitude, gps->altitude, gps->speed);

      if (current_menu == MENU_GPS_DATE_TIME) {
        char* date_str = (char*) malloc(20);
        char* time_str = (char*) malloc(20);

        sprintf(date_str, "Date: %d/%d/%d", year, month, day);
        // TODO: fix time +24
        sprintf(time_str, "Time: %d:%d:%d", hour, minute, second);

        oled_screen_clear();
        oled_screen_display_text("GPS Date/Time", 0, 0, OLED_DISPLAY_INVERT);
        // TODO: refresh only the date and time
        oled_screen_display_text(date_str, 0, 2, OLED_DISPLAY_NORMAL);
        oled_screen_display_text(time_str, 0, 3, OLED_DISPLAY_NORMAL);
      } else if (current_menu == MENU_GPS_LOCATION) {
        char* latitude_str = (char*) malloc(22);
        char* longitude_str = (char*) malloc(22);
        char* altitude_str = (char*) malloc(22);
        char* speed_str = (char*) malloc(22);

        // TODO: add ° symbol
        sprintf(latitude_str, "  %.05fN", gps->latitude);
        sprintf(longitude_str, "  %.05fE", gps->longitude);
        sprintf(altitude_str, "Alt: %.02fm", gps->altitude);
        sprintf(speed_str, "Speed: %.02fm/s", gps->speed);

        oled_screen_clear();
        oled_screen_display_text("GPS Location", 0, 0, OLED_DISPLAY_INVERT);
        oled_screen_display_text("Latitude", 0, 2, OLED_DISPLAY_NORMAL);
        oled_screen_display_text(latitude_str, 0, 3, OLED_DISPLAY_NORMAL);
        oled_screen_display_text("Longitude", 0, 4, OLED_DISPLAY_NORMAL);
        oled_screen_display_text(longitude_str, 0, 5, OLED_DISPLAY_NORMAL);
        oled_screen_display_text(altitude_str, 0, 6, OLED_DISPLAY_NORMAL);

        // TODO: add speed menu
        // oled_screen_display_text(speed_str, 0, 5, OLED_DISPLAY_NORMAL);
      }
      break;
    case GPS_UNKNOWN:
      /* print unknown statements */
      ESP_LOGW(TAG, "Unknown statement:%s", (char*) event_data);
      break;
    default:
      break;
  }
}

void gps_module_exit_submenu_cb() {
  screen_module_menu_t current_menu = menu_screens_get_current_menu();

  switch (current_menu) {
    case MENU_GPS:
      gps_module_exit();
      menu_screens_unregister_submenu_cbs();
      break;
    default:
      break;
  }
}

void gps_module_begin() {
  /* NMEA parser configuration */
  nmea_parser_config_t config = NMEA_PARSER_CONFIG_DEFAULT();
  /* init NMEA parser library */
  nmea_hdl = nmea_parser_init(&config);
  /* register event handler for NMEA parser library */
  nmea_parser_add_handler(nmea_hdl, gps_event_handler, NULL);

  menu_screens_register_exit_submenu_cb(gps_module_exit_submenu_cb);
}

void gps_module_exit() {
  /* unregister event handler */
  nmea_parser_remove_handler(nmea_hdl, gps_event_handler);
  /* deinit NMEA parser library */
  nmea_parser_deinit(nmea_hdl);
}

uint8_t gps_module_get_time_zone() {
  uint8_t default_time_zone = 14;  // UTC+0
  return preferences_get_uint("time_zone", default_time_zone);
}

void gps_module_set_time_zone(uint8_t time_zone) {
  preferences_put_uint("time_zone", time_zone);
}

#include "gps_module.h"
#include "esp_log.h"
#include "nmea_parser.h"
#include "stdint.h"

#include "menu_screens_modules.h"
#include "oled_screen.h"

#define TIME_ZONE (-6)    // Mexico City time zone
#define YEAR_BASE (2000)  // date in GPS starts from 2000

static const char* TAG = "gps_module";

nmea_parser_handle_t nmea_hdl = NULL;

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
               gps->tim.hour + TIME_ZONE, gps->tim.minute, gps->tim.second,
               gps->latitude, gps->longitude, gps->altitude, gps->speed);

      uint16_t year = gps->date.year + YEAR_BASE;
      uint8_t month = gps->date.month;
      uint8_t day = gps->date.day;
      uint8_t hour = gps->tim.hour + TIME_ZONE;
      hour = hour > 24 ? hour - 24 : hour;
      uint8_t minute = gps->tim.minute;
      minute = minute > 60 ? minute - 60 : minute;
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

void gps_module_begin() {
  /* NMEA parser configuration */
  nmea_parser_config_t config = NMEA_PARSER_CONFIG_DEFAULT();
  /* init NMEA parser library */
  nmea_hdl = nmea_parser_init(&config);
  /* register event handler for NMEA parser library */
  nmea_parser_add_handler(nmea_hdl, gps_event_handler, NULL);
}

void gps_module_exit() {
  /* unregister event handler */
  nmea_parser_remove_handler(nmea_hdl, gps_event_handler);
  /* deinit NMEA parser library */
  nmea_parser_deinit(nmea_hdl);
}

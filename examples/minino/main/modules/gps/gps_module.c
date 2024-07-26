#include "esp_log.h"
#include "stdint.h"

#include "gps_module.h"
#include "menu_screens_modules.h"
#include "oled_screen.h"
#include "preferences.h"
#include "wardriving_module.h"

// #define TIME_ZONE (+3)    // Mexico City time zone
#define YEAR_BASE (2000)  // date in GPS starts from 2000

static const char* TAG = "gps_module";

nmea_parser_handle_t nmea_hdl = NULL;
gps_event_callback_t gps_event_callback = NULL;

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

char* get_signal_strength(gps_t* gps) {
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

void update_date_and_time(gps_t* gps) {
  char* signal_str = (char*) malloc(20);
  char* date_str = (char*) malloc(20);
  char* time_str = (char*) malloc(20);

  sprintf(signal_str, "Signal: %s", get_signal_strength(gps));
  sprintf(date_str, "Date: %d/%d/%d", gps->date.year, gps->date.month,
          gps->date.day);
  sprintf(time_str, "Time: %d:%d:%d", gps->tim.hour, gps->tim.minute,
          gps->tim.second);

  gps_date_time_items[1] = signal_str;
  gps_date_time_items[3] = date_str;
  gps_date_time_items[4] = time_str;
}

void update_location(gps_t* gps) {
  char* signal_str = (char*) malloc(20);
  char* latitude_str = (char*) malloc(22);
  char* longitude_str = (char*) malloc(22);
  char* altitude_str = (char*) malloc(22);
  char* speed_str = (char*) malloc(22);

  // TODO: add ° symbol
  sprintf(signal_str, "Signal: %s", get_signal_strength(gps));
  sprintf(latitude_str, "  %.05f N", gps->latitude);
  sprintf(longitude_str, "  %.05f E", gps->longitude);
  sprintf(altitude_str, "  %.04fm", gps->altitude);

  gps_location_items[1] = signal_str;
  gps_location_items[4] = latitude_str;
  gps_location_items[6] = longitude_str;
  gps_location_items[8] = altitude_str;
}

void update_speed(gps_t* gps) {
  char* signal_str = (char*) malloc(20);
  char* speed_str = (char*) malloc(22);

  sprintf(signal_str, "Signal: %s", get_signal_strength(gps));
  sprintf(speed_str, "Speed: %.02fm/s", gps->speed);

  gps_speed_items[1] = signal_str;
  gps_speed_items[3] = speed_str;
}

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

  if (current_menu == MENU_GPS) {
    return;
  }

  switch (event_id) {
    case GPS_UPDATE:
      if (gps_event_callback != NULL) {
        gps_event_callback(event_data);
        return;
      }
      /* update GPS information */
      gps_t* gps = gps_module_get_instance(event_data);

      update_date_and_time(gps);
      update_location(gps);
      update_speed(gps);
      menu_screens_display_menu();
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

  ESP_LOGI(TAG, "Start reading GPS");
  /* NMEA parser configuration */
  nmea_parser_config_t config = NMEA_PARSER_CONFIG_DEFAULT();
  /* init NMEA parser library */
  nmea_hdl = nmea_parser_init(&config);
  /* register event handler for NMEA parser library */
  nmea_parser_add_handler(nmea_hdl, gps_event_handler, NULL);
}

void gps_module_stop_read() {
  ESP_LOGI(TAG, "Stop reading GPS");
  /* unregister event handler */
  nmea_parser_remove_handler(nmea_hdl, gps_event_handler);
  /* deinit NMEA parser library */
  nmea_parser_deinit(nmea_hdl);
}

void gps_module_exit_submenu_cb() {
  screen_module_menu_t current_menu = menu_screens_get_current_menu();

  switch (current_menu) {
    // case MENU_GPS_WARDRIVING:
    //   wardriving_module_end();
    //   break;
    case MENU_GPS_WARDRIVING_START:
      wardriving_module_stop_scan();
      break;
    case MENU_GPS:
      menu_screens_unregister_submenu_cbs();
      break;
    case MENU_GPS_DATE_TIME:
    case MENU_GPS_LOCATION:
    case MENU_GPS_SPEED:
      gps_module_stop_read();
      break;
    default:
      break;
  }
}

void gps_module_enter_submenu_cb(screen_module_menu_t user_selection) {
  switch (user_selection) {
    // case MENU_GPS_WARDRIVING:
    //   wardriving_module_begin();
    //   break;
    case MENU_GPS_WARDRIVING_START:
      wardriving_module_start_scan();
      break;
    case MENU_GPS_DATE_TIME:
    case MENU_GPS_LOCATION:
    case MENU_GPS_SPEED:
      gps_module_start_scan();
      break;
    default:
      break;
  }
}

void gps_module_begin() {
#if !defined(CONFIG_GPS_MODULE_DEBUG)
  esp_log_level_set(TAG, ESP_LOG_NONE);
#endif
  menu_screens_register_exit_submenu_cb(gps_module_exit_submenu_cb);
  menu_screens_register_enter_submenu_cb(gps_module_enter_submenu_cb);
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

  ESP_LOGI(TAG, "Satellites in use: %d", gps->sats_in_use);
  ESP_LOGI(TAG, "Satellites in view: %d", gps->sats_in_view);
  ESP_LOGI(TAG,
           "%d/%d/%d %d:%d:%d => \r\n"
           "\t\t\t\t\t\tlatitude   = %.05f°N\r\n"
           "\t\t\t\t\t\tlongitude = %.05f°E\r\n"
           "\t\t\t\t\t\taltitude   = %.02fm\r\n"
           "\t\t\t\t\t\tspeed      = %.02fm/s",
           gps->date.year, gps->date.month, gps->date.day, gps->tim.hour,
           gps->tim.minute, gps->tim.second, gps->latitude, gps->longitude,
           gps->altitude, gps->speed);

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

#include "esp_log.h"
#include "stdint.h"

#include "animations_task.h"
#include "general_animations.h"
#include "general_notification.h"
#include "general_radio_selection.h"
#include "gps_hw.h"
#include "gps_module.h"
#include "gps_screens.h"
#include "menus_module.h"
#include "modals_module.h"
#include "oled_screen.h"
#include "preferences.h"
#include "wardriving_module.h"

#define YEAR_BASE (2000)  // date in GPS starts from 2000

static const char* TAG = "gps_module";

nmea_parser_handle_t nmea_hdl = NULL;
gps_event_callback_t gps_event_callback = NULL;
static bool is_uart_installed = false;

void gps_module_check_state();

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

/**
 * @brief GPS Satellites Event Handler
 *
 * @param event_handler_arg handler specific arguments
 * @param event_base event base, here is fixed to ESP_NMEA_EVENT
 * @param event_id event id
 * @param event_data event specific arguments
 *
 * @return void
 */
static void gps_sats_event_handler(void* event_handler_arg,
                                   esp_event_base_t event_base,
                                   int32_t event_id,
                                   void* event_data) {
  switch (event_id) {
    case GPS_UPDATE:
      /* update GPS information */
      (void) gps_module_get_instance(event_data);
      break;
    case GPS_UNKNOWN:
      /* print unknown statements */
      ESP_LOGW(TAG, "Unknown statement:%s", (char*) event_data);
      break;
    default:
      break;
  }
}

/**
 * @brief Reset GPS test
 *
 * @return void
 */
void gps_module_reset_test(void) {
  animations_task_run(&general_animation_loading, 300, NULL);
  for (int i = 0; i < 5; i++) {
    /* NMEA parser configuration */
    nmea_parser_config_t config = NMEA_PARSER_CONFIG_DEFAULT();
    /* init NMEA parser library */
    nmea_hdl = nmea_parser_init(&config);
    nmea_parser_add_handler(nmea_hdl, gps_sats_event_handler, NULL);
    vTaskDelay(3000 / portTICK_PERIOD_MS);
    nmea_parser_remove_handler(nmea_hdl, gps_sats_event_handler);
    /* deinit NMEA parser library */
    nmea_parser_deinit(nmea_hdl);
    vTaskDelay(3000 / portTICK_PERIOD_MS);
  }
  animations_task_stop();
  gps_module_start_scan();
}

/**
 * @brief Start reading the GPS module
 *
 * @return void
 */
void gps_module_start_scan() {
  if (is_uart_installed) {
    return;
  }
  is_uart_installed = true;

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

  gps_screens_show_waiting_signal();
}

/**
 * @brief Stop reading the GPS module
 *
 * @return void
 */
void gps_module_stop_read() {
  is_uart_installed = false;
  ESP_LOGI(TAG, "Stop reading GPS");

  /* Unregister event handler first */
  nmea_parser_remove_handler(nmea_hdl, gps_event_handler);

  /* Small delay to ensure any pending callback finishes
   * The GPS task runs every 5ms, so 20ms ensures we miss at least
   * one full cycle after unregistering */
  vTaskDelay(pdMS_TO_TICKS(20));

  /* Now safe to deinit NMEA parser library */
  nmea_parser_deinit(nmea_hdl);
}

/**
 * @brief Get the signal strength
 *
 * @param gps The GPS module instance
 *
 * @return char*
 */
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

/**
 * @brief Get the GPS module instance
 *
 * @param event_data The event data
 *
 * @return gps_t*
 */
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
           "%d/%d/%d %d:%d:%d => "
           " Sats in use: %d"
           " Sats in view: %d"
           " Valid: %s"
           " latitude   = %.05f°N"
           " longitude = %.05f°E"
           " altitude   = %.02fm"
           " speed      = %.02fm/s",
           gps->date.year, gps->date.month, gps->date.day, gps->tim.hour,
           gps->tim.minute, gps->tim.second, gps->sats_in_use,
           gps->sats_in_view, gps->valid ? "true" : "false", gps->latitude,
           gps->longitude, gps->altitude, gps->speed);

  return gps;
}

/**
 * @brief Get the GPS module time zone
 *
 * @return uint8_t
 */
uint8_t gps_module_get_time_zone() {
  uint8_t default_time_zone = 14;  // UTC+0
  return preferences_get_uint("time_zone", default_time_zone);
}

/**
 * @brief Get the full date time
 *
 * @param gps The GPS module instance
 *
 * @return char*
 */
char* get_full_date_time(gps_t* gps) {
  char* date_time = malloc(30);
  snprintf(date_time, 30, "%d-%d-%d %d:%d:%d", gps->date.year, gps->date.month,
           gps->date.day, gps->tim.hour, gps->tim.minute, gps->tim.second);
  return date_time;
}

/**
 * @brief Get the string date time
 *
 * @param gps The GPS module instance
 *
 * @return char*
 */
char* get_str_date_time(gps_t* gps) {
  char* date_time = malloc(30);
  snprintf(date_time, 30, "%d%d%d%d%d%d", gps->date.year, gps->date.month,
           gps->date.day, gps->tim.hour, gps->tim.minute, gps->tim.second);
  return date_time;
}

/**
 * @brief Set the GPS module time zone
 *
 * @param time_zone The time zone
 *
 * @return void
 */
void gps_module_set_time_zone(uint8_t time_zone) {
  preferences_put_uint("time_zone", time_zone);
}

/**
 * @brief Register the GPS module event callback
 *
 * @param callback The callback
 *
 * @return void
 */
void gps_module_register_cb(gps_event_callback_t callback) {
  gps_module_unregister_cb();
  gps_event_callback = callback;
}

/**
 * @brief Unregister the GPS module event callback
 *
 * @return void
 */
void gps_module_unregister_cb() {
  gps_event_callback = NULL;
}

/**
 * @brief On config enter
 *
 * @return void
 */
void gps_module_on_config_enter() {
  gps_screens_show_config();
}

/**
 * @brief On test enter
 *
 * @return void
 */
void gps_module_on_test_enter(void) {
  menus_module_set_app_state(true, gps_module_general_data_input_cb);
  gps_module_reset_test();
}

/**
 * @brief General data run
 *
 * @return void
 */
void gps_module_general_data_run() {
  menus_module_set_app_state(true, gps_module_general_data_input_cb);
  gps_module_start_scan();
}

/**
 * @brief General data input callback
 *
 * @param button_name The button name
 * @param button_event The button event
 *
 * @return void
 */
static void gps_module_general_data_input_cb(uint8_t button_name,
                                             uint8_t button_event) {
  if (button_event != BUTTON_PRESS_DOWN || button_name != BUTTON_LEFT) {
    return;
  }
  gps_module_stop_read();
  menus_module_exit_app();
}

/**
 * @brief GPS states
 *
 * @return const char*
 */
static const char* gps_states[] = {
    "Disabled",
    "Enabled",
};

/**
 * @brief State handler
 *
 * @param state The state
 *
 * @return void
 */
static void state_handler(uint8_t state) {
  if (state) {
    gps_hw_on();
  } else {
    gps_hw_off();
  }
  gsp_hw_save_state();
}

/**
 * @brief GPS settings exit callback
 *
 * @return void
 */
static void gps_settings_exit_cb() {
  if (gps_hw_get_state()) {
    menus_module_set_default_input();
    menus_module_refresh();
  } else {
    menus_module_exit_app();
  }
}

/**
 * @brief GPS settings state menu, function to show the GPS state menu
 *
 * @return void
 */
static void gps_settings_state_menu() {
  general_radio_selection_menu_t state = {0};
  state.banner = "GPS State";
  state.options = gps_states;
  state.options_count = sizeof(gps_states) / sizeof(char*);
  state.current_option = gps_hw_get_state();
  state.select_cb = state_handler;
  state.style = RADIO_SELECTION_OLD_STYLE;
  state.exit_cb = gps_settings_exit_cb;

  general_radio_selection(state);
}

/**
 * @brief Check the GPS state
 *
 * @return void
 */
void gps_module_check_state() {
  if (gps_hw_get_state()) {
    return;
  }

  general_notification_ctx_t notification = {0};
  notification.head = "GPS Disabled";
  notification.body = "Enable it    first";
  notification.duration_ms = 2000;
  general_notification(notification);

  gps_settings_state_menu();
}

/**
 * @brief Reset the GPS state
 *
 * @return void
 */
void gps_module_reset_state() {
  if (gps_hw_get_state() && !preferences_get_bool(GPS_ENABLED_MEM, false)) {
    gps_hw_off();
  }
}

/**
 * @brief Reconfigure GPS with specific option type if GPS is active
 *
 * This function checks if GPS is currently active (UART installed by NMEA
 * parser). If active, it directly configures the GPS options without touching
 * UART. If not active, it uses gps_hw_init_preferences to temporarily configure
 * UART.
 *
 * @param init_type Type of configuration to apply (see enum in gps_hw.h)
 */
void gps_module_reconfigure_options(uint8_t init_type) {
  // Check if GPS is currently active (NMEA parser has UART installed)
  if (is_uart_installed && nmea_hdl != NULL) {
    // GPS is active, configure directly without touching UART
    ESP_LOGI(TAG, "GPS is active, reconfiguring option type: %d", init_type);
    gps_hw_configure_options(init_type);
  } else {
    // GPS is not active, use init_preferences to temporarily configure UART
    ESP_LOGI(TAG, "GPS is not active, using init_preferences for type: %d",
             init_type);
    gps_hw_init_preferences(init_type);
  }
}
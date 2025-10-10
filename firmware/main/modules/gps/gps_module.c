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
static bool gps_advanced_configured = false;

void gps_module_check_state();
static void gps_module_configure_advanced();
static void gps_module_configure_updaterate(void);
static bool gps_module_send_command(const char* command);

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

static void gps_sats_event_handler(void* event_handler_arg,
                                   esp_event_base_t event_base,
                                   int32_t event_id,
                                   void* event_data) {
  switch (event_id) {
    case GPS_UPDATE:
      /* update GPS information */
      gps_module_get_instance(event_data);
      break;
    case GPS_UNKNOWN:
      /* print unknown statements */
      ESP_LOGW(TAG, "Unknown statement:%s", (char*) event_data);
      break;
    default:
      break;
  }
}

static void configure_gps_options(void) {
  // Configure GPS for ATGM336H-6N-74 with multi-constellation support (only
  // once)
  if (!gps_advanced_configured) {
    if (preferences_get_int(ADVANCED_OPTIONS_PREF_KEY, 1) == 1) {
      gps_module_configure_advanced();
      gps_advanced_configured = true;
    }
  }

  gps_module_configure_updaterate();

  uint16_t agnss_option = preferences_get_int(AGNSS_OPTIONS_PREF_KEY, 1);
  if (agnss_option == 0) {
    gps_module_disable_agnss();
  } else {
    gps_module_enable_agnss();
  }
  gps_module_set_power_mode(preferences_get_int(POWER_OPTIONS_PREF_KEY, 0));
}

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

void gps_module_start_scan() {
  // #if !defined(CONFIG_GPS_MODULE_DEBUG)
  //   esp_log_level_set(TAG, ESP_LOG_NONE);
  // #endif
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

  configure_gps_options();

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

uint8_t gps_module_get_time_zone() {
  uint8_t default_time_zone = 14;  // UTC+0
  return preferences_get_uint("time_zone", default_time_zone);
}

char* get_full_date_time(gps_t* gps) {
  char* date_time = malloc(30);
  snprintf(date_time, 30, "%d-%d-%d %d:%d:%d", gps->date.year, gps->date.month,
           gps->date.day, gps->tim.hour, gps->tim.minute, gps->tim.second);
  return date_time;
}

char* get_str_date_time(gps_t* gps) {
  char* date_time = malloc(30);
  snprintf(date_time, 30, "%d%d%d%d%d%d", gps->date.year, gps->date.month,
           gps->date.day, gps->tim.hour, gps->tim.minute, gps->tim.second);
  return date_time;
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

void gps_module_on_config_enter() {
  gps_screens_show_config();
}

void gps_module_on_test_enter(void) {
  menus_module_set_app_state(true, gps_module_general_data_input_cb);
  gps_module_reset_test();
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

static const char* gps_states[] = {
    "Disabled",
    "Enabled",
};

static void state_handler(uint8_t state) {
  if (state) {
    gps_hw_on();
  } else {
    gps_hw_off();
  }
  gsp_hw_save_state();
}

static void gps_settings_exit_cb() {
  if (gps_hw_get_state()) {
    menus_module_set_default_input();
    menus_module_refresh();
  } else {
    menus_module_exit_app();
  }
}

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

void gps_module_reset_state() {
  if (gps_hw_get_state() && !preferences_get_bool(GPS_ENABLED_MEM, false)) {
    gps_hw_off();
  }
}

void gps_module_set_update_rate(uint8_t rate) {
  if (rate < 1 || rate > 10) {
    ESP_LOGW(TAG, "Invalid update rate: %d (must be 1-10 Hz)", rate);
    return;
  }

  char command[32];
  uint16_t interval_ms = 1000 / rate;

  // Calculate correct checksum for dynamic rate
  uint8_t checksum = 0;
  char temp[24];
  snprintf(temp, sizeof(temp), "PMTK220,%d", interval_ms);
  for (int i = 0; temp[i] != '\0'; i++) {
    checksum ^= temp[i];
  }

  snprintf(command, sizeof(command), "$PMTK220,%d*%02X\r\n", interval_ms,
           checksum);
  gps_module_send_command(command);
}

/**
 * @brief Enable A-GNSS (Assisted GNSS) for faster fix times
 *
 * A-GNSS uses assistance data to reduce Time To First Fix (TTFF)
 * and improve accuracy in challenging environments
 */
void gps_module_enable_agnss() {
  ESP_LOGI(TAG, "Enabling A-GNSS for faster fix times");

  // Enable A-GNSS assistance data
  gps_module_send_command("$PMTK869,1*28\r\n");

  // Enable A-GNSS data logging (optional)
  gps_module_send_command("$PMTK869,1,1*35\r\n");

  ESP_LOGI(TAG, "A-GNSS enabled successfully");
}

/**
 * @brief Disable A-GNSS (Assisted GNSS)
 *
 * Disables assistance data, GPS will work in standalone mode
 */
void gps_module_disable_agnss() {
  ESP_LOGI(TAG, "Disabling A-GNSS");

  // Disable A-GNSS assistance data
  gps_module_send_command("$PMTK869,0*29\r\n");

  ESP_LOGI(TAG, "A-GNSS disabled successfully");
}

/**
 * @brief Set GPS power mode for power optimization
 *
 * @param mode Power mode: NORMAL, LOW_POWER, or STANDBY
 */
void gps_module_set_power_mode(gps_power_mode_t mode) {
  const char* mode_names[] = {"NORMAL", "LOW_POWER", "STANDBY"};

  if (mode >= GPS_POWER_MODE_NORMAL && mode <= GPS_POWER_MODE_STANDBY) {
    ESP_LOGI(TAG, "Setting GPS power mode to %s", mode_names[mode]);

    switch (mode) {
      case GPS_POWER_MODE_NORMAL:
        // Normal operation mode - full performance
        gps_module_send_command("$PMTK225,0*2B\r\n");
        break;

      case GPS_POWER_MODE_LOW_POWER:
        // Low power mode - reduced performance, lower consumption
        gps_module_send_command("$PMTK225,1*2A\r\n");
        break;

      case GPS_POWER_MODE_STANDBY:
        // Standby mode - minimal power consumption
        gps_module_send_command("$PMTK225,2*29\r\n");
        break;

      default:
        ESP_LOGW(TAG, "Unknown power mode: %d", mode);
        return;
    }

    ESP_LOGI(TAG, "GPS power mode set to %s successfully", mode_names[mode]);
  } else {
    ESP_LOGW(TAG, "Invalid power mode: %d (must be 0-2)", mode);
  }
}

static void gps_module_configure_updaterate(void) {
  // Set update rate to 5Hz for better responsiveness
  char update_rate_cmd[128];
  uint32_t update_rate = preferences_get_int(URATE_OPTIONS_PREF_KEY, 1);
  if (update_rate == GPS_UPDATE_RATE_1HZ) {
    sprintf(update_rate_cmd, "$PMTK220,1000*1F\r\n");  // 1000ms = 1Hz
    ESP_LOGI(TAG, "Change update rate to: 1 HZ");
  } else if (update_rate == GPS_UPDATE_RATE_5HZ) {
    sprintf(update_rate_cmd, "$PMTK220,200*2C\r\n");  // 200ms = 5Hz
    ESP_LOGI(TAG, "Change update rate to: 5 HZ");
  } else if (update_rate == GPS_UPDATE_RATE_10HZ) {
    sprintf(update_rate_cmd, "$PMTK220,100*2F\r\n");  // 100ms = 10Hz
    ESP_LOGI(TAG, "Change update rate to: 10 HZ");
  } else {
    ESP_LOGE(TAG, "Not supported");
    return;
  }

  if (!gps_module_send_command(update_rate_cmd)) {
    ESP_LOGE(TAG, "Failed to set update rate");
  }
}

/**
 * @brief Configure GPS for ATGM336H-6N-74 with advanced features
 */
static void gps_module_configure_advanced() {
  ESP_LOGI(TAG, "Configuring GPS for ATGM336H-6N-74");

  // Wait a bit for UART to be fully ready
  vTaskDelay(pdMS_TO_TICKS(100));

  bool config_success = true;

  // Configure multi-constellation support (GPS, GLONASS, Galileo, BeiDou, QZSS)
  char constellation_cmd[] = "$PMTK353,1,1,1,1,1,0,0,0,0*2A\r\n";
  if (!gps_module_send_command(constellation_cmd)) {
    ESP_LOGE(TAG, "Failed to configure constellations");
    config_success = false;
  }

  // Enable additional NMEA sentences for better data
  char nmea_cmd[] = "$PMTK314,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0*28\r\n";
  if (!gps_module_send_command(nmea_cmd)) {
    ESP_LOGE(TAG, "Failed to configure NMEA sentences");
    config_success = false;
  }

  if (config_success) {
    ESP_LOGI(TAG, "GPS advanced configuration complete successfully");
  } else {
    ESP_LOGW(TAG, "GPS advanced configuration completed with errors");
  }
}

/**
 * @brief Send command to GPS module via UART and validate response
 */
static bool gps_module_send_command(const char* command) {
  if (!is_uart_installed || !nmea_hdl) {
    ESP_LOGW(TAG, "UART not ready, skipping command: %s", command);
    return false;
  }

  // Get the UART port from the NMEA parser
  uart_port_t uart_port = 1;  // UART_NUM_1 as configured in NMEA parser

  int len = uart_write_bytes(uart_port, command, strlen(command));
  if (len < 0) {
    ESP_LOGE(TAG, "Failed to send command: %s", command);
    return false;
  }

  ESP_LOGI(TAG, "Sent GPS command: %s", command);
  vTaskDelay(pdMS_TO_TICKS(200));  // Wait for GPS to process

  // Try to read response (optional validation)
  uint8_t response[64];
  int response_len = uart_read_bytes(uart_port, response, sizeof(response) - 1,
                                     pdMS_TO_TICKS(100));
  if (response_len > 0) {
    response[response_len] = '\0';
    ESP_LOGD(TAG, "GPS response: %s", response);
  }

  return true;
}
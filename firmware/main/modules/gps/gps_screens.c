#include "gps_screens.h"
#include "esp_log.h"
#include "sd_card.h"

#include "general_radio_selection.h"
#include "general_screens.h"
#include "general_scrolling_text.h"
#include "general_submenu.h"
#include "menus_module.h"
#include "oled_screen.h"
#include "preferences.h"

static char* gps_route_file_name = NULL;
static char* gps_route_file_buffer = NULL;
static uint16_t gps_route_lines = 0;
static bool gps_route_recording = false;
static uint16_t gps_route_points_saved = 0;

static const char* TAG = "route";

static const char* config_menu_options[] = {"AGNSS", "Power mode", "Advanced",
                                            "Update Rate"};
static const char* agnss_options[] = {"Disable", "Enable"};
static const char* power_options[] = {"Normal", "LOW_POWER", "STANDBY"};
static const char* advanced_options[] = {"Disable", "Enable"};
static const char* update_rate_options[] = {"1 Hz", "5 Hz", "10 Hz"};
static uint16_t last_config_selection = 0;

#define GPS_ROUTE_DIR_NAME          "/GPS_Route"
#define GPS_ROUTE_FILE_SIZE         8192
#define GPS_ROUTE_CSV_HEADER_LINES  1
#define GPS_ROUTE_MAX_CSV_LINES     20  // Reducir para escribir más frecuentemente
#define GPS_ROUTE_SAVE_INTERVAL_SEC 5
const char* gps_route_gpx_header =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<gpx version=\"1.1\" creator=\"MININO-GPS\">\n"
    "  <metadata>\n"
    "    <name>Minino Trail</name>\n"
    "    <time>%s</time>\n"
    "  </metadata>\n"
    "  <trk>\n"
    "    <name>Minino Track</name>\n"
    "    <trkseg>\n";
const char* gps_route_gpx_footer =
    "    </trkseg>\n"
    "  </trk>\n"
    "</gpx>\n";

char* gps_help[] = {
    "Verify your",    "time zone if", "the time is not",
    "correct, go to", "`Settings/",   "System/Time",
    "zone` and",      "select the",   "correct one.",
};
const general_menu_t gps_help_menu = {.menu_count = 9,
                                      .menu_items = gps_help,
                                      .menu_level = GENERAL_TREE_APP_MENU};

typedef enum {
  CONFIG_AGNSS,
  CONFIG_POWWER,
  CONFIG_ADVANCED,
  CONFIG_URATE
} config_menu_options_t;

void gps_screens_show_help() {
  general_register_scrolling_menu(&gps_help_menu);
  general_screen_display_scrolling_text_handler(menus_module_exit_app);
}

void gps_screen_running_test(void) {
  oled_screen_clear_buffer();
  oled_screen_display_text_center("Getting", 0, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("Info", 1, OLED_DISPLAY_NORMAL);
  oled_screen_display_show();
}

static void gps_screens_test(gps_t* gps) {
  char* str = (char*) malloc(20);
  char* str2 = (char*) malloc(20);
  oled_screen_clear_buffer();
  // uint8_t sats = gps_module_get_signal_strength(gps);  // Unused variable
  if (gps->sats_in_use == 0) {
    sprintf(str, "GPS OK");
    sprintf(str2, "Sats: %d", 0);
  } else {
    sprintf(str, "Signal: %s   ", gps_module_get_signal_strength(gps));
    sprintf(str2, "Sats: %d", gps->sats_in_use);
  }

  oled_screen_display_text(str, 0, 0, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("UTC Date-Time", 1, OLED_DISPLAY_NORMAL);
  oled_screen_display_text(str2, 0, 2, OLED_DISPLAY_NORMAL);

  sprintf(str, "Date: %d/%02d/%02d", gps->date.year, gps->date.month,
          gps->date.day);
  oled_screen_display_text(str, 0, 3, OLED_DISPLAY_NORMAL);
  sprintf(str, "Time: %02d:%02d:%02d", gps->tim.hour, gps->tim.minute,
          gps->tim.second);
  oled_screen_display_text(str, 0, 4, OLED_DISPLAY_NORMAL);
  oled_screen_display_show();

  free(str);
  free(str2);
}

static void gps_screens_update_date_and_time(gps_t* gps) {
  char* str = (char*) malloc(20);
  oled_screen_clear_buffer();
  sprintf(str, "Signal: %s   ", gps_module_get_signal_strength(gps));
  oled_screen_display_text_center("UTC Date-Time", 0, OLED_DISPLAY_NORMAL);
  oled_screen_display_text(str, 0, 0, OLED_DISPLAY_NORMAL);
  sprintf(str, "Date: %d/%02d/%02d", gps->date.year, gps->date.month,
          gps->date.day);
  oled_screen_display_text(str, 0, 2, OLED_DISPLAY_NORMAL);
  sprintf(str, "Time: %02d:%02d:%02d", gps->tim.hour, gps->tim.minute,
          gps->tim.second);
  oled_screen_display_text(str, 0, 3, OLED_DISPLAY_NORMAL);
  oled_screen_display_show();
  free(str);
}

static void gps_screens_update_location(gps_t* gps) {
  char* str = (char*) malloc(20);
  oled_screen_clear_buffer();
  sprintf(str, "Signal: %s   ", gps_module_get_signal_strength(gps));
  oled_screen_display_text(str, 0, 0, OLED_DISPLAY_NORMAL);
  oled_screen_display_text("Latitude:", 0, 2, OLED_DISPLAY_NORMAL);
  sprintf(str, "  %.05f N", gps->latitude);
  oled_screen_display_text(str, 0, 3, OLED_DISPLAY_NORMAL);
  oled_screen_display_text("Longitude:", 0, 4, OLED_DISPLAY_NORMAL);
  sprintf(str, "  %.05f E", gps->longitude);
  oled_screen_display_text(str, 0, 5, OLED_DISPLAY_NORMAL);
  oled_screen_display_text("Altitude:", 0, 6, OLED_DISPLAY_NORMAL);
  sprintf(str, "  %.04fm", gps->altitude);
  oled_screen_display_text(str, 0, 7, OLED_DISPLAY_NORMAL);
  oled_screen_display_show();
  free(str);
}

static void gps_screens_save_location(gps_t* gps) {
  static uint32_t counter = 0;
  counter++;

  if (gps == NULL || gps->sats_in_use == 0) {
    oled_screen_clear();
    oled_screen_display_text_center("No GPS Signal", 3, OLED_DISPLAY_NORMAL);
    oled_screen_display_show();
    return;
  }

  if (!gps_route_recording) {
    gps_route_recording = true;
    gps_route_lines = GPS_ROUTE_CSV_HEADER_LINES;
    gps_route_points_saved = 0;

    esp_err_t err = sd_card_mount();
    if (err != ESP_OK) {
      oled_screen_clear();
      oled_screen_display_text_center("No SD Card", 2, OLED_DISPLAY_NORMAL);
      oled_screen_display_text_center("detected!", 3, OLED_DISPLAY_NORMAL);
      oled_screen_display_show();
      gps_route_recording = false;
      return;
    }

    err = sd_card_create_dir(GPS_ROUTE_DIR_NAME);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to create %s directory: %s", GPS_ROUTE_DIR_NAME,
               esp_err_to_name(err));
      sd_card_unmount();
      gps_route_recording = false;
      return;
    }

    gps_route_file_name = malloc(strlen(GPS_ROUTE_DIR_NAME) + 30);
    if (gps_route_file_name == NULL) {
      ESP_LOGE(TAG, "Failed to allocate memory for gps_route_file_name");
      sd_card_unmount();
      gps_route_recording = false;
      return;
    }

    gps_route_file_buffer = malloc(GPS_ROUTE_FILE_SIZE);
    if (gps_route_file_buffer == NULL) {
      ESP_LOGE(TAG, "Failed to allocate memory for gps_route_file_buffer");
      free(gps_route_file_name);
      sd_card_unmount();
      gps_route_recording = false;
      return;
    }

    char* full_date_time = get_full_date_time(gps);
    if (full_date_time == NULL) {
      ESP_LOGE(TAG, "Failed to get full date time");
      free(gps_route_file_name);
      free(gps_route_file_buffer);
      sd_card_unmount();
      gps_route_recording = false;
      return;
    }

    snprintf(gps_route_file_name, strlen(GPS_ROUTE_DIR_NAME) + 30,
             "%s/Route_%s.gpx", GPS_ROUTE_DIR_NAME, full_date_time);
    for (int i = 0; i < strlen(gps_route_file_name); i++) {
      if (gps_route_file_name[i] == ' ')
        gps_route_file_name[i] = '_';
      if (gps_route_file_name[i] == ':')
        gps_route_file_name[i] = '-';
    }

    char iso_time[32];
    snprintf(iso_time, sizeof(iso_time), "%04d-%02d-%02dT%02d:%02d:%02dZ",
             gps->date.year, gps->date.month, gps->date.day, gps->tim.hour,
             gps->tim.minute, gps->tim.second);

    snprintf(gps_route_file_buffer, GPS_ROUTE_FILE_SIZE, gps_route_gpx_header,
             iso_time);
    esp_err_t err_header =
        sd_card_append_to_file(gps_route_file_name, gps_route_file_buffer);
    if (err_header != ESP_OK) {
      ESP_LOGE(TAG, "Failed to write GPX header: %s",
               esp_err_to_name(err_header));
      free(gps_route_file_name);
      free(gps_route_file_buffer);
      sd_card_unmount();
      gps_route_recording = false;
      return;
    }
    gps_route_file_buffer[0] =
        '\0';  // Limpiar buffer después de escribir el encabezado
    free(full_date_time);
  }

  char* str = (char*) malloc(20);
  if (str == NULL) {
    ESP_LOGE(TAG, "Failed to allocate memory for display string");
    return;
  }
  oled_screen_clear_buffer();
  snprintf(str, 20, "Points: %u", gps_route_points_saved);
  oled_screen_display_text(str, 0, 0, OLED_DISPLAY_NORMAL);
  snprintf(str, 20, "Lat: %.05f", gps->latitude);
  oled_screen_display_text(str, 0, 2, OLED_DISPLAY_NORMAL);
  snprintf(str, 20, "Lon: %.05f", gps->longitude);
  oled_screen_display_text(str, 0, 3, OLED_DISPLAY_NORMAL);
  oled_screen_display_show();
  free(str);

  if (counter % GPS_ROUTE_SAVE_INTERVAL_SEC == 0) {
    if (gps->sats_in_use == 0) {
      oled_screen_clear();
      oled_screen_display_text_center("No GPS Signal", 3, OLED_DISPLAY_NORMAL);
      oled_screen_display_show();
      return;
    }

    char* gpx_line_buffer = malloc(256);
    if (gpx_line_buffer == NULL) {
      ESP_LOGE(TAG, "Failed to allocate memory for gpx_line_buffer");
      return;
    }

    char iso_time[32];
    snprintf(iso_time, sizeof(iso_time), "%04d-%02d-%02dT%02d:%02d:%02dZ",
             gps->date.year, gps->date.month, gps->date.day, gps->tim.hour,
             gps->tim.minute, gps->tim.second);

    snprintf(gpx_line_buffer, 256,
             "      <trkpt lat=\"%.05f\" lon=\"%.05f\">\n"
             "        <ele>%.02f</ele>\n"
             "        <time>%s</time>\n"
             "      </trkpt>\n",
             gps->latitude, gps->longitude, gps->altitude, iso_time);

    if (strlen(gps_route_file_buffer) + strlen(gpx_line_buffer) <
        GPS_ROUTE_FILE_SIZE) {
      strcat(gps_route_file_buffer, gpx_line_buffer);
      gps_route_lines++;
      gps_route_points_saved++;
    } else {
      ESP_LOGW(TAG, "Buffer full, appending to SD");
      esp_err_t err =
          sd_card_append_to_file(gps_route_file_name, gps_route_file_buffer);
      if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to append to SD: %s", esp_err_to_name(err));
      }
      gps_route_file_buffer[0] = '\0';  // Limpiar buffer
      gps_route_lines = GPS_ROUTE_CSV_HEADER_LINES;
    }
    free(gpx_line_buffer);
  }
}

void gps_screens_stop_route_recording() {
  if (gps_route_recording) {
    // Escribir cualquier punto pendiente en el buffer
    if (gps_route_file_buffer[0] != '\0') {
      esp_err_t err =
          sd_card_append_to_file(gps_route_file_name, gps_route_file_buffer);
      if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to append final points: %s",
                 esp_err_to_name(err));
      }
    }
    // Escribir el pie de página GPX
    esp_err_t err =
        sd_card_append_to_file(gps_route_file_name, gps_route_gpx_footer);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to append GPX footer: %s", esp_err_to_name(err));
    }
    free(gps_route_file_buffer);
    free(gps_route_file_name);
    sd_card_unmount();
    gps_route_recording = false;
    gps_route_points_saved = 0;
    gps_route_lines = 0;
    ESP_LOGI(TAG, "Route recording stopped, SD card unmounted");
  }
}

static void gps_screens_update_speed(gps_t* gps) {
  char* str = (char*) malloc(20);
  oled_screen_clear_buffer();
  sprintf(str, "Signal: %s   ", gps_module_get_signal_strength(gps));
  oled_screen_display_text(str, 0, 0, OLED_DISPLAY_NORMAL);
  sprintf(str, "Speed: %.02fm/s", gps->speed);
  oled_screen_display_text(str, 0, 2, OLED_DISPLAY_NORMAL);
  oled_screen_display_show();
  free(str);
}

void gps_screens_show_waiting_signal() {
  oled_screen_clear_buffer();
  oled_screen_display_text_center("Waiting Signal", 0, OLED_DISPLAY_NORMAL);
  oled_screen_display_show();
}

static void agnss_radio_handler(uint8_t option) {
  preferences_put_int(AGNSS_OPTIONS_PREF_KEY, option);
}

static void gps_screens_show_agnss(void) {
  general_radio_selection_menu_t settings = {0};
  settings.banner = "En/Disable AGNSS";
  settings.options = agnss_options;
  settings.options_count = sizeof(agnss_options) / sizeof(char*);
  settings.select_cb = agnss_radio_handler;
  settings.style = RADIO_SELECTION_OLD_STYLE;
  settings.exit_cb = gps_screens_show_config;
  settings.current_option = preferences_get_int(AGNSS_OPTIONS_PREF_KEY, 1);
  general_radio_selection(settings);
}

static void power_radio_handler(uint8_t option) {
  preferences_put_int(POWER_OPTIONS_PREF_KEY, option);
}

static void gps_screens_show_power(void) {
  general_radio_selection_menu_t settings = {0};
  settings.banner = "Power Mode";
  settings.options = power_options;
  settings.options_count = sizeof(power_options) / sizeof(char*);
  settings.select_cb = power_radio_handler;
  settings.style = RADIO_SELECTION_OLD_STYLE;
  settings.exit_cb = gps_screens_show_config;
  settings.current_option = preferences_get_int(POWER_OPTIONS_PREF_KEY, 0);
  general_radio_selection(settings);
}

static void advanced_radio_handler(uint8_t option) {
  preferences_put_int(ADVANCED_OPTIONS_PREF_KEY, option);
}

static void gps_screens_show_advanced(void) {
  general_radio_selection_menu_t settings = {0};
  settings.banner = "Advanced Mode";
  settings.options = advanced_options;
  settings.options_count = sizeof(advanced_options) / sizeof(char*);
  settings.select_cb = advanced_radio_handler;
  settings.style = RADIO_SELECTION_OLD_STYLE;
  settings.exit_cb = gps_screens_show_config;
  settings.current_option = preferences_get_int(ADVANCED_OPTIONS_PREF_KEY, 1);
  general_radio_selection(settings);
}

static void update_rate_radio_handler(uint8_t option) {
  preferences_put_int(URATE_OPTIONS_PREF_KEY, option);
}

static void gps_screens_show_urate(void) {
  general_radio_selection_menu_t settings = {0};
  settings.banner = "Update Rate";
  settings.options = update_rate_options;
  settings.options_count = sizeof(update_rate_options) / sizeof(char*);
  settings.select_cb = update_rate_radio_handler;
  settings.style = RADIO_SELECTION_OLD_STYLE;
  settings.exit_cb = gps_screens_show_config;
  settings.current_option = preferences_get_int(URATE_OPTIONS_PREF_KEY, 1);
  general_radio_selection(settings);
}

static void config_main_handler(uint8_t option) {
  last_config_selection = option;
  switch (option) {
    case CONFIG_AGNSS:
      gps_screens_show_agnss();
      break;
    case CONFIG_POWWER:
      gps_screens_show_power();
      break;
    case CONFIG_ADVANCED:
      gps_screens_show_advanced();
      break;
    case CONFIG_URATE:
      gps_screens_show_urate();
      break;
    default:
      break;
  }
}

void gps_screens_show_config(void) {
  general_submenu_menu_t main = {0};
  main.options = config_menu_options;
  main.options_count = sizeof(config_menu_options) / sizeof(char*);
  main.select_cb = config_main_handler;
  main.selected_option = last_config_selection;
  main.exit_cb = menus_module_restart;
  general_submenu(main);
}

void gps_screens_update_handler(gps_t* gps) {
  menu_idx_t current = menus_module_get_current_menu();
  switch (current) {
    case MENU_GPS_DATE_TIME:
      gps_screens_update_date_and_time(gps);
      break;
    case MENU_GPS_LOCATION:
      gps_screens_update_location(gps);
      break;
    case MENU_GPS_SPEED:
      gps_screens_update_speed(gps);
      break;
    case MENU_GPS_ROUTE:
      gps_screens_save_location(gps);
      break;
    case MENU_GPS_TEST:
      gps_screens_test(gps);
      break;
    default:
      return;
      break;
  }
}
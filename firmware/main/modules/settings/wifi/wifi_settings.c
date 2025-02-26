#include <string.h>
#include "bitmaps_general.h"
#include "cmd_wifi.h"
#include "esp_log.h"
#include "general/general_screens.h"
#include "led_events.h"
#include "menus_module.h"
#include "modals_module.h"
#include "oled_screen.h"
#include "preferences.h"
#include "wifi_settings_scenes.h"

#define TAG_CONFIG_MODULE "CONFIG_MODULE"

#ifdef CONFIG_RESOLUTION_128X64
  #define START_PAGE     2
  #define Y_N_OFFSET     4
  #define ITEMOFFSET     2
  #define ITEMSPERSCREEN 4
#else  // CONFIG_RESOLUTION_128X32
  #define START_PAGE     0
  #define Y_N_OFFSET     2
  #define ITEMOFFSET     1
  #define ITEMSPERSCREEN 3
#endif

static int selected_item = 0;
static int total_items = 0;
static int max_items = 6;

char* options_wifi_menu[] = {"Connect", "Forget", NULL};

typedef enum {
  WIFI_SETTING_IDLE = 0,
  WIFI_SETTING_CONFIG,
  WIFI_SETTING_COUNT
} wifi_setting_state_t;

typedef struct {
  wifi_setting_state_t state;
  int selected_item;
  int total_items;
  int max_items;
} wifi_setting_t;

static wifi_setting_state_t wifi_setting_state = WIFI_SETTING_IDLE;
static wifi_setting_t wifi_config_state;

static void only_exit_input_cb(uint8_t button_name, uint8_t button_event);
static void config_module_state_machine(uint8_t button_name,
                                        uint8_t button_event);
static void config_module_state_machine_config(uint8_t button_name,
                                               uint8_t button_event);
static void config_module_state_machine_config_modal_connect(
    uint8_t button_name,
    uint8_t button_event);
static void config_module_state_machine_config_modal_forget(
    uint8_t button_name,
    uint8_t button_event);
static void config_module_wifi_display_list();
static void config_module_wifi_display_connected();

static void config_module_wifi_display_selected_item(char* item_text,
                                                     uint8_t item_number) {
  oled_screen_display_bitmap(minino_face, 0, (item_number * 8), 8, 8,
                             OLED_DISPLAY_NORMAL);
  oled_screen_display_text(item_text, 16, item_number, OLED_DISPLAY_INVERT);
}

static void config_module_wifi_display_selected_item_center(
    char* item_text,
    uint8_t item_number) {
  uint8_t x = (128 - (strlen(item_text) * 8)) / 2;

  oled_screen_display_bitmap(minino_face, x - 12, (item_number * 8), 8, 8,
                             OLED_DISPLAY_NORMAL);
  oled_screen_display_text(item_text, x, item_number, OLED_DISPLAY_INVERT);
}

static void config_module_wifi_display_not_wifi() {
  oled_screen_clear();
  oled_screen_display_text_center("No saved APs", START_PAGE,
                                  OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("Add new AP", START_PAGE + 1,
                                  OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("From our serial", START_PAGE + 2,
                                  OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("Console", START_PAGE + 3,
                                  OLED_DISPLAY_NORMAL);
}

static int validate_wifi_count() {
  int count = preferences_get_int("count_ap", 0);
  if (count == 0) {
    config_module_wifi_display_not_wifi();
    return 0;
  }
  return count;
}

static void config_module_wifi_display_connecting() {
  oled_screen_clear();
  modals_module_show_banner("Connecting");
}

static void config_module_wifi_display_disconnected() {
  oled_screen_clear();
  modals_module_show_banner("Disconnected");
  wifi_config_state.state = WIFI_SETTING_IDLE;
  selected_item = 0;
  vTaskDelay(2000 / portTICK_PERIOD_MS);
  menus_module_set_app_state(true, config_module_state_machine);
  config_module_wifi_display_list();
}

static void config_module_wifi_display_connected() {
  oled_screen_clear();
  modals_module_show_banner("Connected");
  vTaskDelay(2000 / portTICK_PERIOD_MS);
  cmd_wifi_unregister_callback();
  // menus_module_exit_app();
  wifi_settings_scenes_main();
}

static void config_module_wifi_handle_connection(bool state) {
  if (state) {
    config_module_wifi_display_connected();
  } else {
    config_module_wifi_display_disconnected();
  }
}

static void config_module_wifi_display_list() {
  oled_screen_clear();
  oled_screen_display_text("< Exit", 0, 0, OLED_DISPLAY_NORMAL);

  if (validate_wifi_count() == 0) {
    return;
  }

  int position = 1;
  uint16_t start_item = (selected_item / ITEMSPERSCREEN) * ITEMSPERSCREEN;

  ESP_LOGI(__func__, "Selected item: %d", validate_wifi_count());
  // int index = (wifi_config_state.total_items > max_items) ? selected_item :
  // 0; int limit = (wifi_config_state.total_items > max_items)
  //                 ? (max_items + selected_item)
  //                 : max_items;
  for (int i = start_item;
       i < start_item + ITEMSPERSCREEN && i < wifi_config_state.total_items;
       i++) {
    char wifi_ap[100];
    char wifi_ssid[100];
    sprintf(wifi_ap, "wifi%d", i);
    esp_err_t err = preferences_get_string(wifi_ap, wifi_ssid, 100);
    if (err != ESP_OK) {
      ESP_LOGW(__func__, "Error getting AP");
      return;
    }
    if (strlen(wifi_ssid) > 16) {
      wifi_ssid[16] = '\0';
    }
    // int page = (wifi_config_state.total_items > max_items)
    //                ? (i + 1) - selected_item
    //                : (i + 1);
    if (i == selected_item) {
      config_module_wifi_display_selected_item(wifi_ssid, position);
    } else {
      oled_screen_display_text(wifi_ssid, 0, position, OLED_DISPLAY_NORMAL);
    }
    position = position + ITEMOFFSET;
  }
}

static void config_module_wifi_display_sel_options() {
  char wifi_ap[100];
  sprintf(wifi_ap, "wifi%d", wifi_config_state.selected_item);
  char wifi_ssid[100];
  esp_err_t err = preferences_get_string(wifi_ap, wifi_ssid, 100);
  if (err != ESP_OK) {
    ESP_LOGW(__func__, "Error getting AP");
    return;
  }
  if (strlen(wifi_ssid) > 16) {
    wifi_ssid[16] = '\0';
  }
  genera_screen_display_card_information(wifi_ssid, "");
  // oled_screen_clear();
  // oled_screen_display_text("< Back", 0, 0, OLED_DISPLAY_NORMAL);
  // oled_screen_display_text_center(wifi_ssid, 1, OLED_DISPLAY_NORMAL);
  int page = 2;
  for (int i = 0; options_wifi_menu[i] != NULL; i++) {
    if (i == selected_item) {
      config_module_wifi_display_selected_item_center(options_wifi_menu[i],
                                                      page);
    } else {
      oled_screen_display_text_center(options_wifi_menu[i], page,
                                      OLED_DISPLAY_NORMAL);
    }
    page++;
  }
}

static void config_module_wifi_display_forget_modal() {
  oled_screen_clear();
  oled_screen_display_text_center("Forget this AP?", 1, OLED_DISPLAY_NORMAL);
  if (selected_item == 0) {
    config_module_wifi_display_selected_item_center("YES", Y_N_OFFSET);
    oled_screen_display_text_center("NO", Y_N_OFFSET + 1, OLED_DISPLAY_NORMAL);
  } else {
    oled_screen_display_text_center("YES", Y_N_OFFSET, OLED_DISPLAY_NORMAL);
    config_module_wifi_display_selected_item_center("NO", Y_N_OFFSET + 1);
  }
}

static void config_module_wifi_display_connect_modal() {
  genera_screen_display_card_information("Connect this AP?", "");
  if (selected_item == 0) {
    config_module_wifi_display_selected_item_center("YES", Y_N_OFFSET);
    oled_screen_display_text_center("NO", Y_N_OFFSET + 1, OLED_DISPLAY_NORMAL);
  } else {
    oled_screen_display_text_center("YES", Y_N_OFFSET, OLED_DISPLAY_NORMAL);
    config_module_wifi_display_selected_item_center("NO", Y_N_OFFSET + 1);
  }
}

void wifi_settings_begin() {
  int count = validate_wifi_count();
  if (count == 0) {
    menus_module_set_app_state(true, only_exit_input_cb);
    return;
  }
  menus_module_set_app_state(true, config_module_state_machine);
  wifi_config_state.state = WIFI_SETTING_IDLE;
  wifi_config_state.total_items = count;
  total_items = count;
  ESP_LOGI(__func__, "Saved APs: %d", count);
  config_module_wifi_display_list();
}

static void only_exit_input_cb(uint8_t button_name, uint8_t button_event) {
  if (button_event != BUTTON_PRESS_DOWN) {
    return;
  }
  if (button_name == BUTTON_LEFT) {
    cmd_wifi_unregister_callback();
    // menus_module_exit_app();
    wifi_settings_scenes_main();
  }
}
static void config_module_state_machine(uint8_t button_name,
                                        uint8_t button_event) {
  if (button_event != BUTTON_PRESS_DOWN) {
    return;
  }

  ESP_LOGI(TAG_CONFIG_MODULE, "BLE engine state machine from team: %d %d",
           button_name, button_event);

  switch (button_name) {
    case BUTTON_LEFT:
      cmd_wifi_unregister_callback();
      // menus_module_exit_app();
      wifi_settings_scenes_main();
      break;
    case BUTTON_RIGHT:
      ESP_LOGI(TAG_CONFIG_MODULE, "Selected item: %d", selected_item);
      wifi_config_state.selected_item = selected_item;
      wifi_config_state.state = WIFI_SETTING_CONFIG;
      selected_item = 0;

      config_module_wifi_display_sel_options();
      menus_module_set_app_state(true, config_module_state_machine_config);
      break;
    case BUTTON_UP:
      selected_item =
          (selected_item == 0) ? total_items - 1 : selected_item - 1;
      config_module_wifi_display_list();
      break;
    case BUTTON_DOWN:
      selected_item =
          (selected_item == total_items - 1) ? 0 : selected_item + 1;
      config_module_wifi_display_list();
      break;
    case BUTTON_BOOT:
    default:
      break;
  }
}

static void config_module_state_machine_config(uint8_t button_name,
                                               uint8_t button_event) {
  if (button_event != BUTTON_PRESS_DOWN) {
    return;
  }
  switch (button_name) {
    case BUTTON_LEFT:
      wifi_config_state.state = WIFI_SETTING_IDLE;
      selected_item = 0;
      menus_module_set_app_state(true, config_module_state_machine);
      config_module_wifi_display_list();
      break;
    case BUTTON_RIGHT:
      ESP_LOGI(TAG_CONFIG_MODULE, "Selected item: %d", selected_item);
      if (selected_item == 0) {
        selected_item = 0;
        config_module_wifi_display_connect_modal();
        menus_module_set_app_state(
            true, config_module_state_machine_config_modal_connect);
      } else {
        selected_item = 0;
        config_module_wifi_display_forget_modal();
        menus_module_set_app_state(
            true, config_module_state_machine_config_modal_forget);
      }
      break;
    case BUTTON_UP:
      selected_item = (selected_item == 0) ? 3 - 1 : selected_item - 1;

      config_module_wifi_display_sel_options();
      break;
    case BUTTON_DOWN:
      selected_item = (selected_item == 3 - 1) ? 0 : selected_item + 1;
      config_module_wifi_display_sel_options();
      break;
    case BUTTON_BOOT:
    default:
      break;
  }
}

static void config_module_state_machine_config_modal_connect(
    uint8_t button_name,
    uint8_t button_event) {
  if (button_event != BUTTON_PRESS_DOWN) {
    return;
  }
  switch (button_name) {
    case BUTTON_LEFT:
      break;
    case BUTTON_RIGHT:
      ESP_LOGI(TAG_CONFIG_MODULE, "Selected item: %d", selected_item);
      if (selected_item == 0) {
        if (preferences_get_bool("wifi_connected", false)) {
          config_module_wifi_display_connected();
          break;
        }
        config_module_wifi_display_connecting();
        wifi_config_state.state = WIFI_SETTING_IDLE;
        char wifi_ap[100];
        sprintf(wifi_ap, "wifi%d", wifi_config_state.selected_item);
        char wifi_ssid[100];
        preferences_get_string(wifi_ap, wifi_ssid, 100);
        char wifi_pass[100];
        preferences_get_string(wifi_ssid, wifi_pass, 100);
        connect_wifi(wifi_ssid, wifi_pass,
                     config_module_wifi_handle_connection);
        menus_module_set_app_state(true, config_module_state_machine_config);
        wifi_config_state.selected_item = 0;
      } else {
        selected_item = 0;
        wifi_config_state.state = WIFI_SETTING_IDLE;
        menus_module_set_app_state(true, config_module_state_machine_config);
        config_module_wifi_display_sel_options();
      }
      break;
    case BUTTON_UP:
      selected_item = (selected_item == 0) ? 2 - 1 : selected_item - 1;
      config_module_wifi_display_connect_modal();
      break;
    case BUTTON_DOWN:
      selected_item = (selected_item == 2 - 1) ? 0 : selected_item + 1;
      config_module_wifi_display_connect_modal();
      break;
    case BUTTON_BOOT:
    default:
      break;
  }
}

static void config_module_state_machine_config_modal_forget(
    uint8_t button_name,
    uint8_t button_event) {
  if (button_event != BUTTON_PRESS_DOWN) {
    return;
  }
  switch (button_name) {
    case BUTTON_LEFT:
      break;
    case BUTTON_RIGHT:
      ESP_LOGI(TAG_CONFIG_MODULE, "Selected item: %d", selected_item);
      if (selected_item == 0) {
        char wifi_ap[100];
        sprintf(wifi_ap, "wifi%d", wifi_config_state.selected_item);
        char wifi_ssid[100];
        preferences_get_string(wifi_ap, wifi_ssid, 100);
        esp_err_t err = preferences_remove(wifi_ssid);
        if (err != ESP_OK) {
          ESP_LOGW(__func__, "Error removing AP");
          return;
        }
        err = preferences_remove(wifi_ap);
        if (err != ESP_OK) {
          ESP_LOGW(__func__, "Error removing AP");
          return;
        }
        int count = preferences_get_int("count_ap", 0);
        int counter = 0;
        for (int i = 0; i < count - 1; i++) {
          char wifi_ap[100];
          char wifi_ssid[100];
          sprintf(wifi_ap, "wifi%d", i);
          esp_err_t err = preferences_get_string(wifi_ap, wifi_ssid, 100);
          if (err != ESP_OK) {
            continue;
          }
          char wifi_ap_new[100];
          sprintf(wifi_ap_new, "wifi%d", counter);
          preferences_put_string(wifi_ap_new, wifi_ssid);
          counter++;
        }
        err = preferences_put_int("count_ap", count - 1);
        if (err != ESP_OK) {
          ESP_LOGW(__func__, "Error removing AP");
          return;
        }
      }
      int count = validate_wifi_count();
      if (count == 0) {
        menus_module_set_app_state(true, only_exit_input_cb);
        return;
      }
      wifi_config_state.total_items = count;
      total_items = count;
      selected_item = 0;
      wifi_config_state.state = WIFI_SETTING_IDLE;
      menus_module_set_app_state(true, config_module_state_machine_config);
      config_module_wifi_display_sel_options();
      break;
    case BUTTON_UP:
      selected_item = (selected_item == 0) ? 2 - 1 : selected_item - 1;
      config_module_wifi_display_forget_modal();
      break;
    case BUTTON_DOWN:
      selected_item = (selected_item == 2 - 1) ? 0 : selected_item + 1;
      config_module_wifi_display_forget_modal();
      break;
    case BUTTON_BOOT:
    default:
      break;
  }
}
#include "menu_screens_modules.h"
#include "OTA.h"
#include "bitmaps.h"
#include "ble_module.h"
#include "configuration.h"
#include "esp_log.h"
#include "gps_module.h"
#include "leds.h"
#include "oled_screen.h"
#include "open_thread.h"
#include "open_thread_module.h"
#include "ota_module.h"
#include "preferences.h"
#include "radio_selector.h"
#include "settings_module.h"
#include "string.h"
#include "web_file_browser_module.h"
#include "wifi_module.h"
#include "wifi_sniffer.h"
#include "zigbee_module.h"
#include "zigbee_screens_module.h"
#include "zigbee_switch.h"

#define MAX_MENU_ITEMS_PER_SCREEN 3

static const char* TAG = "menu_screens_modules";
uint8_t selected_item;
uint32_t num_items;
screen_module_menu_t previous_menu;
screen_module_menu_t current_menu;
uint8_t bluetooth_devices_count;

static app_state_t app_state = {
    .in_app = false,
    .app_handler = NULL,
};

static enter_submenu_cb_t enter_submenu_cb = NULL;
static exit_submenu_cb_t exit_submenu_cb = NULL;

void handle_user_selection(screen_module_menu_t user_selection);

esp_err_t test_menu_list() {
  ESP_LOGI(TAG, "Testing menus list size");
  size_t menu_list_size = sizeof(menu_list) / sizeof(menu_list[0]);
  if (menu_list_size != MENU_COUNT) {
    ESP_LOGE(TAG, "menu_list size is not as screen_module_menu_t enum");
    return ESP_FAIL;
  }
  ESP_LOGI(TAG, "Test passed");
  return ESP_OK;
}

esp_err_t test_menu_next_menu_table() {
  ESP_LOGI(TAG, "Testing next menu table size");
  size_t next_menu_table_size =
      sizeof(next_menu_table) / sizeof(next_menu_table[0]);
  if (next_menu_table_size != MENU_COUNT) {
    ESP_LOGE(TAG, "next_menu_table size is not as screen_module_menu_t enum");
    return ESP_FAIL;
  }
  ESP_LOGI(TAG, "Test passed");
  return ESP_OK;
}

esp_err_t test_prev_menu_table() {
  ESP_LOGI(TAG, "Testing previous menu table size");
  size_t prev_menu_table_size =
      sizeof(prev_menu_table) / sizeof(prev_menu_table[0]);
  if (prev_menu_table_size != MENU_COUNT) {
    ESP_LOGE(TAG, "prev_menu_table size is not as screen_module_menu_t enum");
    return ESP_FAIL;
  }
  ESP_LOGI(TAG, "Test passed");
  return ESP_OK;
}

esp_err_t test_menu_items() {
  ESP_LOGI(TAG, "Testing menu items size");
  size_t menu_items_size = sizeof(menu_items) / sizeof(menu_items[0]);
  if (menu_items_size != MENU_COUNT) {
    ESP_LOGE(TAG, "menu_items size is not as screen_module_menu_t enum");
    return ESP_FAIL;
  }
  ESP_LOGI(TAG, "Test passed");
  return ESP_OK;
}

void run_tests() {
  ESP_ERROR_CHECK(test_menu_list());
  ESP_ERROR_CHECK(test_menu_next_menu_table());
  ESP_ERROR_CHECK(test_prev_menu_table());
  ESP_ERROR_CHECK(test_menu_items());
}

void show_logo() {
  // buzzer_set_freq(50);
  oled_screen_clear();
  leds_on();
  buzzer_play();
  vTaskDelay(500 / portTICK_PERIOD_MS);
  buzzer_stop();
  oled_screen_display_text_center("Still under", 2, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("DEVELOPMENT", 3, OLED_DISPLAY_NORMAL);
  vTaskDelay(500 / portTICK_PERIOD_MS);
  buzzer_play();
  vTaskDelay(500 / portTICK_PERIOD_MS);
  buzzer_stop();
  vTaskDelay(500 / portTICK_PERIOD_MS);
  buzzer_play();
  vTaskDelay(500 / portTICK_PERIOD_MS);
  buzzer_stop();
  vTaskDelay(2000 / portTICK_PERIOD_MS);
  buzzer_play();
  oled_screen_display_bitmap(epd_bitmap_face_logo, 46, 16, 32, 32,
                             OLED_DISPLAY_NORMAL);
  char* version = malloc(20);
  sprintf(version, "v%s BETA", CONFIG_PROJECT_VERSION);
  oled_screen_display_text_center(version, 6, OLED_DISPLAY_INVERT);
  free(version);
  vTaskDelay(500 / portTICK_PERIOD_MS);
  buzzer_stop();
}

void screen_module_set_screen(int current_menu) {
  preferences_put_int("MENUNUMBER", prev_menu_table[current_menu]);
  oled_screen_clear();
  menu_screens_display_text_banner("Exiting...");
}

void screen_module_get_screen() {
  current_menu = preferences_get_int("MENUNUMBER", MENU_MAIN);
  handle_user_selection(current_menu);

  // Update number of items
  if (current_menu == MENU_MAIN) {
    char** submenu = menu_items[current_menu];
    if (submenu != NULL) {
      while (submenu[num_items] != NULL) {
        num_items++;
      }
    }
    show_logo();
  } else {
    preferences_put_int("MENUNUMBER", MENU_MAIN);
    menu_screens_display_menu();
  }
}

void menu_screens_begin() {
  selected_item = 0;
  previous_menu = MENU_MAIN;
  current_menu = MENU_MAIN;
  num_items = 0;
  bluetooth_devices_count = 0;

  run_tests();
  oled_screen_begin();
  oled_screen_clear();
  screen_module_get_screen();
}

/**
 * @brief Add empty strings at the beginning and end of the array
 *
 * @param array
 * @param length
 *
 * @return char**
 */
char** add_empty_strings(char** array, int length) {
  char** newArray = malloc((length + 2) * sizeof(char*));

  // Add the empty string at the beginning
  newArray[0] = strdup("");

  // Copy the original array
  for (int i = 0; i < length; i++) {
    newArray[i + 1] = strdup(array[i]);
  }

  // Add the empty string at the end
  newArray[length + 1] = strdup("");

  num_items = length + 2;

  return newArray;
}

/**
 * @brief Remove the items flag
 *
 * @param items
 * @param length
 *
 * @return char**
 */
char** remove_items_flag(char** items, int length) {
  char** newArray = malloc((length - 1) * sizeof(char*));

  for (int i = 0; i < length - 1; i++) {
    newArray[i] = strdup(items[i + 1]);
    // ESP_LOGI(TAG, "Item: %s", newArray[i]);
  }
  // ESP_LOGI(TAG, "Number of items: %d", length - 1);

  num_items = length + 1;

  return newArray;
}

/**
 * @brief Check if the current menu is empty
 *
 * @return bool
 */
bool is_menu_empty(screen_module_menu_t menu) {
  return menu_items[menu][0] == NULL;
}

/**
 * @brief Check if the current menu is vertical scroll
 *
 * @return bool
 */
bool is_menu_vertical_scroll(screen_module_menu_t menu) {
  if (is_menu_empty(menu)) {
    return false;
  }
  return strcmp(menu_items[menu][0], VERTICAL_SCROLL_TEXT) == 0;
}

/**
 * @brief Check if the current menu is configuration
 *
 * @return bool
 */
bool is_menu_configuration(screen_module_menu_t menu) {
  if (is_menu_empty(menu)) {
    return false;
  }
  return strcmp(menu_items[menu][0], CONFIGURATION_MENU_ITEMS) == 0;
}

bool is_menu_question(screen_module_menu_t menu) {
  if (is_menu_empty(menu)) {
    return false;
  }
  return strcmp(menu_items[menu][0], QUESTION_MENU_ITEMS) == 0;
}

/**
 * @brief Get the menu items for the current menu
 *
 * @return char**
 */
char** get_menu_items() {
  num_items = 0;
  char** submenu = menu_items[current_menu];
  if (submenu != NULL) {
    while (submenu[num_items] != NULL) {
      // ESP_LOGI(TAG, "Item: %s", submenu[num_items]);
      num_items++;
    }
  }
  // ESP_LOGI(TAG, "Number of items: %d", num_items);

  if (num_items == 0) {
    return NULL;
  }

  if (is_menu_vertical_scroll(current_menu) ||
      is_menu_configuration(current_menu) || is_menu_question(current_menu)) {
    return submenu;
  }

  return add_empty_strings(menu_items[current_menu], num_items);
}

/**
 * @brief Display the menu items
 *
 * Show only 3 options at a time in the following order:
 * Page 1: Option 1
 * Page 3: Option 2 -> selected option
 * Page 5: Option 3
 *
 * @param items
 *
 * @return void
 */
void display_menu_items(char** items) {
#ifdef CONFIG_RESOLUTION_128X64
  char* prefix = "  ";
  uint8_t page = 1;
  uint8_t page_increment = 2;
#else  // CONFIG_RESOLUTION_128X32
  char* prefix = "> ";
  uint8_t page = 1;
  uint8_t page_increment = 1;
#endif

  oled_screen_clear();
  for (int i = 0; i < 3; i++) {
    char* text = (char*) malloc(strlen(items[i + selected_item]) + 2);
    if (i == 0) {
      sprintf(text, " %s", items[i + selected_item]);
    } else if (i == 1) {
      // sprintf(text, "  %s", items[i + selected_item]);
      sprintf(text, "%s%s", prefix, items[i + selected_item]);
    } else {
      sprintf(text, " %s", items[i + selected_item]);
    }

    oled_screen_display_text(text, 0, page, OLED_DISPLAY_NORMAL);
    page += page_increment;
  }

#ifdef CONFIG_RESOLUTION_128X64
  oled_screen_display_selected_item_box();
  oled_screen_display_show();
#endif
}

/**
 * @brief Display the scrolling text
 *
 * @param text
 *
 * @return void
 */
void display_scrolling_text(char** text) {
  uint8_t startIdx =
      (selected_item >= MAX_PAGE) ? selected_item - (MAX_PAGE - 1) : 0;
  selected_item = (num_items - 2 > MAX_PAGE && selected_item < (MAX_PAGE - 1))
                      ? (MAX_PAGE - 1)
                      : selected_item;
  oled_screen_clear();
  // ESP_LOGI(TAG, "num: %d", num_items - 2);

  for (uint8_t i = startIdx; i < num_items - 2; i++) {
    // ESP_LOGI(TAG, "Text[%d]: %s", i, text[i]);
    if (i == selected_item) {
      oled_screen_display_text(
          text[i], 0, i - startIdx,
          OLED_DISPLAY_NORMAL);  // Change it to INVERT to debug
    } else {
      oled_screen_display_text(text[i], 0, i - startIdx, OLED_DISPLAY_NORMAL);
    }
  }
}

void display_configuration_items(char** items) {
  uint8_t startIdx =
      (selected_item >= MAX_PAGE) ? selected_item - (MAX_PAGE - 1) : 0;
  oled_screen_clear();

  for (uint8_t i = startIdx; i < num_items - 2; i++) {
    if (i == selected_item) {
      oled_screen_display_text(items[i], 0, i - startIdx, OLED_DISPLAY_INVERT);
    } else {
      oled_screen_display_text(items[i], 0, i - startIdx, OLED_DISPLAY_NORMAL);
    }
  }
}

void display_question_items(char** items) {
#ifdef CONFIG_RESOLUTION_128X64
  uint8_t page_offset = 4;
  uint8_t question_page = 1;
#else  // CONFIG_RESOLUTION_128X32
  uint8_t page_offset = 2;
  uint8_t question_page = 0;
#endif

  oled_screen_clear();

  char* question = items[2];
  oled_screen_display_text_center(question, question_page, OLED_DISPLAY_NORMAL);

  // There are only two possible answers
  if (selected_item > 1) {
    selected_item = 1;
  }

  for (uint8_t i = 0; i <= 1; i++) {
    if (i == selected_item) {
      oled_screen_display_text_center(items[i], i + page_offset,
                                      OLED_DISPLAY_INVERT);
    } else {
      oled_screen_display_text_center(items[i], i + page_offset,
                                      OLED_DISPLAY_NORMAL);
    }
  }
}

/**
 * @brief Display the menu items
 *
 * @return void
 */
void menu_screens_display_menu() {
  char** items = get_menu_items();

  if (items == NULL) {
    ESP_LOGW(TAG, "Options is NULL");
    return;
  }

  if (is_menu_vertical_scroll(current_menu)) {
    char** text = remove_items_flag(items, num_items);
    display_scrolling_text(text);
  } else if (is_menu_configuration(current_menu)) {
    char** new_items = remove_items_flag(items, num_items);
    display_configuration_items(new_items);
  } else if (is_menu_question(current_menu)) {
    char** new_items = remove_items_flag(items, num_items);
    display_question_items(new_items);
  } else {
    display_menu_items(items);
  }
}

app_state_t menu_screens_get_app_state() {
  return app_state;
}

void menu_screens_set_app_state(bool in_app, app_handler_t app_handler) {
  app_state.in_app = in_app;
  app_state.app_handler = app_handler;
}

void menu_screens_register_enter_submenu_cb(enter_submenu_cb_t cb) {
  enter_submenu_cb = cb;
  ESP_LOGI(TAG, "Enter submenu callback registered");
}

void menu_screens_register_exit_submenu_cb(exit_submenu_cb_t cb) {
  exit_submenu_cb = cb;
  ESP_LOGI(TAG, "Exit submenu callback registered");
}

void menu_screens_unregister_submenu_cbs() {
  enter_submenu_cb = NULL;
  exit_submenu_cb = NULL;
  ESP_LOGI(TAG, "Submenu callbacks unregistered");
}

screen_module_menu_t menu_screens_get_current_menu() {
  return current_menu;
}

uint32_t menu_screens_get_menu_length(char* menu[]) {
  uint32_t num_items = 0;
  if (menu != NULL) {
    while (menu[num_items] != NULL) {
      ESP_LOGI(TAG, "Item: %s", menu[num_items]);
      num_items++;
    }
  }
  return num_items;
}

bool menu_screens_is_configuration(screen_module_menu_t menu) {
  if (is_menu_configuration(menu) && current_menu == menu) {
    return true;
  } else {
    return false;
  }
}

void menu_screens_exit_submenu() {
  ESP_LOGI(TAG, "Exiting submenu");
  previous_menu = prev_menu_table[current_menu];
  ESP_LOGI(TAG, "Previous: %s Current: %s", menu_list[previous_menu],
           menu_list[current_menu]);

  if (exit_submenu_cb != NULL) {
    exit_submenu_cb();
  }

  // TODO: Store selected item history into flash
  selected_item = selected_item_history[current_menu];
  current_menu = previous_menu;
  menu_screens_display_menu();
}

void handle_user_selection(screen_module_menu_t user_selection) {
  if (enter_submenu_cb != NULL) {
    enter_submenu_cb(user_selection);
    return;
  }

  switch (user_selection) {
    case MENU_WEB_SD_BROWSER:
      web_file_browser_module_init();
      break;
    case MENU_SETTINGS_WIFI:
      config_module_begin(MENU_SETTINGS_WIFI);
      break;
    case MENU_SETTINGS:
      settings_module_begin();
      break;
    case MENU_WIFI_APPS:
      wifi_module_begin();
      break;
    case MENU_BLUETOOTH_TRAKERS_SCAN:
      ble_module_begin(MENU_BLUETOOTH_TRAKERS_SCAN);
      break;
    case MENU_BLUETOOTH_SPAM:
      ble_module_begin(MENU_BLUETOOTH_SPAM);
      break;
    case MENU_ZIGBEE_SWITCH:
      zigbee_module_begin(MENU_ZIGBEE_SWITCH);
      break;
    case MENU_ZIGBEE_SNIFFER:
      zigbee_module_begin(MENU_ZIGBEE_SNIFFER);
      break;
    case MENU_THREAD_BROADCAST:
    case MENU_THREAD_APPS:
      open_thread_module_begin(MENU_THREAD_APPS);
      break;
    case MENU_MATTER_APPS:
    case MENU_ZIGBEE_LIGHT:
    case MENU_SETTINGS_DISPLAY:
    case MENU_SETTINGS_SOUND:
      oled_screen_clear();
      menu_screens_display_text_banner("In development");
      break;
    case MENU_GPS:
      gps_module_begin();
      break;
    case MENU_ABOUT_UPDATE:
      ota_module_init();
      break;
    default:
      ESP_LOGI(TAG, "Unhandled menu: %s", menu_list[user_selection]);
      break;
  }
}

void menu_screens_enter_submenu() {
  ESP_LOGI(TAG, "Selected item: %d", selected_item);
  screen_module_menu_t next_menu = MENU_MAIN;

  if (is_menu_empty(current_menu)) {
    ESP_LOGW(TAG, "Empty menu");
    return;
  } else if (is_menu_vertical_scroll(current_menu)) {
    selected_item = 0;  // Avoid selecting invalid item
    next_menu = current_menu;
  } else if (is_menu_configuration(current_menu)) {
    next_menu = current_menu;
  } else {
    next_menu = next_menu_table[current_menu][selected_item];
  }

  ESP_LOGI(TAG, "Previous: %s Current: %s", menu_list[current_menu],
           menu_list[next_menu]);

  handle_user_selection(next_menu);

  if (current_menu != next_menu) {
    selected_item_history[next_menu] = selected_item;
    selected_item = 0;
  }
  current_menu = next_menu;

  if (!app_state.in_app) {
    menu_screens_display_menu();
  }
}

uint8_t menu_screens_get_selected_item() {
  return selected_item;
}

void menu_screens_ingrement_selected_item() {
  selected_item = (selected_item == num_items - MAX_MENU_ITEMS_PER_SCREEN)
                      ? selected_item
                      : selected_item + 1;
  menu_screens_display_menu();
}

void menu_screens_decrement_selected_item() {
  selected_item = (selected_item == 0) ? 0 : selected_item - 1;
  menu_screens_display_menu();
}

void menu_screens_display_text_banner(char* text) {
#ifdef CONFIG_RESOLUTION_128X64
  uint8_t page = 3;
#else  // CONFIG_RESOLUTION_128X32
  uint8_t page = 2;
#endif
  oled_screen_display_text_center(text, page, OLED_DISPLAY_NORMAL);
}

void menu_screens_update_options(char* options[], uint8_t selected_option) {
  uint32_t menu_length = menu_screens_get_menu_length(options);
  uint8_t option = selected_option + 1;
  uint8_t i = 0;

  for (i = 1; i < menu_length; i++) {
    char* prev_item = options[i];
    // ESP_LOGI(TAG, "Prev item: %s", prev_item);
    char* new_item = malloc(strlen(prev_item) + 5);
    char* start_of_number = strchr(prev_item, ']') + 2;
    if (i == option) {
      snprintf(new_item, strlen(prev_item) + 5, "[x] %s", start_of_number);
      options[i] = new_item;
    } else {
      snprintf(new_item, strlen(prev_item) + 5, "[ ] %s", start_of_number);
      options[i] = new_item;
    }
    // ESP_LOGI(TAG, "New item: %s", options[i]);
  }
  options[i] = NULL;
}

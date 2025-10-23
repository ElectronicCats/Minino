#include "display_settings.h"
#include <string.h>
#include "bitmaps_general.h"
#include "esp_log.h"
#include "general_radio_selection.h"
#include "led_events.h"
#include "menus_module.h"
#include "modals_module.h"
#include "oled_screen.h"
#include "preferences.h"

#define TAG_DISPLAY_CONFIG "DISPLAY_MODULE"

#define TIMER_MAX_TIME 360
#define TIMER_MIN_TIME 30

#ifdef CONFIG_RESOLUTION_128X64
  #define ITEMOFFSET     2
  #define ITEMSPERSCREEN 4
  #define TIME_PAGE      4
  #define Y_N_OFFSET     4
#else  // CONFIG_RESOLUTION_128X32
  #define ITEMOFFSET     1
  #define ITEMSPERSCREEN 2
  #define TIME_PAGE      3
  #define Y_N_OFFSET     1
#endif

typedef enum { DISPLAY_MENU, DISPLAY_LIST, DISPLAY_COUNT } display_menu_t;

static char* display_settings_menu_items[] = {"Screen Logo", "Screen Time",
                                              NULL};
static const char** screen_saver_options = NULL;
static int selected_item = 0;
static int screen_selected = 0;
static int time_default_time = 30;

static void display_config_module_state_machine(uint8_t button_name,
                                                uint8_t button_event);
static void display_config_module_state_machine_menu_logo(uint8_t button_name,
                                                          uint8_t button_event);
static void display_config_module_state_machine_menu_time(uint8_t button_name,
                                                          uint8_t button_event);
static void display_config_module_state_machine_modal(uint8_t button_name,
                                                      uint8_t button_event);
void screen_saver_selector_menu();

static void config_module_wifi_display_selected_item(char* item_text,
                                                     uint8_t item_number) {
  oled_screen_display_bitmap(minino_face, 0, (item_number * 8), 8, 8,
                             OLED_DISPLAY_NORMAL);
  oled_screen_display_text(item_text, 16, item_number, OLED_DISPLAY_INVERT);
}

static void config_module_wifi_display_selected_item_center(
    char* item_text,
    uint8_t item_number) {
  oled_screen_display_bitmap(minino_face, 36, (item_number * 8), 8, 8,
                             OLED_DISPLAY_NORMAL);
  oled_screen_display_text(item_text, 56, item_number, OLED_DISPLAY_INVERT);
}

static void display_config_display_menu_item() {
  oled_screen_clear_buffer();
  oled_screen_display_text("< Exit", 0, 0, OLED_DISPLAY_NORMAL);
  for (int i = 0; display_settings_menu_items[i] != NULL; i++) {
    int page = (i + 1);
    if (selected_item == i) {
      config_module_wifi_display_selected_item(display_settings_menu_items[i],
                                               page);
    } else {
      oled_screen_display_text(display_settings_menu_items[i], 0, page,
                               OLED_DISPLAY_NORMAL);
    }
  }
  oled_screen_display_show();
}

static void display_config_display_list_logo() {
  oled_screen_clear_buffer();
  oled_screen_display_text("< Back", 0, 0, OLED_DISPLAY_NORMAL);
  int current_scren = preferences_get_int("dp_select", 0);
  int position = 1;
  uint16_t start_item = (selected_item / ITEMSPERSCREEN) * ITEMSPERSCREEN;
  for (uint16_t i = start_item;
       i < start_item + ITEMSPERSCREEN && epd_bitmaps_list[i] != NULL; i++) {
    char display_text[18];
    if (i == current_scren) {
      sprintf(display_text, "%s..[Curr]", epd_bitmaps_list[i]);
    } else {
      sprintf(display_text, "%s", epd_bitmaps_list[i]);
    }
    if (selected_item == i) {
      config_module_wifi_display_selected_item(display_text, position);
    } else {
      oled_screen_display_text(display_text, 0, position, OLED_DISPLAY_NORMAL);
    }
    position = position + ITEMOFFSET;
  }
  oled_screen_display_show();
}

static void display_config_display_time_selection() {
  oled_screen_clear_buffer();
  oled_screen_display_text("< Back", 0, 0, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("Time in seconds", 1, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("Min:30 - Max:360", 2, OLED_DISPLAY_NORMAL);
  char time_text[18];
  sprintf(time_text, "Time: %d", time_default_time);
  oled_screen_display_text_center(time_text, TIME_PAGE, OLED_DISPLAY_INVERT);
  oled_screen_display_show();
}

void display_config_module_begin() {
  ESP_LOGI(TAG_DISPLAY_CONFIG, "Initializing ble module screen state machine");
  menus_module_set_app_state(true, display_config_module_state_machine);
  display_config_display_menu_item();
};

static void display_settings_show_modal() {
  oled_screen_clear_buffer();
  oled_screen_display_text_center("Apply this config?", TIME_PAGE - 3,
                                  OLED_DISPLAY_NORMAL);
  if (selected_item == 0) {
    config_module_wifi_display_selected_item_center("YES", Y_N_OFFSET);
    oled_screen_display_text_center("NO", Y_N_OFFSET + 1, OLED_DISPLAY_NORMAL);
  } else {
    oled_screen_display_text_center("YES", Y_N_OFFSET, OLED_DISPLAY_NORMAL);
    config_module_wifi_display_selected_item_center("NO", Y_N_OFFSET + 1);
  }
  oled_screen_display_show();
}

static void display_config_module_state_machine(uint8_t button_name,
                                                uint8_t button_event) {
  if (button_event != BUTTON_PRESS_DOWN) {
    return;
  }
  switch (button_name) {
    case BUTTON_LEFT:
      menus_module_exit_app();
      break;
    case BUTTON_RIGHT:
      ESP_LOGI(TAG_DISPLAY_CONFIG, "Selected item: %d", selected_item);
      if (selected_item == 0) {
        screen_saver_selector_menu();
      } else {
        selected_item = 0;
        menus_module_set_app_state(
            true, display_config_module_state_machine_menu_time);
        display_config_display_time_selection();
      }

      break;
    case BUTTON_UP:
      selected_item =
          (selected_item == 0) ? DISPLAY_COUNT - 1 : selected_item - 1;
      display_config_display_menu_item();
      break;
    case BUTTON_DOWN:
      selected_item =
          (selected_item == DISPLAY_COUNT - 1) ? 0 : selected_item + 1;
      display_config_display_menu_item();
      break;
    case BUTTON_BOOT:
    default:
      break;
  }
}

static void display_config_module_state_machine_menu_time(
    uint8_t button_name,
    uint8_t button_event) {
  if (button_event != BUTTON_PRESS_DOWN) {
    return;
  }
  switch (button_name) {
    case BUTTON_LEFT:
      selected_item = 0;
      menus_module_set_app_state(true, display_config_module_state_machine);
      display_config_display_menu_item();
      break;
    case BUTTON_RIGHT:
      ESP_LOGI(TAG_DISPLAY_CONFIG, "Selected item: %d", selected_item);
      preferences_put_int("dp_time", time_default_time);
      oled_screen_clear();
      modals_module_show_banner("Saved");
      keyboard_module_reset_idle_timer();
      vTaskDelay(2000 / portTICK_PERIOD_MS);
      menus_module_set_app_state(true, display_config_module_state_machine);
      selected_item = 0;
      display_config_display_menu_item();
      break;
    case BUTTON_UP:
      time_default_time = (time_default_time == TIMER_MAX_TIME)
                              ? TIMER_MIN_TIME
                              : time_default_time + 10;
      display_config_display_time_selection();
      break;
    case BUTTON_DOWN:
      time_default_time = (time_default_time == TIMER_MIN_TIME)
                              ? TIMER_MAX_TIME
                              : time_default_time - 10;
      display_config_display_time_selection();
      break;
    case BUTTON_BOOT:
    default:
      break;
  }
}

static void display_config_module_state_machine_menu_logo(
    uint8_t button_name,
    uint8_t button_event) {
  if (button_event != BUTTON_PRESS_DOWN) {
    return;
  }
  switch (button_name) {
    case BUTTON_LEFT:
      selected_item = 0;
      menus_module_set_app_state(true, display_config_module_state_machine);
      display_config_display_menu_item();
      break;
    case BUTTON_RIGHT:
      ESP_LOGI(TAG_DISPLAY_CONFIG, "Selected item: %d", selected_item);
      screen_selected = selected_item;
      selected_item = 0;
      menus_module_set_app_state(true,
                                 display_config_module_state_machine_modal);
      display_settings_show_modal();
      break;
    case BUTTON_UP:
      selected_item =
          (selected_item == 0) ? MININO_COUNT - 1 : selected_item - 1;
      display_config_display_list_logo();
      break;
    case BUTTON_DOWN:
      selected_item =
          (selected_item == MININO_COUNT - 1) ? 0 : selected_item + 1;
      display_config_display_list_logo();
      break;
    case BUTTON_BOOT:
    default:
      break;
  }
}

static void display_config_module_state_machine_modal(uint8_t button_name,
                                                      uint8_t button_event) {
  if (button_event != BUTTON_PRESS_DOWN) {
    return;
  }
  switch (button_name) {
    case BUTTON_LEFT:
      menus_module_set_app_state(true, display_config_module_state_machine);
      display_config_display_menu_item();
      break;
    case BUTTON_RIGHT:
      if (selected_item == 0) {
        ESP_LOGI(TAG_DISPLAY_CONFIG, "Selected item: %d", selected_item);
        preferences_put_int("dp_select", screen_selected);
        oled_screen_clear();
        modals_module_show_banner("Saved");
        vTaskDelay(2000 / portTICK_PERIOD_MS);
      }
      menus_module_set_app_state(true, display_config_module_state_machine);
      selected_item = 0;
      display_config_display_menu_item();
      break;
    case BUTTON_UP:
      selected_item = (selected_item == 0) ? 1 : selected_item - 1;
      display_settings_show_modal();
      break;
    case BUTTON_DOWN:
      selected_item = (selected_item == 1) ? 0 : selected_item + 1;
      display_settings_show_modal();
      break;
    case BUTTON_BOOT:
    default:
      break;
  }
}
static void screen_saver_selection_exit() {
  selected_item = 0;
  menus_module_set_app_state(true, display_config_module_state_machine);
  display_config_display_menu_item();
}

static void screen_saver_selection_handler(uint8_t screen_saver) {
  preferences_put_int("dp_select", screen_saver);
}

const char** get_screen_saver_names() {
  uint8_t screen_savers_count = sizeof(screen_savers) / sizeof(epd_bitmap_t*);
  const char** names = malloc(sizeof(const char*) * screen_savers_count);
  for (uint8_t i = 0; i < screen_savers_count; i++) {
    names[i] = screen_savers[i]->name;
  }
  return names;
}
void screen_saver_selector_menu() {
  uint8_t screen_savers_count = sizeof(screen_savers) / sizeof(epd_bitmap_t*);
  screen_saver_options = get_screen_saver_names();
  general_radio_selection_menu_t screen_saver_logo_menu;
  screen_saver_logo_menu.banner = "Screen Saver";
  screen_saver_logo_menu.exit_cb = screen_saver_selection_exit;
  screen_saver_logo_menu.options = screen_saver_options;
  screen_saver_logo_menu.options_count = screen_savers_count;
  screen_saver_logo_menu.style = RADIO_SELECTION_OLD_STYLE;
  screen_saver_logo_menu.current_option =
      MIN(screen_savers_count - 1, preferences_get_int("dp_select", 0));
  screen_saver_logo_menu.select_cb = screen_saver_selection_handler;
  general_radio_selection(screen_saver_logo_menu);
}
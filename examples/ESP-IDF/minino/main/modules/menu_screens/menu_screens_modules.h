#ifndef DISPLAY_H
#define DISPLAY_H

#include "bluetooth_scanner.h"
#include "buzzer.h"
#include "cmd_sniffer.h"
#include "keyboard.h"
#include "screen_modules.h"
#include "sh1106.h"
#include "wifi_sniffer.h"

#define INVERT    1
#define NO_INVERT 0

extern uint8_t selected_item;
extern Layer previous_layer;
extern Layer current_layer;
extern int num_items;

/**
 * @brief Struct to hold the keyboard state in app
 */
typedef struct {
  bool in_app;
  void (*app_handler)(button_event_t button_pressed);
} app_state_t;

void menu_screens_init();
void display_clear(void);
void display_show(void);
void display_text(const char* text, int x, int page, int invert);
void display_clear_line(int x, int page, int invert);
void display_bitmap(const uint8_t* bitmap,
                    int x,
                    int y,
                    int width,
                    int height,
                    int invert);
void display_selected_item_box();
char** add_empty_strings(char** array, int length);
char** remove_srolling_text_flag(char** items, int length);
char** get_menu_items();
void display_menu_items(char** items);
void display_scrolling_text(char** text);
void display_menu();
void display_wifi_sniffer_animation_task(void* pvParameter);
void display_wifi_sniffer_animation_start();
void display_wifi_sniffer_animation_stop();
void display_wifi_sniffer_cb(sniffer_runtime_t* sniffer);
void display_bluetooth_scanner(bluetooth_scanner_record_t record);
void display_thread_cli();
void display_in_development_banner();
void display_gps_init();
void display_gps_deinit();

/**
 * @brief Get the app state
 *
 * @return app_state_t
 */
app_state_t menu_screens_get_app_state();

/**
 * @brief Update the app state
 *
 * @param bool in_app
 * @param void app_handler function pointer
 */
void menu_screens_set_app_state(
    bool in_app,
    void (*app_handler)(button_event_t button_pressed));

/**
 * @brief Exit the submenu
 *
 * @return void
 */
void menu_screens_exit_submenu();

/**
 * @brief Enter the submenu
 *
 * @return void
 */
void menu_screens_enter_submenu();

/**
 * @brief Increment the selected item
 *
 * @return void
 */
void menu_screens_ingrement_selected_item();

/**
 * @brief Decrement the selected item
 *
 * @return void
 */
void menu_screens_decrement_selected_item();

/**
 * @brief Update the previous layer
 *
 * @return void
 */
void menu_screens_update_previous_layer();

#endif  // DISPLAY_H

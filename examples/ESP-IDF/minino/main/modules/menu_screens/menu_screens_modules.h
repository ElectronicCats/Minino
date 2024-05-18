#pragma once

#include "bluetooth_scanner.h"
#include "buzzer.h"
#include "cmd_sniffer.h"
#include "keyboard_module.h"
#include "oled_driver.h"
#include "screens_modules.h"

typedef void (*app_handler_t)(button_event_t button_pressed);

/**
 * @brief Struct to hold the keyboard state in app
 */
typedef struct {
  bool in_app;
  app_handler_t app_handler;
} app_state_t;

/**
 * @brief Enum with the screen module controls
 */
typedef enum {
  SCREEN_IN_NAVIGATION = 0,
  SCREEN_IN_APP,
} screen_module_controls_type_t;

/**
 * @brief Initialize the menu screens
 *
 * @return void
 */
void menu_screens_begin();

/**
 * @brief Display the main menu
 *
 * @return void
 */
void menu_screens_display_menu();

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
 * @param app_handler_t app_handler
 *
 * @return void
 */
void menu_screens_set_app_state(bool in_app, app_handler_t app_handler);

/**
 * @brief Get the current menu
 *
 * @return screen_module_menu_t
 */
screen_module_menu_t menu_screens_get_current_menu();

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

// TODO: Move to separate files
void display_bluetooth_scanner(bluetooth_scanner_record_t record);
void display_thread_cli();
void display_in_development_banner();
void display_gps_init();
void display_gps_deinit();

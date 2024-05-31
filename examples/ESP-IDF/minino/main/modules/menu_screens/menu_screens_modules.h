#pragma once

#include "buzzer.h"
#include "cmd_sniffer.h"
#include "keyboard_module.h"
#include "screens_modules.h"

typedef void (*app_handler_t)(button_event_t button_pressed);

/**
 * @brief Structure to store the app screen state information
 *
 */
typedef struct {
  bool in_app;
  int app_selected;
} app_screen_state_information_t;

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
 * @brief Get the current menu length
 *
 * @param char* menu[] The menu
 *
 * @return uint32_t
 */
uint32_t menu_screens_get_menu_length(char* menu[]);

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
 * @brief Display `In development` banner
 *
 * @return void
 */
void menu_screens_display_in_development_banner();

/**
 * @brief Display `Loading...` banner
 *
 * @return void
 */
void menu_screens_display_loading_banner();

/**
 * @brief Update the items array
 *
 * Set [x] to the selected option
 * Set [ ] to the other options
 *
 * @param char* options[] The options
 * @param uint8_t selected_option The selected option
 *
 * @return void
 */
void menu_screens_update_options(char* options[], uint8_t selected_option);

// TODO: Move to separate files
void display_thread_broadcast();
void display_gps_init();
void display_gps_deinit();

/**
 * @brief Update the keyboard state
 *
 * @param bool in_app
 * @param void app_handler function pointer
 */
void module_keyboard_update_state(
    bool in_app,
    void (*app_handler)(button_event_t button_pressed));

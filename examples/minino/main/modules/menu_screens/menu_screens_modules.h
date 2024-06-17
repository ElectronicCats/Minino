#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "buzzer.h"
#include "cmd_sniffer.h"
#include "keyboard_module.h"
#include "screens_modules.h"

typedef void (*app_handler_t)(button_event_t button_pressed);
typedef void (*enter_submenu_cb_t)(screen_module_menu_t user_selection);
typedef void (*exit_submenu_cb_t)();

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
 * @brief Register enter submenu callback
 *
 * @param enter_submenu_cb_t enter_submenu_cb The user selection callback
 *
 * @return void
 */
void menu_screens_register_enter_submenu_cb(enter_submenu_cb_t cb);

/**
 * @brief Register exit submenu callback
 *
 * @param exit_submenu_cb_t enter_submenu_cb The user selection callback
 *
 * @return void
 */

/**
 * @brief Register exit submenu callback
 *
 * @param exit_submenu_cb_t cb The user selection callback
 *
 * @return void
 */
void menu_screens_register_exit_submenu_cb(exit_submenu_cb_t cb);

/**
 * @brief Unregister submenu callbacks
 *
 * @return void
 */
void menu_screens_unregister_submenu_cbs();

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
 * @brief Check if the given menu is configuration
 *
 * @param screen_module_menu_t user_selection The user selection
 *
 * @return bool
 */
bool menu_screens_is_configuration(screen_module_menu_t user_selection);

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
 * @brief Get the selected item
 *
 * @return uint8_t
 */
uint8_t menu_screens_get_selected_item();

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
 * @brief Display `text` banner
 *
 * @param char* text The text to display
 *
 * @return void
 */
void menu_screens_display_text_banner(char* text);

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

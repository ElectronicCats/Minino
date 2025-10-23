// Menus related
#include "general_notification.h"
#include "general_radio_selection.h"
#include "general_scrolling_text.h"
#include "general_submenu.h"
// For custom implementation of keyboard events
#include "menus_module.h"
#include "oled_screen.h"
// Save preferences in flash
#include "preferences.h"
// ESP libaries
#include "esp_log.h"
// Header for this file
#include "hello_module.h"

#define RADIO_SELECTOR_KEY "hellosel"

/* Here we init or options text for the menu */
static const char* main_menu_options[] = {"Say Hello", "Radio Menu",
                                          "Custom Event", "Help"};
static const char* help_text[] = {"This is a", "example app.", "This menu is",
                                  "scrolling", "if you want",  "to implement",
                                  "dont use",  "lines above",  "15 chars."};
static const char* help_text_custom[] = {
    "Press once",    "LEFT button",   "to exit",

    "Keep pressing", "the UP button", "for a while",
    "to trigger",    "LONG PRESS",    "UP",

    "Double press",  "RIGHT button",  "to trigger",
    "DOUBLE CLICK",

    "Keep pressing", "the DOWN",      "button for",
    "a while",       "to trigger",    "LONG PRESS",
    "HOLD"};
static char* radio_options[] = {"Single", "Timed"};
// Use this variable to save the index of the last selection (this is optional)
static uint16_t last_main_selection = 0;

// Due to the use of this function in many part of the code, we declare first
static void set_custom_keyboard_event();

// Init an enum to handle the selections
typedef enum {
  SAY_HELLO,
  RADIO_SELECTOR,
  CUSTOM_EVENT,
  HELP,
} main_menu_options_t;

typedef enum {
  RADIO_SINGLE,
  RADIO_TIMED,
} radio_selector_item_t;

/*
 * Displays a scrolling help for custom keyboard events.
 * This screen stays visible until the user presses the back button.
 */
static void hello_show_custom_help() {
  general_scrolling_text_ctx help = {
      0};                      // Create and initialize the help configuration
  help.banner = "How to use";  // Title at the top of the help screen
  help.text_arr = help_text_custom;  // Array of text lines to display
  help.text_len =
      sizeof(help_text_custom) / sizeof(char*);      // Total number of lines
  help.window_type = GENERAL_SCROLLING_TEXT_WINDOW;  // Set the window style
  help.scroll_type =
      GENERAL_SCROLLING_TEXT_CLAMPED;        // Scrolling stops at the end
  help.exit_cb = set_custom_keyboard_event;  // What to do when the user exits
                                             // (go back to main menu)
  general_scrolling_text_array(help);        // Show the help screen
}

/*
 * Displays a scrolling help screen.
 * This screen stays visible until the user presses the back button.
 */
static void hello_show_help() {
  general_scrolling_text_ctx help = {
      0};                      // Create and initialize the help configuration
  help.banner = "Hello Help";  // Title at the top of the help screen
  help.text_arr = help_text;   // Array of text lines to display
  help.text_len = sizeof(help_text) / sizeof(char*);  // Total number of lines
  help.window_type = GENERAL_SCROLLING_TEXT_WINDOW;   // Set the window style
  help.scroll_type =
      GENERAL_SCROLLING_TEXT_CLAMPED;  // Scrolling stops at the end
  help.exit_cb =
      hello_main;  // What to do when the user exits (go back to main menu)
  general_scrolling_text_array(help);  // Show the help screen
}
/*
 * Displays a notification message on screen.
 * It stays visible until the user presses the back button.
 */
static void hello_show_notify() {
  general_notification_ctx_t notification = {
      0};                       // Create and initialize the notification
  notification.head = "Hello";  // Title of the notification
  notification.body = "Minino. Press button back to exit.";  // Message content
  notification.on_exit = hello_main;  // Go back to the main menu when exiting
  general_notification_handler(notification);  // Show the notification
}
/*
 * Displays a notification for a fixed amount of time (2 seconds).
 * After the time expires, it returns to the main menu automatically.
 */
static void hello_show_notify_timed() {
  general_notification_ctx_t notification = {0};
  notification.duration_ms = 2000;  // Display time in milliseconds
  notification.head = "Hello";
  notification.body = "Minino. Wait 2 sec to back to the menu.";
  general_notification(notification);  // Show the timed notification
  hello_main();                        // Return to the main menu
}
/*
 * This function is called when the user selects an option in the radio menu.
 * It saves the selected option into flash memory.
 */
static void hello_radio_handler(uint8_t option) {
  /* Save the value of the selected option into the flash,
  we use a key to create a relation between the key and user selection*/
  preferences_put_int(RADIO_SELECTOR_KEY, option);
}
/*
 * Displays a radio-style menu (only one option can be selected).
 * User can choose between "Single" or "Timed".
 */
static void hello_show_radio_selector() {
  general_radio_selection_menu_t settings = {0};
  settings.banner = "Hello Type";    // Menu title
  settings.options = radio_options;  // Options to choose from
  settings.options_count = sizeof(radio_options) / sizeof(char*);
  settings.select_cb =
      hello_radio_handler;  // Function to call when user selects
  settings.style = RADIO_SELECTION_OLD_STYLE;  // Visual style of the menu
                                               // (RADIO_SELECTION_NEW_STYLE /
                                               // RADIO_SELECTION_OLD_STYLE)
  settings.exit_cb = hello_main;               // Return to main menu on exit
  settings.current_option = preferences_get_int(
      RADIO_SELECTOR_KEY, 0);         // Load saved option or use default
  general_radio_selection(settings);  // Show the radio menu
}
/* This is action function when the button and event is actionated */
static void custom_keyboard_event(char* event_name) {
  general_notification_ctx_t notification = {
      0};                       // Create and initialize the notification
  notification.head = "Event";  // Title of the notification
  notification.body = event_name;
  notification.on_exit =
      set_custom_keyboard_event;  // Go back to the main menu when exiting
  general_notification_handler(notification);  // Show the notification
}

/* This handler is used to do a custom event for our aplication */
static void custom_keyboard_handler(uint8_t button_index,
                                    uint8_t button_event) {
  /* The button_index is the value of the button pressed by the user and
  the button_event is the type of button event, we have many events:
  - BUTTON_PRESS_DOWN = 0,
  - BUTTON_PRESS_UP,
  - BUTTON_PRESS_REPEAT,
  - BUTTON_PRESS_REPEAT_DONE,
  - BUTTON_SINGLE_CLICK,
  - BUTTON_DOUBLE_CLICK,
  - BUTTON_LONG_PRESS_START,
  - BUTTON_LONG_PRESS_HOLD,
  - BUTTON_LONG_PRESS_UP,
  - BUTTON_PRESS_END,
  And for the buttons types we have:
  BUTTON_BOOT = 0,
  BUTTON_LEFT,
  BUTTON_RIGHT,
  BUTTON_UP,
  BUTTON_DOWN,

  So we just want a event of type BUTTON_PRESS_DOWN, BUTTON_DOUBLE_CLICK,
  BUTTON_LONG_PRESS_HOLD and BUTTON_LONG_PRESS_UP

  */
  switch (button_index) {
    /* We will use this event to get back to the main menu of our app */
    case BUTTON_LEFT:
      if (button_event == BUTTON_PRESS_DOWN) {
        hello_main();
      }
      break;
    /* We will use this event to wait for a BUTTON_LONG_PRESS_UP to show a event
     */
    case BUTTON_UP:
      if (button_event == BUTTON_LONG_PRESS_UP) {
        custom_keyboard_event("BUTTON LONG PRESS UP");
      }
      break;
    /* We will use this event to wait for a BUTTON_DOUBLE_CLICK to show a event
     */
    case BUTTON_RIGHT:
      if (button_event == BUTTON_DOUBLE_CLICK) {
        custom_keyboard_event("BUTTON DOUBLE CLICK");
      }
      break;
    /* We will use this event to wait for a BUTTON_LONG_PRESS_HOLD to show a
     * event */
    case BUTTON_DOWN:
      if (button_event == BUTTON_LONG_PRESS_HOLD) {
        custom_keyboard_event("BUTTON LONG PRESS HOLD");
      }
      break;
    default:
      break;
  }
}

/* This function will setup the handlers to use our custom events */
static void set_custom_keyboard_event() {
  // This function will clear all the oled screen;
  oled_screen_clear();
  /* We display the title of the screen at row 0 and inverted display */
  oled_screen_display_text_center("Custom Events", 0, OLED_DISPLAY_INVERT);
  /* We display a splited text, this allow us to set large text but limited due
  to this not handle a scrolling text. We need to pass the text, a pointer to
  the a variable where we store or counter for each row and the style of display
*/
  int row_index = 2;
  oled_screen_display_text_splited(
      "Press the buttoms to use your custom event keyboard", &row_index,
      OLED_DISPLAY_NORMAL);
  menus_module_set_app_state(true, custom_keyboard_handler);
}

/* Main Handler: This function will handle the callback when a item is pressed*/
static void hello_main_handler(uint8_t option) {
  last_main_selection = option;
  switch (option) {
    case SAY_HELLO:
      if (preferences_get_int(RADIO_SELECTOR_KEY, 0) == 0) {
        /* Show the notification
        with this function the modal will stay until the back button is pressed
      */
        hello_show_notify();
      } else {
        /* With this function the modal will stay until the time is complete */
        hello_show_notify_timed();
      }
      break;
    case RADIO_SELECTOR:
      hello_show_radio_selector();
      break;
    case CUSTOM_EVENT:
      hello_show_custom_help();
      break;
    case HELP:
      hello_show_help();
      break;
    default:
      break;
  }
}

/* Main function: This function will show the main menu*/
void hello_main() {
  general_submenu_menu_t main = {0};
  main.options = main_menu_options;  // Set the list of menu options
  main.options_count = sizeof(main_menu_options) /
                       sizeof(char*);  // Count how many options there are
  main.select_cb =
      hello_main_handler;  // Function to call when an option is selected
  main.selected_option = last_main_selection;  // Restore last selected item
  // Function to call when exiting the menu. menus_module_restart is our default
  // function from menus_module.h file
  main.exit_cb = menus_module_restart;
  general_submenu(main);  // Show the menu on screen
}
# Hello World Minino
We'll build our first module, minino has a folder called "general" this contains some code for general usage.

├── general
│   ├── bitmaps_general.h
│   ├── general_flash_storage
│   ├── general_knob
│   ├── general_notification
│   ├── general_radio_selection
│   ├── general_screens.c
│   ├── general_screens.h
│   ├── general_scrolling_text
│   └── general_submenu

| Item  | Description  |
|---|---|
|  bitmaps_general.h | This file have the general bitmaps to use with animation or as icon |
| general_flash_storage  | This file have function to use to save list of items in the flash, like WIFI AP list  |
|  general_knob |  This file have a knob interface for custom user input values |
|  general_notification |  This file have a notification interface |
|  general_radio_selection |  This file have a "radio" selector |
|  general_screens |  This file have the logic to build the general GUI |
|  general_scrolling_text |  This file have the logic to scrolling text |
|  general_submenu |  This file have the logic to build submenus |
| | |

## Folder struct
Our main folder structure are:
├── components -> Single logic components for general usage
│   ├── ble_hid
│   ├── ble_scann
│   ├── bt_gattc
│   ├── bt_gatts
│   ├── bt_spam
│   ├── buzzer
│   ├── cmd_wifi
│   ├── console
│   ├── dns_server
│   ├── drone_id
│   ├── files_ops
│   ├── flash_fs
│   ├── ieee_sniffer
│   ├── ieee802154
│   ├── ledc_controller
│   ├── leds
│   ├── minino_config
│   ├── nmea_parser
│   ├── OTA
│   ├── preferences
│   ├── radio_selector
│   ├── sd_card
│   ├── trackers_scanner
│   ├── uart_bridge
│   ├── uart_sender
│   ├── wifi_ap_manager
│   ├── wifi_app
│   ├── wifi_attacks
│   ├── wifi_captive
│   ├── wifi_controller
│   ├── wifi_scanner
│   ├── wifi_sniffer
├── main -> Main project structure folder
│   ├── apps
│   ├── drivers
│   ├── entities
│   ├── general
│   ├── modules
│   └── templates
├── managed_components -> Third party components
│   ├── espressif__button
│   ├── espressif__cmake_utilities
│   ├── espressif__esp-modbus
│   ├── espressif__iperf
│   └── espressif__pcap
├── profiles -> Custom boards configs
│   ├── bsides
│   ├── bsseattle
│   ├── bugcon
│   ├── dragonjar
│   ├── ekoparty
│   └── minino
└── resources -> Static resources


If you want to do an "app" like using ble stack or game then you need to put in the "app" folder inside "main".

---

## Start coding
First we need to create a folder inside the `main/app` folder with the name of our aplication, in this case "hello" will be the name.

> Note: In case of you need to use space named folder, use a underscore. "hello world" -> "hello_world"

### Coding our module
Our module need a exposed function to call from any other file so we create our "hello_module.h" inside the folder of our app and add this code:
```c
// hello_module.h

/* If the HELLO_MODULE_H is already defined then the compiler not need to append again */
#ifndef HELLO_MODULE_H
/* If not, then the compiler will take this file. We do this to avoid recalls of functions */
#define HELLO_MODULE_H

// Set our main function. We will use this when we need to call our module
void hello_main();

#endif
```
Then we write or complete hello app, this app will contain three options:
- Say hello: This will show "Hello", the mode of display will be selected using a value saved in flash, if the value is 0, will show a modal where will need to press the back button to exit; if the value is 1, will show a modal and after 2 seconds we'll back to the main  menu
- Radio Menu: This is or selector for the type of hello option
- Help: This will show a help scrolling text



#### Coding the init function
First we need to add our headers files:
```c
// Menus related
#include "general_submenu.h"
#include "menus_module.h"
#include "general_notification.h"
#include "general_scrolling_text.h"
#include "general_radio_selection.h"
// Save preferences in flash
#include "preferences.h"
// ESP libaries
#include "esp_log.h"
```

Then we need to add our initial code, this function will be used to init all the aplicacion:
```c
/* Here we init or options text for the menu */
static const char* main_menu_options[] = {"Say Hello";
// Use this variable to save the index of the last selection (this is optional)
static uint16_t last_main_selection = 0;

// Init an enum to handle the selections
typedef enum {
  SAY_HELLO,
} main_menu_options_t;

/* 
 * Displays a notification message on screen.
 * It stays visible until the user presses the back button.
*/
static void hello_show_notify(){
  general_notification_ctx_t   notification = {0};                      // Create and initialize the notification
  notification.head            = "Hello";                               // Title of the notification
  notification.body            = "Minino. Press button back to exit.";  // Message content
  notification.on_exit         = hello_main;                            // Go back to the main menu when exiting
  general_notification_handler(notification);  // Show the notification
}

/* Main Handler: This function will handle the callback when a item is pressed*/
static void hello_main_handler(uint8_t option){
  last_main_selection = option;
  switch (option) {
  case SAY_HELLO:
    hello_show_notify();
    break;
  default:
    break;
  }
}

void hello_main(){
  general_submenu_menu_t main = {0};
  main.options         = main_menu_options;                          // Set the list of menu options
  main.options_count   = sizeof(main_menu_options) / sizeof(char*);  // Count how many options there are
  main.select_cb       = hello_main_handler;                         // Function to call when an option is selected
  main.selected_option = last_main_selection;                        // Restore last selected item
  main.exit_cb         = menus_module_restart;                       // Function to call when exiting the menu
  general_submenu(main);                                        // Show the menu on screen
}

```

This structure is the most basic to do an aplication, first we init our list of string options that will showing in the menu. Then we init a variable called `last_main_selection` to handle the last selected item (this is optional).
Our enum is used to organize de code, each item of the enum is related to a consecutive number initialized with 0, then 1, and so on. You can not use this enum and just handle by their index 0, 1 or 2.,
```c
typedef enum {
  SAY_HELLO,
} main_menu_options_t;
```

The function handler is a callback function, where the item is selected using the right button (selection) the callback is trigger and then by a switch statement, we handle what we do.
```c
static void hello_main_handler(uint8_t option){
  last_main_selection = option;
  switch (option) {
  case SAY_HELLO:
    ...
  ...
  default:
    break;
  }
}
```

The function `hello_show_notify` will create our modal notification to say "Hello Minino".
```c
static void hello_show_notify(){
  general_notification_ctx_t   notification = {0};                      // Create and initialize the notification
  notification.head            = "Hello";                               // Title of the notification
  notification.body            = "Minino";                              // Message content
  notification.on_exit         = hello_main;                            // Go back to the main menu when exiting
  general_notification_handler(notification);  // Show the notification
}
```

The `hello_main` is our init function, this function will show the menu items. In this function we init the struct with submenu items and options.
``` c
void hello_main(){
  general_submenu_menu_t main = {0};
  main.options         = main_menu_options;                          // Set the list of menu options
  main.options_count   = sizeof(main_menu_options) / sizeof(char*);  // Count how many options there are
  main.select_cb       = hello_main_handler;                         // Function to call when an option is selected
  main.selected_option = last_main_selection;                        // Restore last selected item
  main.exit_cb         = menus_module_restart;                       // Function to call when exiting the menu
  general_submenu(main);                                        // Show the menu on screen
}
```



#### Add to the menu
To add our example to the menu we need to modify the `menus.h` file inside `modules/menus_module/menus_include/`.
```c
// First we add our header
#include "hello_module.h"

// Then we need to add a new item
typedef enum {
  MENU_MAIN = 0,
  MENU_APPLICATIONS,
  MENU_SETTINGS,
  MENU_ABOUT,
  /* Applications */
  MENU_WIFI_APPS,
  MENU_BLUETOOTH_APPS,
  // MENU_ZIGBEE_APPS and MENU_THREAD_APPS are not available in this beta version (only in versions prior to 1.1.13.0)
  MENU_ZIGBEE_APPS,
  MENU_THREAD_APPS,
  MENU_GPS,
  MENU_GPIO_APPS,
  MENU_HELLO,          // <- Add new item to the main app menu
  ...
}

// And add a struct menu to the list
menu_t menus[] = {  //////////////////////////////////
  ...
  {
       .display_name          = "Hello APP",       // The name that will show in the menu
       .menu_idx              = MENU_HELLO,        // The index of the menu
       .parent_idx            = MENU_APPLICATIONS, // Parent index
       .entry_cmd             = "hello",           // Command for the CLI (optional)
       .last_selected_submenu = 0,                 // Is our main app so we put in 0
       .on_enter_cb           = hello_main,        // Our main function
       .on_exit_cb            = NULL,              // If we want a custom exit callback
       .is_visible            = true               // Turn visible
  },
  ...
};
```

---

#### Adding powers
Now we can add some powers to our minino, with this we will undestand more about how we can make more complex apps.

First we add more items to our main menu options like "radio menu" and "help":
- Radio menu: Allow the user to select the type of notification hello will show
- Help: Will show a scrolling help text

Then we add two more item list, the "radio_options" that has our two kind of notifications and the help text.

> Our **RADIO_SELECTOR_KEY** is a key that we use to save values to preferences in the flash

```c
#define RADIO_SELECTOR_KEY "hellosel"

/* Here we init or options text for the menu */
static const char* main_menu_options[] = {"Say Hello", "Radio Menu", "Help"};
static const char* help_text[] = {"This is a", "example app.", "This menu is", "scrolling", "if you want", "to implement","dont use", "lines above", "15 chars."};
static const char* radio_options[] = {"Single", "Timed"};
// Use this variable to save the index of the last selection (this is optional)
static uint16_t last_main_selection = 0;
```

We add our new items to our `main_menu_options_t` and declare our `set_custom_keyboard_event` function due to the use in many levels:
```c
// Due to the use of this function in many part of the code, we declare first
static void set_custom_keyboard_event();


typedef enum {
  SAY_HELLO,
  // Our new items
  RADIO_SELECTOR,
  CUSTOM_EVENT,
  HELP,
} main_menu_options_t;
```

Then we add one more enum for or radio options:
```c
typedef enum {
  RADIO_SINGLE,
  RADIO_TIMED,
} radio_selector_item_t;
```

And add our new functions to show the notifications and the help:
```c
/* 
 * Displays a scrolling help for custom keyboard events.
 * This screen stays visible until the user presses the back button.
*/
static void hello_show_custom_help() {
  general_scrolling_text_ctx help = {0}; // Create and initialize the help configuration
  help.banner = "How to use";            // Title at the top of the help screen
  help.text_arr = help_text_custom;             // Array of text lines to display
  help.text_len = sizeof(help_text_custom) / sizeof(char*); // Total number of lines
  help.window_type = GENERAL_SCROLLING_TEXT_WINDOW;  // Set the window style
  help.scroll_type = GENERAL_SCROLLING_TEXT_CLAMPED; // Scrolling stops at the end
  help.exit_cb = set_custom_keyboard_event;         // What to do when the user exits (go back to main menu)
  general_scrolling_text_array(help);    // Show the help screen
}
/* 
 * Displays a scrolling help screen.
 * This screen stays visible until the user presses the back button.
*/
static void hello_show_help() {
  general_scrolling_text_ctx help = {0}; // Create and initialize the help configuration
  help.banner = "Hello Help";            // Title at the top of the help screen
  help.text_arr = help_text;             // Array of text lines to display
  help.text_len = sizeof(help_text) / sizeof(char*); // Total number of lines
  help.window_type = GENERAL_SCROLLING_TEXT_WINDOW;  // Set the window style
  help.scroll_type = GENERAL_SCROLLING_TEXT_CLAMPED; // Scrolling stops at the end
  help.exit_cb = hello_main;             // What to do when the user exits (go back to main menu)
  general_scrolling_text_array(help);    // Show the help screen
}
/* 
 * Displays a notification message on screen.
 * It stays visible until the user presses the back button.
*/
static void hello_show_notify(){
  general_notification_ctx_t   notification = {0};                      // Create and initialize the notification
  notification.head            = "Hello";                               // Title of the notification
  notification.body            = "Minino. Press button back to exit.";  // Message content
  notification.on_exit         = hello_main;                            // Go back to the main menu when exiting
  general_notification_handler(notification);  // Show the notification
}
/*
 * Displays a notification for a fixed amount of time (2 seconds).
 * After the time expires, it returns to the main menu automatically.
 */
static void hello_show_notify_timed(){
  general_notification_ctx_t notification             = {0};
  notification.duration_ms = 2000;  // Display time in milliseconds
  notification.head        = "Hello";
  notification.body        = "Minino. Wait 2 sec to back to the menu.";
  general_notification(notification);  // Show the timed notification
  hello_main(); // Return to the main menu
}
```

Then we add our logic to handle the readio selection and showing the menu:
```c
/*
 * This function is called when the user selects an option in the radio menu.
 * It saves the selected option into flash memory.
*/
static void hello_radio_handler(uint16_t option){
  /* Save the value of the selected option into the flash,
  we use a key to create a relation between the key and user selection*/
  preferences_put_int(RADIO_SELECTOR_KEY, option);
}
/*
 * Displays a radio-style menu (only one option can be selected).
 * User can choose between "Single" or "Timed".
*/
static void hello_show_radio_selector(){
  general_radio_selection_menu_t settings = {0};
  settings.banner         = "Hello Type";                                // Menu title
  settings.options        = radio_options;                               // Options to choose from
  settings.options_count  = sizeof(radio_options) / sizeof(char*);
  settings.select_cb      = hello_radio_handler;                         // Function to call when user selects
  settings.style          = RADIO_SELECTION_OLD_STYLE;                   // Visual style of the menu (RADIO_SELECTION_NEW_STYLE / RADIO_SELECTION_OLD_STYLE)
  settings.exit_cb        = hello_main;                                  // Return to main menu on exit
  settings.current_option = preferences_get_int(RADIO_SELECTOR_KEY, 0);  // Load saved option or use default
  general_radio_selection(settings);  // Show the radio menu
}
```

To use our custom keyboard events we code a function to show the event as notification, setup a handler function for the buttons actions and a function to init our custom events:
```c
/* This is action function when the button and event is actionated */
static void custom_keyboard_event(char *event_name){
  general_notification_ctx_t   notification = {0};                      // Create and initialize the notification
  notification.head            = "Event";                               // Title of the notification
  notification.body            = event_name;
  notification.on_exit         = set_custom_keyboard_event;             // Go back to the main menu when exiting
  general_notification_handler(notification);  // Show the notification
}

/* This handler is used to do a custom event for our aplication */
static void custom_keyboard_handler(uint8_t button_index, uint8_t button_event){
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

  So we just want a event of type BUTTON_PRESS_DOWN, BUTTON_DOUBLE_CLICK, BUTTON_LONG_PRESS_HOLD and BUTTON_LONG_PRESS_UP

  */
  switch (button_index){
    /* We will use this event to get back to the main menu of our app */
    case BUTTON_LEFT:
      if (button_event == BUTTON_PRESS_DOWN){
        hello_main();
      }
      break;
    /* We will use this event to wait for a BUTTON_LONG_PRESS_UP to show a event */
    case BUTTON_UP:
      if (button_event == BUTTON_LONG_PRESS_UP){
        custom_keyboard_event("BUTTON LONG PRESS UP");
      }
      break;
    /* We will use this event to wait for a BUTTON_DOUBLE_CLICK to show a event */
    case BUTTON_RIGHT:
      if (button_event == BUTTON_DOUBLE_CLICK){
        custom_keyboard_event("BUTTON DOUBLE CLICK");
      }
      break;
    /* We will use this event to wait for a BUTTON_LONG_PRESS_HOLD to show a event */
    case BUTTON_DOWN:
      if (button_event == BUTTON_LONG_PRESS_HOLD){
        custom_keyboard_event("BUTTON LONG PRESS HOLD");
      }
      break;
    default:
      break;
  }
}

/* This function will setup the handlers to use our custom events */
static void set_custom_keyboard_event(){
  // This function will clear all the oled screen;
  oled_screen_clear();
  /* We display the title of the screen at row 0 and inverted display */
  oled_screen_display_text_center("Custom Events", 0, OLED_DISPLAY_INVERT);
  /* We display a splited text, this allow us to set large text but limited due to this not handle a scrolling text.
  We need to pass the text, a pointer to the a variable where we store or counter for each row and the style of display */
  int row_index = 2;
  oled_screen_display_text_splited("Press the buttoms to use your custom event keyboard", &row_index, OLED_DISPLAY_NORMAL);
  menus_module_set_app_state(true, custom_keyboard_handler);
}

```


And finally we update our current `hellp_main_handler` to add our new logic:
```c
/* Main Handler: This function will handle the callback when a item is pressed*/
static void hello_main_handler(uint8_t option){
  last_main_selection = option;
  switch (option) {
  case SAY_HELLO:
    if(preferences_get_int(RADIO_SELECTOR_KEY, 0) == 0){
      /* Show the notification
      with this function the modal will stay until the back button is pressed */
      hello_show_notify();
    }else{
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
```

---

# The full code for `hello_module`:
## `hello_module.c`

```c
// Menus related
#include "general_submenu.h"
#include "menus_module.h"
#include "general_notification.h"
#include "general_scrolling_text.h"
#include "general_radio_selection.h"
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
static const char* main_menu_options[] = {"Say Hello", "Radio Menu","Custom Event", "Help"};
static const char* help_text[] = {"This is a", "example app.", "This menu is", "scrolling", "if you want", "to implement","dont use", "lines above", "15 chars."};
static const char* help_text_custom[] = {
  "Press once",          
  "LEFT button",         
  "to exit",             

  "Keep pressing",       
  "the UP button",       
  "for a while",         
  "to trigger",          
  "LONG PRESS",     
  "UP",     

  "Double press",        
  "RIGHT button",        
  "to trigger",          
  "DOUBLE CLICK",        

  "Keep pressing",       
  "the DOWN",
  "button for",       
  "a while",         
  "to trigger",          
  "LONG PRESS",
  "HOLD"
};
static const char* radio_options[] = {"Single", "Timed"};
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
  general_scrolling_text_ctx help = {0}; // Create and initialize the help configuration
  help.banner = "How to use";            // Title at the top of the help screen
  help.text_arr = help_text_custom;             // Array of text lines to display
  help.text_len = sizeof(help_text_custom) / sizeof(char*); // Total number of lines
  help.window_type = GENERAL_SCROLLING_TEXT_WINDOW;  // Set the window style
  help.scroll_type = GENERAL_SCROLLING_TEXT_CLAMPED; // Scrolling stops at the end
  help.exit_cb = set_custom_keyboard_event;         // What to do when the user exits (go back to main menu)
  general_scrolling_text_array(help);    // Show the help screen
}

/* 
 * Displays a scrolling help screen.
 * This screen stays visible until the user presses the back button.
*/
static void hello_show_help() {
  general_scrolling_text_ctx help = {0}; // Create and initialize the help configuration
  help.banner = "Hello Help";            // Title at the top of the help screen
  help.text_arr = help_text;             // Array of text lines to display
  help.text_len = sizeof(help_text) / sizeof(char*); // Total number of lines
  help.window_type = GENERAL_SCROLLING_TEXT_WINDOW;  // Set the window style
  help.scroll_type = GENERAL_SCROLLING_TEXT_CLAMPED; // Scrolling stops at the end
  help.exit_cb = hello_main;             // What to do when the user exits (go back to main menu)
  general_scrolling_text_array(help);    // Show the help screen
}
/* 
 * Displays a notification message on screen.
 * It stays visible until the user presses the back button.
*/
static void hello_show_notify(){
  general_notification_ctx_t   notification = {0};                      // Create and initialize the notification
  notification.head            = "Hello";                               // Title of the notification
  notification.body            = "Minino. Press button back to exit.";  // Message content
  notification.on_exit         = hello_main;                            // Go back to the main menu when exiting
  general_notification_handler(notification);  // Show the notification
}
/*
 * Displays a notification for a fixed amount of time (2 seconds).
 * After the time expires, it returns to the main menu automatically.
 */
static void hello_show_notify_timed(){
  general_notification_ctx_t notification             = {0};
  notification.duration_ms = 2000;  // Display time in milliseconds
  notification.head        = "Hello";
  notification.body        = "Minino. Wait 2 sec to back to the menu.";
  general_notification(notification);  // Show the timed notification
  hello_main(); // Return to the main menu
}
/*
 * This function is called when the user selects an option in the radio menu.
 * It saves the selected option into flash memory.
*/
static void hello_radio_handler(uint16_t option){
  /* Save the value of the selected option into the flash,
  we use a key to create a relation between the key and user selection*/
  preferences_put_int(RADIO_SELECTOR_KEY, option);
}
/*
 * Displays a radio-style menu (only one option can be selected).
 * User can choose between "Single" or "Timed".
*/
static void hello_show_radio_selector(){
  general_radio_selection_menu_t settings = {0};
  settings.banner         = "Hello Type";                                // Menu title
  settings.options        = radio_options;                               // Options to choose from
  settings.options_count  = sizeof(radio_options) / sizeof(char*);
  settings.select_cb      = hello_radio_handler;                         // Function to call when user selects
  settings.style          = RADIO_SELECTION_OLD_STYLE;                   // Visual style of the menu (RADIO_SELECTION_NEW_STYLE / RADIO_SELECTION_OLD_STYLE)
  settings.exit_cb        = hello_main;                                  // Return to main menu on exit
  settings.current_option = preferences_get_int(RADIO_SELECTOR_KEY, 0);  // Load saved option or use default
  general_radio_selection(settings);  // Show the radio menu
}
/* This is action function when the button and event is actionated */
static void custom_keyboard_event(char *event_name){
  general_notification_ctx_t   notification = {0};                      // Create and initialize the notification
  notification.head            = "Event";                               // Title of the notification
  notification.body            = event_name;
  notification.on_exit         = set_custom_keyboard_event;             // Go back to the main menu when exiting
  general_notification_handler(notification);  // Show the notification
}

/* This handler is used to do a custom event for our aplication */
static void custom_keyboard_handler(uint8_t button_index, uint8_t button_event){
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

  So we just want a event of type BUTTON_PRESS_DOWN, BUTTON_DOUBLE_CLICK, BUTTON_LONG_PRESS_HOLD and BUTTON_LONG_PRESS_UP

  */
  switch (button_index){
    /* We will use this event to get back to the main menu of our app */
    case BUTTON_LEFT:
      if (button_event == BUTTON_PRESS_DOWN){
        hello_main();
      }
      break;
    /* We will use this event to wait for a BUTTON_LONG_PRESS_UP to show a event */
    case BUTTON_UP:
      if (button_event == BUTTON_LONG_PRESS_UP){
        custom_keyboard_event("BUTTON LONG PRESS UP");
      }
      break;
    /* We will use this event to wait for a BUTTON_DOUBLE_CLICK to show a event */
    case BUTTON_RIGHT:
      if (button_event == BUTTON_DOUBLE_CLICK){
        custom_keyboard_event("BUTTON DOUBLE CLICK");
      }
      break;
    /* We will use this event to wait for a BUTTON_LONG_PRESS_HOLD to show a event */
    case BUTTON_DOWN:
      if (button_event == BUTTON_LONG_PRESS_HOLD){
        custom_keyboard_event("BUTTON LONG PRESS HOLD");
      }
      break;
    default:
      break;
  }
}

/* This function will setup the handlers to use our custom events */
static void set_custom_keyboard_event(){
  // This function will clear all the oled screen;
  oled_screen_clear();
  /* We display the title of the screen at row 0 and inverted display */
  oled_screen_display_text_center("Custom Events", 0, OLED_DISPLAY_INVERT);
  /* We display a splited text, this allow us to set large text but limited due to this not handle a scrolling text.
  We need to pass the text, a pointer to the a variable where we store or counter for each row and the style of display */
  int row_index = 2;
  oled_screen_display_text_splited("Press the buttoms to use your custom event keyboard", &row_index, OLED_DISPLAY_NORMAL);
  menus_module_set_app_state(true, custom_keyboard_handler);
}

/* Main Handler: This function will handle the callback when a item is pressed*/
static void hello_main_handler(uint8_t option){
  last_main_selection = option;
  switch (option) {
  case SAY_HELLO:
    if(preferences_get_int(RADIO_SELECTOR_KEY, 0) == 0){
      /* Show the notification
      with this function the modal will stay until the back button is pressed */
      hello_show_notify();
    }else{
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
void hello_main(){
  general_submenu_menu_t main = {0};
  main.options         = main_menu_options;                          // Set the list of menu options
  main.options_count   = sizeof(main_menu_options) / sizeof(char*);  // Count how many options there are
  main.select_cb       = hello_main_handler;                         // Function to call when an option is selected
  main.selected_option = last_main_selection;                        // Restore last selected item
  // Function to call when exiting the menu. menus_module_restart is our default function from menus_module.h file
  main.exit_cb         = menus_module_restart;                       
  general_submenu(main);                                    // Show the menu on screen
}
```

## `hello_module.h`
```c
#ifndef HELLO_MODULE_H
#define HELLO_MODULE_H

void hello_main();

#endif
```


---

# Full code for `hello_cmd`
## `hello_cmd.c`
```c
// ESP libaries
#include <string.h>
#include "argtable3/argtable3.h"
#include "esp_console.h"
#include "esp_log.h"

// Header for this file
#include "hello_cmd.h"
// Custom actions with screen
// Get the general bitmaps with the minino letters
#include "bitmaps_general.h"
// Get the oled library
#include "oled_screen.h"
// Get the screen saver to deactivate
#include "screen_saver.h"

// Struct for the arguments of the command
static struct {
  struct arg_str* name;
  struct arg_end* end;
} hello_cmd_args;


/* This functions allows handle the command arguments */
static int hello_cmd_handler(int argc, char** argv){
  /* Check for error in the command params*/
  int nerrros = arg_parse(argc, argv, (void**) &hello_cmd_args);
  if (nerrros != 0) {
    arg_print_errors(stderr, hello_cmd_args.end, "HELLO CMD");
    return 1;
  }

  /* Stop the screen saver. We do this manually in this example,
  but if you use a function from generals you dont need to do manually,
  this is only when you want complete control of what you are doing*/
  screen_saver_stop();

  // Clear the oled
  oled_screen_clear();
  /* Show the minino letters in the center of the screen, the bitmap struct of general are:
  const epd_bitmap_t minino_letters_bitmap = {
    .idx = MININO_LETTERS, // The index of the element
    .name = "Letters", // The name to show in the menus
    .bitmap = epd_bitmap_minino_text_logo, // The bitmap array
    .width = 64, // The width of the bitmap
    .height = 32, // Height of the bitmap
  };
  */
  oled_screen_display_bitmap(minino_letters_bitmap.bitmap, 32, 16, minino_letters_bitmap.width, minino_letters_bitmap.height, OLED_DISPLAY_NORMAL);
  // Show the user input in the center of the screen at position 4
  oled_screen_display_text_center("Of:", 5, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center(hello_cmd_args.name->sval[0], 6, OLED_DISPLAY_INVERT);

  printf("\nMeow %s! Says Minino\n", hello_cmd_args.name->sval[0]);
  
  return 0;
}

/* This function is used to register the commands in the main command register in the file:
 modules/cat_dos/cat_console
*/
void hello_cmd_register(){
  /* Setup the arg name, with this we declare the "n" and "name" as a option  that we can use as:
  hello_cmd name=MyName ; hello_cmd n=MyName
  With tis params the user is obligated to declare the n or name as part of the argument, if you want just
  to put the command and the arg, for example for single argument, set to NULL the shortopts and longopts
  like: arg_str0(NULL, NULL, "Str", "Your name");
  */
  hello_cmd_args.name = arg_str0("n", "name", "Str", "Your name");
  /* Just set the amount of argument we accept*/
  hello_cmd_args.end = arg_end(1);

  // We init the console command
  esp_console_cmd_t hello_cmd_setup = {
    .command = "hello_cmd", // This is how we call it
    .help = "Im a simple command hello", // Help to the user to know about what the command does
    .category = "CMD", // If the app module have an category, this used to group commands
    .hint = NULL, // Some help to the user, this requires interactive console
    .func = &hello_cmd_handler, // Pointer to the callback function that handle our values
    .argtable = &hello_cmd_args, // Pointer to our struct of argument
  };

  /* ESP_ERROR_CHECK function check if the return value of a function it is different of ESP_OK
  then show and error an reboot. If everything it is ok, then cointinue the normal process
  With esp_console_cmd_register we register our console cmd to use*/
  ESP_ERROR_CHECK(esp_console_cmd_register(&hello_cmd_setup));
}
```

## `hello_cmd.h`
```c
#ifndef HELLO_CMD_H
#define HELLO_CMD_H

void hello_cmd_register();

#endif
```

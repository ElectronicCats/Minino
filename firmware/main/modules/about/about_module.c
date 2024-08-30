#include "about_module.h"
#include <stdint.h>
#include <string.h>
#include "animations_task.h"
#include "freertos/FreeRTOS.h"
#include "led_events.h"
#include "menus_module.h"
#include "oled_screen.h"

static char* about_credits_text[] = {
    "Developed by",
    "Electronic Cats",
    "and PWnLabs",
    "",
    "With love from",
    "Mexico...",
    "",
    "Thanks",
    "- Kevin",
    "  @kevlem97",
    "- Roberto",
    "- Francisco",
    "  @deimoshall",
    "and Electronic",
    "Cats team",
};

static char* about_legal_text[] = {
    "The user",        "assumes all",     "responsibility",   "for the use of",
    "MININO and",      "agrees to use",   "it legally and",   "ethically,",
    "avoiding any",    "activities that", "may cause harm,",  "interference,",
    "or unauthorized", "access to",       "systems or data.",
};

static const general_menu_t about_credits_menu = {
    .menu_items = about_credits_text,
    .menu_count = 14,
    .menu_level = GENERAL_TREE_APP_INFORMATION,
};

static const general_menu_t about_legal_menu = {
    .menu_items = about_legal_text,
    .menu_count = 15,
    .menu_level = GENERAL_TREE_APP_INFORMATION,
};

void about_module_display_credits_menu() {
  general_register_scrolling_menu(&about_credits_menu);
  general_screen_display_scrolling_text_handler(menus_module_exit_app);
}

void about_module_display_legal_menu() {
  general_register_scrolling_menu(&about_legal_menu);
  general_screen_display_scrolling_text_handler(menus_module_exit_app);
}

void about_module_display_version() {
  general_screen_display_card_information_handler(
      "Minino", "v" CONFIG_PROJECT_VERSION, menus_module_exit_app, NULL);
}
void about_module_display_license() {
  general_screen_display_card_information_handler("License", "GNU GPL 3.0",
                                                  menus_module_exit_app, NULL);
}

#include <stdint.h>
#include <string.h>
#ifndef GENERAL_SCREENS_H
  #define GENERAL_SCREENS_H

  #if CONFIG_RESOLUTION_128X64
    #define ITEMS_PER_SCREEN 5
  #elif CONFIG_RESOLUTION_128X32
    #define ITEMS_PER_SCREEN 3
  #endif

typedef enum {
  GENERAL_MENU_MAIN = 0,
  GENERAL_MENU_SUBMENU
} general_menu_tree_index_t;

typedef enum {
  GENERAL_TREE_APP_MENU,
  GENERAL_TREE_APP_SUBMENU,
  GENERAL_TREE_APP_INFORMATION
} menu_tree_t;

typedef struct {
  char** menu_items;
  uint16_t menu_count;
  menu_tree_t menu_level;
} general_menu_t;

void general_register_menu(const general_menu_t* ctx);
void general_register_scrolling_menu(const general_menu_t* ctx);
void general_clear_screen();
void general_screen_display_menu(uint16_t current_option);
void genera_screen_display_card_information(char* title, char* body);
void genera_screen_display_notify_information(char* title, char* body);
void general_screen_display_card_information_handler(char* title,
                                                     char* body,
                                                     void* callback_exit,
                                                     void* callback_restore);
void general_screen_display_scrolling_text_handler(void* callback_exit);
char** general_screen_truncate_text(char* p_text, int* num_lines);
void general_screen_display_auto_card(char* lines,
                                      int num_lines,
                                      void* callback_exit);
#endif  // GENERAL_SCREENS_H

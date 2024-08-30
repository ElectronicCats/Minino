#include <stdbool.h>
#include <stdint.h>
#include "general/general_screens.h"
#ifndef TRACKERS_SCREENS_H
  #define TRACKERS_SCREENS_H

enum {
  TRACKERS_SCAN,
  TRACKERS_LIST,
  TRACKERS_COUNT
} trackers_menu_item = TRACKERS_SCAN;
char* trackers_menu_items[TRACKERS_COUNT] = {"SCAN", "LIST"};

void clear_screen();
void module_register_menu(menu_tree_t menu);
void module_display_menu(uint16_t current_item);

void module_display_scanning();
void module_display_device_detected(char* device_name);
void module_display_tracker_information(char* title, char* body);
void module_update_scan_state(bool scanning);
void module_add_tracker_to_list(char* tracker_name);
void module_update_tracker_name(char* tracker_name, uint16_t index);
#endif  // TRACKERS_SCREENS_H

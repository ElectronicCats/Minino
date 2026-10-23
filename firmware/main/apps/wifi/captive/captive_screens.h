#ifndef CAPTIVE_SCREENS_H
#define CAPTIVE_SCREENS_H

#include "general_notification.h"
#include "general_radio_selection.h"
#include "general_scrolling_text.h"
#include "general_submenu.h"
#include "menus_module.h"
#include "preferences.h"
#include "wifi_scanner.h"

extern const char* captive_main_menu[7];
extern const char* no_folder_help[31];

void captive_module_show_help();
void captive_module_show_notification_no_ap_records();

#endif
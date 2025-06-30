#ifndef CAPTIVE_SCREENS_H
#define CAPTIVE_SCREENS_H

#include "general_notification.h"
#include "general_radio_selection.h"
#include "general_scrolling_text.h"
#include "general_submenu.h"
#include "menus_module.h"
#include "preferences.h"
#include "wifi_scanner.h"

const char* captive_main_menu[] = {"Portals", "Mode", "Preferences",
                                   "Channel", "Run",  "Help"};
const char* no_folder_help[] = {"If you dont have",
                                "the Portals",
                                "folder already",
                                "in your memory",
                                "create",
                                "a folder.",
                                "called:",
                                "",
                                "portals",
                                "",
                                "in the root",
                                "path of the",
                                "SD card and",
                                "save the",
                                "HTML files",
                                "inside",
                                "",
                                "use the user",
                                "inputs to ",
                                "handle the",
                                "data",
                                "",
                                "configure the",
                                "preference",
                                "dump mode",
                                "",
                                "If you",
                                "have problems",
                                "with the AP",
                                "change the",
                                "channel."};

void captive_module_show_help();
void captive_module_show_notification_no_ap_records();

#endif
#include "captive_screens.h"

void captive_module_show_help() {
  general_scrolling_text_ctx help = {0};
  help.banner = "Help";
  help.text_arr = no_folder_help;
  help.text_len = sizeof(no_folder_help) / sizeof(char*);
  help.window_type = GENERAL_SCROLLING_TEXT_WINDOW;
  help.scroll_type = GENERAL_SCROLLING_TEXT_CLAMPED;
  help.exit_cb = captive_module_main;
  general_scrolling_text_array(help);
}

void captive_module_show_notification_no_ap_records() {
  general_notification_ctx_t notification = {0};
  notification.duration_ms = 2000;
  notification.head = "Captive Portal";
  notification.body = "No record found, run again.";
  general_notification(notification);
  captive_module_main();
}
#ifndef DISPLAY_H
#define DISPLAY_H

#include "bluetooth_scanner.h"
#include "buzzer.h"
#include "cmd_sniffer.h"
#include "display_helper.h"
#include "sh1106.h"
#include "wifi_sniffer.h"

#define INVERT    1
#define NO_INVERT 0

extern uint8_t selected_item;
extern Layer previous_layer;
extern Layer current_layer;
extern int num_items;

void display_init(void);
void display_clear(void);
void display_show(void);
void display_text(const char* text, int x, int page, int invert);
void display_clear_line(int x, int page, int invert);
void display_selected_item_box();
char** add_empty_strings(char** array, int length);
char** remove_srolling_text_flag(char** items, int length);
char** get_menu_items();
void display_menu_items(char** items);
void display_scrolling_text(char** text);
void display_menu();
// void display_wifi_sniffer(wifi_sniffer_record_t record);
void display_wifi_sniffer_animation_task(void* pvParameter);
void display_wifi_sniffer_animation_start();
void display_wifi_sniffer_animation_stop();
void display_wifi_sniffer_cb(sniffer_runtime_t* sniffer);
void display_bluetooth_scanner(bluetooth_scanner_record_t record);
void display_thread_cli();
void display_in_development_banner();
void display_gps_init();
void display_gps_deinit();
void display_zb_switch_toggle_pressed();
void display_zb_switch_toggle_released();

#endif  // DISPLAY_H

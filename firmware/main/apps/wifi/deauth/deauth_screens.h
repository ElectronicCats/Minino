#include <stdint.h>
#include "esp_wifi.h"
#ifndef DEAUTH_SCREENS_H
  #define DEAUTH_SCREENS_H

  #define BITMAPS_WIFI_LOADING_FRAME 4
  #define BITMAPS_MICHI_PUNCH_FRAME  2

enum { SCAN, SELECT, DEAUTH, RUN, MENUCOUNT };
enum { BROADCAST, ROGUE_AP, COMBINED, ATTACKSCOUNT };

extern char* deauth_menu[];
extern char* deauth_attacks[];
extern char* deauth_attacks_short[];
extern const unsigned char* wifi_loading[4];

typedef struct {
  uint16_t count;
  wifi_ap_record_t selected_ap;
  uint16_t attack;
} menu_stadistics_t;

void deauth_display_scanning_text();
void deauth_display_scanning();
void deauth_display_attaking_text();
void deauth_display_attacking_animation();
void deauth_display_menu(uint16_t current_item,
                         menu_stadistics_t menu_stadistics);
void deauth_display_attacks(uint16_t current_item,
                            menu_stadistics_t menu_stadistics);
void deauth_clear_screen();
void deauth_display_scanned_ap(wifi_ap_record_t* ap_records,
                               uint16_t scanned_records,
                               uint16_t current_option);

void deauth_display_warning_not_ap_selected();
void deauth_display_warning_not_attack_selected();
#endif  // DEAUTH_SCREENS_H

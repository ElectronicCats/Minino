#include "deauth_screens.h"
#include <stdint.h>
#include <string.h>
#include "animations_task.h"
#include "esp_wifi.h"
#include "general/bitmaps_general.h"
#include "general/general_screens.h"
#include "oled_screen.h"

#ifdef CONFIG_RESOLUTION_128X64
  #define ITEMOFFSET     2
  #define ITEMSPERSCREEN 4
#else  // CONFIG_RESOLUTION_128X32
  #define ITEMOFFSET     1
  #define ITEMSPERSCREEN 2
#endif

static int ap_count = 0;

void deauth_clear_screen() {
  oled_screen_clear();
}

static void deauth_display_selected_item(char* item_text, uint8_t item_number) {
  oled_screen_display_bitmap(minino_face, 0, (item_number * 8), 8, 8,
                             OLED_DISPLAY_NORMAL);
  oled_screen_display_text(item_text, 16, item_number, OLED_DISPLAY_INVERT);
}

void deauth_display_warning_not_ap_selected() {
  oled_screen_clear();
  oled_screen_display_text_center("NO AP SELECTED", 0, OLED_DISPLAY_INVERT);
  oled_screen_display_text_center("SELECT AN AP", 2, OLED_DISPLAY_NORMAL);
}

void deauth_display_warning_not_attack_selected() {
  oled_screen_clear();
  oled_screen_display_text_center("NO ATTACK SELECTED", 0, OLED_DISPLAY_INVERT);
  oled_screen_display_text_center("SELECT AN ATTACK", 2, OLED_DISPLAY_NORMAL);
}

void deauth_display_scanning_text() {
  oled_screen_clear();
  oled_screen_display_text_center("SCANNING AP", 0, OLED_DISPLAY_NORMAL);
}

void deauth_display_scanning() {
  oled_screen_display_text_center("SCANNING AP", 0, OLED_DISPLAY_NORMAL);
  static uint8_t idx = 0;
  oled_screen_display_bitmap(wifi_loading[idx], 48, 16, 32, 32,
                             OLED_DISPLAY_NORMAL);
  idx = ++idx > (BITMAPS_WIFI_LOADING_FRAME - 1) ? 0 : idx;
}

void deauth_display_attaking_text() {
  oled_screen_clear();
  oled_screen_display_text_center("ATTACKING AP", 0, OLED_DISPLAY_NORMAL);
}

void deauth_display_attacking_animation() {
  static uint8_t idx = 0;
  oled_screen_display_bitmap(punch_animation[idx], 48, 16, 32, 32,
                             OLED_DISPLAY_NORMAL);
  idx = ++idx > (BITMAPS_MICHI_PUNCH_FRAME - 1) ? 0 : idx;
}

void deauth_display_menu(uint16_t current_item,
                         menu_stadistics_t menu_stadistics) {
  oled_screen_clear_buffer();
  oled_screen_display_text("< Exit", 0, 0, OLED_DISPLAY_NORMAL);

  int position = 1;
  uint16_t start_item = (current_item / ITEMSPERSCREEN) * ITEMSPERSCREEN;

  for (uint16_t i = start_item;
       i < start_item + ITEMSPERSCREEN && i < MENUCOUNT; i++) {
    if (deauth_menu[i] == NULL) {
      break;
    }
    char item[18];
    if (i == SCAN) {
      snprintf(item, 18, "%s......[%d]", deauth_menu[i], menu_stadistics.count);
    } else if (i == SELECT) {
      if (menu_stadistics.selected_ap.bssid[0] != 0) {
        snprintf(item, 18, "%s.....[OK]", deauth_menu[i]);
      } else {
        snprintf(item, 18, "%s...[NOT]", deauth_menu[i]);
      }
    } else if (i == DEAUTH) {
      if (menu_stadistics.attack == 99) {
        snprintf(item, 18, "%s...[NOT]", deauth_menu[i]);
      } else {
        snprintf(item, 18, "%s....[%s]", deauth_menu[i],
                 deauth_attacks_short[menu_stadistics.attack]);
      }
    } else {
      snprintf(item, 18, "%s", deauth_menu[i]);
    }

    if (i == current_item) {
      deauth_display_selected_item(item, position);
    } else {
      oled_screen_display_text(item, 0, position, OLED_DISPLAY_NORMAL);
    }
    position = position + ITEMOFFSET;
  }
  oled_screen_display_show();
}

void deauth_display_scanned_ap(wifi_ap_record_t* ap_records,
                               uint16_t scanned_records,
                               uint16_t current_option) {
  oled_screen_clear_buffer();
  oled_screen_display_text("< Back", 0, 0, OLED_DISPLAY_NORMAL);

  ap_count = scanned_records;

  for (uint16_t i = current_option; i < (scanned_records + current_option);
       i++) {
    if (i >= scanned_records) {
      break;
    }
    char ssid[MAX_LINE_CHAR];
    general_screen_truncate_text((char*) ap_records[i].ssid, ssid);
    if (i == current_option) {
      char* prefix = "> ";

      char item_text[strlen(prefix) + strlen((char*) ssid) + 1];
      strcpy(item_text, prefix);
      strcat(item_text, (char*) ssid);
      oled_screen_display_text(item_text, 0, (i + 1) - current_option,
                               OLED_DISPLAY_INVERT);
    } else {
      oled_screen_display_text((char*) ssid, 0, (i + 1) - current_option,
                               OLED_DISPLAY_NORMAL);
    }
  }
  oled_screen_display_show();
}

void deauth_display_attacks(uint16_t current_item,
                            menu_stadistics_t menu_stadistics) {
  oled_screen_clear_buffer();
  oled_screen_display_text("< Back", 0, 0, OLED_DISPLAY_NORMAL);

  int position = 1;
  uint16_t start_item = (current_item / ITEMSPERSCREEN) * ITEMSPERSCREEN;

  for (uint16_t i = start_item;
       i < start_item + ITEMSPERSCREEN && i < MENUCOUNT; i++) {
    if (deauth_attacks[i] == NULL) {
      break;
    }
    char item[18];
    if (i == menu_stadistics.attack) {
      snprintf(item, 18, "%s..[SEL]", deauth_attacks[i]);
    } else {
      snprintf(item, 18, "%s", deauth_attacks[i]);
    }
    if (i == current_item) {
      deauth_display_selected_item(item, position);
    } else {
      oled_screen_display_text(item, 0, position, OLED_DISPLAY_NORMAL);
    }
    position = position + ITEMOFFSET;
  }
  oled_screen_display_show();
}

void deauth_display_captive_portals(uint16_t current_item,
                                    menu_stadistics_t menu_stadistics) {
  oled_screen_clear_buffer();
  oled_screen_display_text("< Back", 0, 0, OLED_DISPLAY_NORMAL);

  for (uint16_t i = 0; i < CAPTIVEPORTALCOUNT; i++) {
    if (deauth_attacks_captive_portal[i] == NULL) {
      break;
    }
    char item[18];
    if (i == menu_stadistics.attack) {
      snprintf(item, 18, "%s..[SELECTED]", deauth_attacks_captive_portal[i]);
    } else {
      snprintf(item, 18, "%s", deauth_attacks_captive_portal[i]);
    }
    if (i == current_item) {
      deauth_display_selected_item(item, i + ITEMOFFSET);
    } else {
      oled_screen_display_text(item, 0, i + ITEMOFFSET, OLED_DISPLAY_NORMAL);
    }
  }
  oled_screen_display_show();
}

void deauth_display_captive_waiting() {
  oled_screen_clear();
  oled_screen_display_text_center("WAITING", 0, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("FOR USER", 1, OLED_DISPLAY_NORMAL);
}

void deauth_display_captive_portal_creds(char* ssid, char* user, char* pass) {
  oled_screen_clear();
#ifdef CONFIG_RESOLUTION_128X64
  oled_screen_display_text_center("Captive Portal", 0, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("SSID", 1, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center(ssid, 2, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("USER", 3, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center(user, 4, OLED_DISPLAY_INVERT);
  if (strcmp(pass, "") != 0) {
    oled_screen_display_text_center("PASS", 5, OLED_DISPLAY_NORMAL);
    oled_screen_display_text_center(pass, 6, OLED_DISPLAY_INVERT);
  }
#else  // CONFIG_RESOLUTION_128X32
  oled_screen_display_text_center(ssid, 0, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center(user, 1, OLED_DISPLAY_INVERT);
  if (strcmp(pass, "") != 0) {
    oled_screen_display_text_center(pass, 2, OLED_DISPLAY_INVERT);
  }
#endif
}
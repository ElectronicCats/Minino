// SPDX-License-Identifier: GPL-3.0-or-later
#include "surv_sim_screens.h"
#include <stdio.h>
#include <string.h>
#include "oled_screen.h"

static uint8_t s_spin_idx = 0;
static const char SPINNER[] = {'|', '/', '-', '\\'};

void surv_sim_screens_show(const char* mode_name,
                           const char* target_name,
                           uint32_t packet_count,
                           uint8_t channel,
                           bool is_ble) {
  char buf[32];
  oled_screen_clear_buffer();

  uint8_t pages = oled_screen_get_pages();
  char spin = SPINNER[(s_spin_idx++) % 4];

  if (pages >= 8) {
    snprintf(buf, sizeof(buf), "SIMULATOR [TX] %c", spin);
    oled_screen_display_text(buf, 0, 0, true);

    snprintf(buf, sizeof(buf), "Mod: %-11s", mode_name ? mode_name : "ALL");
    oled_screen_display_text(buf, 0, 1, false);

    snprintf(buf, sizeof(buf), "Tgt: %-11s",
             target_name ? target_name : "Flock T4");
    oled_screen_display_text(buf, 0, 2, false);

    if (is_ble) {
      snprintf(buf, sizeof(buf), "Rad: BLE Adv (2.4G)");
    } else {
      snprintf(buf, sizeof(buf), "Rad: WiFi Ch %-2d", channel);
    }
    oled_screen_display_text(buf, 0, 3, false);

    snprintf(buf, sizeof(buf), "Tx:  %-6lu pkts", (unsigned long) packet_count);
    oled_screen_display_text(buf, 0, 4, false);

    oled_screen_display_text("Emision continua", 0, 5, false);

    oled_screen_display_text("< Salir", 0, 7, true);
  } else {
    snprintf(buf, sizeof(buf), "SIM [TX] %-4s %c",
             mode_name ? mode_name : "ALL", spin);
    oled_screen_display_text(buf, 0, 0, true);

    snprintf(buf, sizeof(buf), "Tgt:%-12s",
             target_name ? target_name : "Flock T4");
    oled_screen_display_text(buf, 0, 1, false);

    snprintf(buf, sizeof(buf), "Tx:%-5lu pkts", (unsigned long) packet_count);
    oled_screen_display_text(buf, 0, 2, false);

    oled_screen_display_text("< Salir", 0, 3, true);
  }

  oled_screen_display_show();
}

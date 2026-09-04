// SPDX-License-Identifier: GPL-3.0-or-later
#include "surveillance_screens.h"
#include <stdio.h>
#include <string.h>
#include "oled_screen.h"
#include "surv_engine.h"

static uint8_t s_anim_idx = 0;
static const char SPINNER[] = {'|', '/', '-', '\\'};

static const char* rssi_to_bars(int8_t rssi) {
  if (rssi == 0) {
    return "[.....]";
  }
  if (rssi >= -55) {
    return "[|||||]";
  }
  if (rssi >= -65) {
    return "[||||.]";
  }
  if (rssi >= -75) {
    return "[|||..]";
  }
  if (rssi >= -85) {
    return "[||...]";
  }
  return "[|....]";
}

static const char* tier_to_detail(uint8_t tier) {
  switch (tier) {
    case 4:
      return "T4 (F-Print)";
    case 3:
      return "T3 (Probe)";
    case 2:
      return "T2 (OUI/MAC)";
    case 1:
      return "T1 (MFR/Echo)";
    default:
      return "T0 (SSID Kw)";
  }
}

void surveillance_screens_show_status(uint8_t score,
                                      const char* profile,
                                      const char* last_label,
                                      uint8_t last_tier,
                                      int8_t rssi,
                                      uint8_t channel,
                                      uint32_t overflows) {
  char buf[32];
  oled_screen_clear_buffer();

  uint8_t pages = oled_screen_get_pages();
  char spin = SPINNER[(s_anim_idx++) % 4];
  const char* prof = profile ? profile : "ALL";

  const char* lvl_badge = "CLEAR";
  bool alert_invert = false;
  if (score >= SURV_SCORE_ALERT) {
    lvl_badge = "!ALERT!";
    alert_invert = true;
  } else if (score >= SURV_SCORE_CAUTION) {
    lvl_badge = "CAUTION";
  }

  if (pages >= 8) {
    // --- Layout 128x64 (8 paginas) ---
    // Pagina 0: Encabezado elegante en video inverso
    snprintf(buf, sizeof(buf), "SURV [%-4s]   %c", prof, spin);
    oled_screen_display_text(buf, 0, 0, true);

    // Pagina 1: Nivel de amenaza y score
    snprintf(buf, sizeof(buf), "[%-7s]  %2dpts", lvl_badge, score);
    oled_screen_display_text(buf, 0, 1, alert_invert);

    // Pagina 2: Dispositivo objetivo
    if (last_label != NULL && last_label[0] != '\0') {
      snprintf(buf, sizeof(buf), "Tgt:%-12s", last_label);
    } else {
      snprintf(buf, sizeof(buf), "Tgt:(Searching)");
    }
    oled_screen_display_text(buf, 0, 2, false);

    // Pagina 3: Nivel de certeza / Tier
    if (last_label != NULL && last_label[0] != '\0') {
      snprintf(buf, sizeof(buf), "Trs:%-12s", tier_to_detail(last_tier));
    } else {
      snprintf(buf, sizeof(buf), "Trs:No threats");
    }
    oled_screen_display_text(buf, 0, 3, false);

    // Pagina 4: Intensidad de senal con barra grafica
    if (last_label != NULL && last_label[0] != '\0') {
      snprintf(buf, sizeof(buf), "%4ddBm %s", rssi, rssi_to_bars(rssi));
    } else {
      snprintf(buf, sizeof(buf), "Sig: Monitoring");
    }
    oled_screen_display_text(buf, 0, 4, false);

    // Pagina 5: Radio y canal
    if (channel > 0) {
      snprintf(buf, sizeof(buf), "Rad: WiFi Ch%-2d", channel);
    } else {
      snprintf(buf, sizeof(buf), "Rad: BLE 2.4GHz");
    }
    oled_screen_display_text(buf, 0, 5, false);

    // Pagina 6: Metricas / Buffer
    if (overflows > 0) {
      snprintf(buf, sizeof(buf), "Queue ovf: %lu", (unsigned long) overflows);
      oled_screen_display_text(buf, 0, 6, false);
    }

    // Pagina 7: Barra inferior con guia de botones
    oled_screen_display_text("^Hlp vMod <Exit", 0, 7, true);
  } else {
    // --- Layout 128x32 (4 paginas) ---
    snprintf(buf, sizeof(buf), "SURV [%-4s] %2dpt", prof, score);
    oled_screen_display_text(buf, 0, 0, true);

    snprintf(buf, sizeof(buf), "Lvl:%-7s %c", lvl_badge, spin);
    oled_screen_display_text(buf, 0, 1, alert_invert);

    if (last_label != NULL && last_label[0] != '\0') {
      snprintf(buf, sizeof(buf), "Tgt:%-8s T%d", last_label, last_tier);
    } else {
      snprintf(buf, sizeof(buf), "Tgt: Scanning");
    }
    oled_screen_display_text(buf, 0, 2, false);

    if (channel > 0) {
      snprintf(buf, sizeof(buf), "%ddBm Ch%-2d ^Hlp", rssi, channel);
    } else {
      snprintf(buf, sizeof(buf), "%ddBm BLE  ^Hlp", rssi);
    }
    oled_screen_display_text(buf, 0, 3, false);
  }

  oled_screen_display_show();
}

static const char* const HELP_LINES[] = {
    "= SURVEILLANCE =",
    "Passive cam &",
    "tracker detect.",
    "",
    "Flock/ALPR works",
    "in US & Canada.",
    "Score 0 CLEAR is",
    "normal elsewhere",
    "",
    "SD signatures:",
    "surveil/",
    "signatures.csv",
    "",
    "Controls:",
    "^:Help   v:Mode",
    ">:Scan   <:Exit",
};

void surveillance_screens_show_help(uint8_t page) {
  oled_screen_clear();
  uint8_t rows = oled_screen_get_pages();
  uint8_t start = page * rows;
  for (uint8_t i = 0; i < rows && (start + i) < 16; i++) {
    bool inv = (start + i == 0);
    oled_screen_display_text((char*) HELP_LINES[start + i], 0, i, inv);
  }
  oled_screen_display_show();
}

void surveillance_screens_show_radio_busy(void) {
  oled_screen_clear();
  oled_screen_display_text("! RADIO BUSY !", 0, 1, true);
  oled_screen_display_text("802.15.4 active", 0, 3, false);
  oled_screen_display_text("Restart Minino", 0, 5, false);
  oled_screen_display_show();
}

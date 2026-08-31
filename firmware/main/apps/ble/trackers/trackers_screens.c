// SPDX-License-Identifier: GPL-3.0-or-later
#include "trackers_screens.h"
#include <stdio.h>
#include <string.h>
#include "oled_screen.h"

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

static const char* distance_label(float d) {
  if (d < 0) {
    return "---";
  }
  if (d < 1.0f) {
    return "<1m";
  }
  if (d < 5.0f) {
    return "Cerca";
  }
  if (d < 20.0f) {
    return "Medio";
  }
  return "Lejos";
}

void trackers_screens_show_status(uint8_t tracker_count,
                                  const char* last_name,
                                  const char* last_vendor,
                                  int8_t rssi,
                                  float distance,
                                  bool gps_valid,
                                  double gps_lat,
                                  double gps_lon,
                                  bool scanning) {
  char buf[32];
  oled_screen_clear_buffer();

  uint8_t pages = oled_screen_get_pages();
  char spin = SPINNER[(s_anim_idx++) % 4];

  if (pages >= 8) {
    snprintf(buf, sizeof(buf), "TRK   [%2d]    %c", tracker_count, spin);
    oled_screen_display_text(buf, 0, 0, true);

    if (scanning) {
      snprintf(buf, sizeof(buf), "[SCAN ] Trackers");
    } else {
      snprintf(buf, sizeof(buf), "[STOP ] Trackers");
    }
    oled_screen_display_text(buf, 0, 1, scanning);

    if (last_name != NULL && last_name[0] != '\0') {
      snprintf(buf, sizeof(buf), "Dev:%-11s", last_name);
    } else {
      snprintf(buf, sizeof(buf), "Dev:(Buscando...)");
    }
    oled_screen_display_text(buf, 0, 2, false);

    if (last_vendor != NULL && last_vendor[0] != '\0') {
      snprintf(buf, sizeof(buf), "Mfg:%-11s", last_vendor);
    } else {
      snprintf(buf, sizeof(buf), "Mfg:---");
    }
    oled_screen_display_text(buf, 0, 3, false);

    if (rssi != 0) {
      snprintf(buf, sizeof(buf), "%4ddBm %s", rssi, rssi_to_bars(rssi));
    } else {
      snprintf(buf, sizeof(buf), "Sig: Monitoreo");
    }
    oled_screen_display_text(buf, 0, 4, false);

    if (distance >= 0) {
      snprintf(buf, sizeof(buf), "Dst:%5.1fm %s", distance,
               distance_label(distance));
    } else {
      snprintf(buf, sizeof(buf), "Dst: Sin datos");
    }
    oled_screen_display_text(buf, 0, 5, false);

    if (gps_valid) {
      snprintf(buf, sizeof(buf), "GPS:%.4f", gps_lat);
      oled_screen_display_text(buf, 0, 6, false);
      snprintf(buf, sizeof(buf), "    %.4f", gps_lon);
      oled_screen_display_text(buf, 0, 7, false);
    } else {
      snprintf(buf, sizeof(buf), "GPS: Sin fix");
      oled_screen_display_text(buf, 0, 6, false);
      oled_screen_display_text("^Help vMode", 0, 7, true);
    }
  } else {
    snprintf(buf, sizeof(buf), "TRK [%2d] %c", tracker_count, spin);
    oled_screen_display_text(buf, 0, 0, true);

    if (scanning) {
      snprintf(buf, sizeof(buf), "[SCAN] %s", last_name ? last_name : "...");
    } else {
      snprintf(buf, sizeof(buf), "[STOP] %s", last_name ? last_name : "...");
    }
    oled_screen_display_text(buf, 0, 1, scanning);

    if (rssi != 0) {
      snprintf(buf, sizeof(buf), "%4ddBm %s", rssi, rssi_to_bars(rssi));
    } else {
      snprintf(buf, sizeof(buf), "Sig: Monitoreo");
    }
    oled_screen_display_text(buf, 0, 2, false);

    snprintf(buf, sizeof(buf), "%.1fm ^Help", distance);
    oled_screen_display_text(buf, 0, 3, false);
  }

  oled_screen_display_show();
}

static const char* const HELP_LINES[] = {
    "=== TRACKERS ===",
    "Detecta AirTag,",
    "SmartTag, Tile",
    "y otros rastreadores",
    "BLE (Bluetooth).",
    "",
    "Muestra distancia",
    "estimada por RSSI.",
    "",
    "Si GPS activo:",
    "captura coordenadas",
    "de cada tracker.",
    "",
    "Controles:",
    "^:Ayuda  v:Scan",
    "<:Salir",
};

void trackers_screens_show_help(uint8_t page) {
  oled_screen_clear();
  uint8_t rows = oled_screen_get_pages();
  uint8_t start = page * rows;
  for (uint8_t i = 0; i < rows && (start + i) < 16; i++) {
    bool inv = (start + i == 0);
    oled_screen_display_text((char*) HELP_LINES[start + i], 0, i, inv);
  }
  oled_screen_display_show();
}

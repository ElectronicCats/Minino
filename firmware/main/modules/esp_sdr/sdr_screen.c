/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Screen rendering for Minino Native SDR on SSD1306/SH1106 OLED (128x64).
 */

#include "sdr_screen.h"
#include <stdio.h>
#include <string.h>
#include "oled_screen.h"

void sdr_screen_render(const sdr_screen_data_t* data) {
  if (data == NULL) {
    return;
  }

  oled_screen_clear_buffer();

  /* Header (Page 0) */
  char header_buf[32];
  if (data->mode == SDR_MODE_TX_PKT) {
    snprintf(header_buf, sizeof(header_buf), "MININO SDR [TX:PKT]");
  } else if (data->mode == SDR_MODE_TX_CW) {
    snprintf(header_buf, sizeof(header_buf), "MININO SDR [TX:CW]%s",
             data->tx_active ? "*" : "");
  } else if (data->mode == SDR_MODE_WEB_HOST) {
    snprintf(header_buf, sizeof(header_buf), "MININO SDR [HOST]");
  } else {
    snprintf(header_buf, sizeof(header_buf), "MININO SDR %s",
             data->streaming ? "[RX]" : "[IDL]");
  }
  oled_screen_display_text(header_buf, 0, 0, OLED_DISPLAY_INVERT);

  /* Frequency Line (Page 2) with Channel number */
  if (data->mode == SDR_MODE_WEB_HOST) {
    oled_screen_display_text("Web: espargos.net", 0, 2, OLED_DISPLAY_NORMAL);
  } else {
    uint8_t ch = 1;
    if (data->freq_mhz >= 2412 && data->freq_mhz <= 2472) {
      ch = (uint8_t) (((data->freq_mhz - 2407u) + 2u) / 5u);
    } else if (data->freq_mhz == 2484) {
      ch = 14;
    }
    char freq_buf[32];
    snprintf(freq_buf, sizeof(freq_buf), "FREQ: %lu MHz (C%d)",
             (unsigned long) data->freq_mhz, (int) ch);
    oled_screen_display_text(freq_buf, 0, 2, OLED_DISPLAY_NORMAL);
  }

  /* Mode / Rate Line (Page 3) */
  char rate_buf[32];
  if (data->mode == SDR_MODE_WEB_HOST) {
    snprintf(rate_buf, sizeof(rate_buf), "App: /espsdr/app");
  } else if (data->mode == SDR_MODE_TX_PKT) {
    snprintf(rate_buf, sizeof(rate_buf), "MODE: 802.11 Raw TX");
  } else if (data->mode == SDR_MODE_TX_CW) {
    snprintf(rate_buf, sizeof(rate_buf), "MODE: PHY CW Carrier");
  } else {
    snprintf(rate_buf, sizeof(rate_buf), "RATE: 80 MS/s IQ");
  }
  oled_screen_display_text(rate_buf, 0, 3, OLED_DISPLAY_NORMAL);

  /* Status Line (Page 4) */
  char burst_buf[32];
  if (data->mode == SDR_MODE_WEB_HOST) {
    snprintf(burst_buf, sizeof(burst_buf), "USB: %s",
             data->tx_active ? "STREAMING..." : "READY TO CONNECT");
  } else if (data->mode == SDR_MODE_TX_PKT) {
    snprintf(burst_buf, sizeof(burst_buf), "SENT: %-6lu PKTS",
             (unsigned long) data->tx_count);
  } else if (data->mode == SDR_MODE_TX_CW) {
    snprintf(burst_buf, sizeof(burst_buf), "EMIT: %s",
             data->tx_active ? "ON (ACTIVE)" : "OFF (STANDBY)");
  } else {
    if (data->rssi_est > -95) {
      snprintf(burst_buf, sizeof(burst_buf), "CAPS: %-4lu  %3ddB",
               (unsigned long) data->burst_count, data->rssi_est);
    } else {
      snprintf(burst_buf, sizeof(burst_buf), "CAPS: %-4lu   --dB",
               (unsigned long) data->burst_count);
    }
  }
  oled_screen_display_text(burst_buf, 0, 4, OLED_DISPLAY_NORMAL);

  /* Signal Level Meter (y = 40 to y = 47) */
  oled_screen_draw_rect(4, 40, 120, 8, OLED_DISPLAY_NORMAL);
  int meter_fill = 0;
  if (data->mode == SDR_MODE_WEB_HOST) {
    meter_fill = data->tx_active ? 116 : 0;
  } else if (data->mode == SDR_MODE_TX_CW) {
    meter_fill = data->tx_active ? 116 : 0;
  } else if (data->mode == SDR_MODE_TX_PKT) {
    meter_fill = data->tx_active ? 116 : 0;
  } else {
    if (data->rssi_est > -95) {
      meter_fill = (int) (data->rssi_est + 95) * 116 / 70;
      if (meter_fill > 116)
        meter_fill = 116;
      if (meter_fill < 0)
        meter_fill = 0;
    }
  }
  if (meter_fill > 0) {
    oled_screen_draw_box(6, 42, meter_fill, 4, OLED_DISPLAY_NORMAL);
  }

  /* Status Line (Page 6) */
  if (data->status_line[0] != '\0') {
    oled_screen_display_text((char*) data->status_line, 0, 6,
                             OLED_DISPLAY_NORMAL);
  }

  /* Footer (Page 7) */
  if (data->footer_line[0] != '\0') {
    oled_screen_display_text((char*) data->footer_line, 0, 7,
                             OLED_DISPLAY_NORMAL);
  } else {
    if (data->mode == SDR_MODE_WEB_HOST) {
      oled_screen_display_text(">:Run Hold>:Mod <:Exit", 0, 7,
                               OLED_DISPLAY_NORMAL);
    } else if (data->mode == SDR_MODE_TX_PKT) {
      oled_screen_display_text("^v:Ch >:Send Hold>:Mod", 0, 7,
                               OLED_DISPLAY_NORMAL);
    } else if (data->mode == SDR_MODE_TX_CW) {
      oled_screen_display_text("^v:Ch >:Emit Hold>:Mod", 0, 7,
                               OLED_DISPLAY_NORMAL);
    } else {
      oled_screen_display_text("^v:Ch >:Cap  Hold>:Mod", 0, 7,
                               OLED_DISPLAY_NORMAL);
    }
  }

  oled_screen_display_show();
}

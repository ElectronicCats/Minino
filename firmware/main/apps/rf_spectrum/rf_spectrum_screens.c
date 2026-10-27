/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Screen rendering for Minino RF Spectrum Analyzer on SSD1306/SH1106 OLED
 * (128x64).
 */

#include "rf_spectrum_screens.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "menu_gfx.h"
#include "oled_screen.h"

static int rssi_to_height(int8_t rssi, int max_h) {
  if (rssi <= RF_NOISE_FLOOR) {
    return 1;
  }
  if (rssi >= RF_MAX_SIGNAL) {
    return max_h;
  }
  int h =
      (int) (rssi - RF_NOISE_FLOOR) * max_h / (RF_MAX_SIGNAL - RF_NOISE_FLOOR);
  if (h < 1)
    h = 1;
  if (h > max_h)
    h = max_h;
  return h;
}

static void render_header(const rf_spectrum_data_t* data) {
  char buf[32];
  /* current_channel indexes the per-channel arrays, so it has to be brought
     back into storage range before it is used as an index. */
  uint8_t ch = rf_channel_is_indexable(data->current_channel)
                   ? data->current_channel
                   : 1;
  int8_t rssi = data->channel_rssi[ch];
  if (rssi < RF_NOISE_FLOOR)
    rssi = RF_NOISE_FLOOR;

  char th_str[8];
  if (data->threshold_dbm <= RF_NOISE_FLOOR) {
    snprintf(th_str, sizeof(th_str), "OFF");
  } else {
    snprintf(th_str, sizeof(th_str), "%d", data->threshold_dbm);
  }

  char ch_p = (data->protocol == RF_PROTOCOL_ZIGBEE) ? 'Z' : 'C';

  if (data->edit_mode == RF_EDIT_CHANNEL) {
    snprintf(buf, sizeof(buf), "%c%-2d* %3ddB T:%s", ch_p, ch, rssi, th_str);
  } else if (data->edit_mode == RF_EDIT_THRESHOLD) {
    snprintf(buf, sizeof(buf), "%c%-2d  %3ddB T:%s*", ch_p, ch, rssi, th_str);
  } else {
    snprintf(buf, sizeof(buf), "%c%-2d  %3ddB %s*", ch_p, ch, rssi,
             data->tri_channel_sweep ? "TRI" : "ALL");
  }
  oled_screen_display_text(buf, 0, 0, OLED_DISPLAY_NORMAL);
}

static void render_footer(const rf_spectrum_data_t* data) {
  const char* mode_name = "BAR";
  if (data->view_mode == SPECTRUM_VIEW_SCOPE) {
    mode_name = "SCP";
  } else if (data->view_mode == SPECTRUM_VIEW_WATERFALL) {
    mode_name = "FAL";
  } else if (data->view_mode == SPECTRUM_VIEW_FOXHUNT) {
    mode_name = "FOX";
  } else if (data->view_mode == SPECTRUM_VIEW_PEAK_TRACKER) {
    mode_name = "PEK";
  }
  const char* aud_str = data->audio_enabled ? "A" : "M";
  const char* sd_str =
      data->pcap_logging ? "CAP" : (data->sd_logging ? "CSV" : "--");
  const char* edit_tgt = (data->edit_mode == RF_EDIT_CHANNEL)     ? "Ch"
                         : (data->edit_mode == RF_EDIT_THRESHOLD) ? "Th"
                                                                  : "Fl";

  char buf[32];
  /* 15 chars max (120 px <= 128 px): "+-Fl >FOX A CAP" */
  snprintf(buf, sizeof(buf), "+-%-2s >%-3s %s %s", edit_tgt, mode_name, aud_str,
           sd_str);
  oled_screen_display_text(buf, 0, 7, OLED_DISPLAY_NORMAL);
}

static void render_bars_view(const rf_spectrum_data_t* data) {
  /* Drawing area: y = 10 to y = 53 (height 43 pixels) */
  const int base_y = 53;
  const int max_h = 40;
  const int bar_w = 6;
  const int bar_gap = 2;
  const int start_x = 8;

  /* Draw baseline */
  oled_screen_draw_hline(4, base_y + 1, 120, OLED_DISPLAY_NORMAL);

  /* Draw threshold line if active */
  if (data->threshold_dbm > RF_NOISE_FLOOR) {
    int th_h = rssi_to_height(data->threshold_dbm, max_h);
    int th_y = base_y - th_h;
    if (th_y >= 10 && th_y <= base_y) {
      for (int x = 4; x < 124; x += 4) {
        oled_screen_draw_hline(x, th_y, 2, OLED_DISPLAY_NORMAL);
      }
    }
  }

  for (int ch = 1; ch <= RF_WIFI_MAX_CHANNELS; ch++) {
    int x = start_x + (ch - 1) * (bar_w + bar_gap);
    int8_t rssi = data->channel_rssi[ch];

    /* If below threshold, filter out bar */
    if (data->threshold_dbm <= RF_NOISE_FLOOR || rssi >= data->threshold_dbm) {
      int h = rssi_to_height(rssi, max_h);
      int y = base_y - h;
      oled_screen_draw_box(x, y, bar_w, h, OLED_DISPLAY_NORMAL);
    }

    /* Draw peak hold tick if above threshold */
    int8_t peak = data->channel_peak[ch];
    if (data->threshold_dbm <= RF_NOISE_FLOOR || peak >= data->threshold_dbm) {
      int peak_h = rssi_to_height(peak, max_h);
      int peak_y = base_y - peak_h - 1;
      int cur_h = rssi_to_height(rssi, max_h);
      int cur_y = base_y - cur_h;
      if (peak_y >= 10 && peak_y < cur_y) {
        oled_screen_draw_hline(x, peak_y, bar_w, OLED_DISPLAY_NORMAL);
      }
    }

    /* Highlight cursor for current focused channel. Zigbee tunes 11..26 but
       the bar chart only has room for the Wi-Fi plan, so pin the cursor to the
       top of the range instead of silently dropping it. */
    uint8_t cursor_ch = rf_channel_is_indexable(data->current_channel)
                            ? data->current_channel
                            : 1;
    if (cursor_ch > RF_WIFI_MAX_CHANNELS)
      cursor_ch = RF_WIFI_MAX_CHANNELS;
    if (ch == cursor_ch) {
      /* Draw small indicator triangle / dot above the bar */
      oled_screen_draw_pixel(x + bar_w / 2, 9, OLED_DISPLAY_NORMAL);
      oled_screen_draw_pixel(x + bar_w / 2 - 1, 8, OLED_DISPLAY_NORMAL);
      oled_screen_draw_pixel(x + bar_w / 2 + 1, 8, OLED_DISPLAY_NORMAL);
    }
  }
}

static void render_scope_view(const rf_spectrum_data_t* data) {
  /* Continuous RF oscilloscope / signal envelope across 128 pixels */
  const int base_y = 53;
  const int max_h = 42;

  /* Draw baseline */
  oled_screen_draw_hline(0, base_y + 1, 128, OLED_DISPLAY_NORMAL);

  /* Draw threshold line if active */
  if (data->threshold_dbm > RF_NOISE_FLOOR) {
    int th_h = rssi_to_height(data->threshold_dbm, max_h);
    int th_y = base_y - th_h;
    if (th_y >= 10 && th_y <= base_y) {
      for (int x = 0; x < 128; x += 4) {
        oled_screen_draw_hline(x, th_y, 2, OLED_DISPLAY_NORMAL);
      }
    }
  }

  /* Draw grid marks */
  for (int x = 0; x < 128; x += 32) {
    oled_screen_draw_pixel(x, base_y - max_h / 2, OLED_DISPLAY_NORMAL);
    oled_screen_draw_pixel(x, base_y - max_h, OLED_DISPLAY_NORMAL);
  }

  /* Draw envelope from history ring buffer */
  uint8_t start_idx = data->hist_idx;
  for (int i = 0; i < 127; i++) {
    uint8_t idx = (start_idx + i) % 128;
    int8_t val = data->history[idx];
    if (data->threshold_dbm <= RF_NOISE_FLOOR || val >= data->threshold_dbm) {
      int h = rssi_to_height(val, max_h);
      int y = base_y - h;
      oled_screen_draw_vline(i, y, h, OLED_DISPLAY_NORMAL);
    }
  }
}

static void render_waterfall_view(const rf_spectrum_data_t* data) {
  /* 2D Waterfall: X = Channels 1..14, Y = time rows 12 to 52 */
  const int start_y = 12;
  const int num_rows = 36;
  const int bar_w = 7;
  const int bar_gap = 1;
  const int start_x = 8;

  for (int r = 0; r < num_rows; r++) {
    int row_idx = (data->wf_head + num_rows - 1 - r) % num_rows;
    int y = start_y + r;
    if (y >= 54)
      break;

    for (int ch = 1; ch <= RF_WIFI_MAX_CHANNELS; ch++) {
      int x = start_x + (ch - 1) * (bar_w + bar_gap);
      uint8_t intensity = data->waterfall[row_idx][ch];

      /* Dithering pattern based on intensity (0 - 4) */
      if (intensity >= 4) {
        oled_screen_draw_hline(x, y, bar_w, OLED_DISPLAY_NORMAL);
      } else if (intensity == 3) {
        for (int b = 0; b < bar_w; b += 2) {
          oled_screen_draw_pixel(x + b, y, OLED_DISPLAY_NORMAL);
        }
      } else if (intensity == 2) {
        if ((r % 2) == 0) {
          for (int b = 0; b < bar_w; b += 2) {
            oled_screen_draw_pixel(x + b, y, OLED_DISPLAY_NORMAL);
          }
        }
      } else if (intensity == 1) {
        if ((r % 3) == 0) {
          oled_screen_draw_pixel(x + bar_w / 2, y, OLED_DISPLAY_NORMAL);
        }
      }
    }
  }

  /* Current channel indicator on bottom */
  uint8_t cur_ch = rf_channel_is_indexable(data->current_channel)
                       ? data->current_channel
                       : 1;
  if (cur_ch > RF_WIFI_MAX_CHANNELS)
    cur_ch = RF_WIFI_MAX_CHANNELS;
  int cur_x = start_x + (cur_ch - 1) * (bar_w + bar_gap);
  oled_screen_draw_hline(cur_x, 54, bar_w, OLED_DISPLAY_NORMAL);
}

static void render_foxhunt_view(const rf_spectrum_data_t* data) {
  uint8_t ch = rf_channel_is_indexable(data->current_channel)
                   ? data->current_channel
                   : 1;
  int8_t rssi = data->channel_rssi[ch];
  int8_t peak = data->channel_peak[ch];

  int prox = 0;
  if (rssi > RF_NOISE_FLOOR) {
    prox =
        (int) (rssi - RF_NOISE_FLOOR) * 100 / (RF_MAX_SIGNAL - RF_NOISE_FLOOR);
    if (prox < 0)
      prox = 0;
    if (prox > 100)
      prox = 100;
  }

  /* Scale labels on Page 1 (15 chars = 120px + 4 = 124px <= 128px) */
  oled_screen_display_text("-90 -70 -50 -30", 4, 1, OLED_DISPLAY_NORMAL);

  /* Scale ticks above the meter: -90, -70, -50, -30 dBm */
  /* Frame coordinates: x = 6, y = 22, width = 116, height = 16 */
  const int box_x = 6;
  const int box_y = 22;
  const int box_w = 116;
  const int box_h = 16;
  const int inner_w = box_w - 4;  // 112

  /* Ticks at -90dBm (prox ~ 7%), -70dBm (prox ~ 35%), -50dBm (prox ~ 64%),
   * -30dBm (prox ~ 92%) */
  const int tick_offsets[] = {8, 39, 72, 103};
  for (int i = 0; i < 4; i++) {
    oled_screen_draw_vline(box_x + 2 + tick_offsets[i], box_y - 4, 4,
                           OLED_DISPLAY_NORMAL);
  }

  /* Outer frame */
  oled_screen_draw_rect(box_x, box_y, box_w, box_h, OLED_DISPLAY_NORMAL);

  /* Inner filled bar for proximity */
  if (data->threshold_dbm <= RF_NOISE_FLOOR || rssi >= data->threshold_dbm) {
    int fill_w = prox * inner_w / 100;
    if (fill_w > inner_w)
      fill_w = inner_w;
    if (fill_w > 0) {
      oled_screen_draw_box(box_x + 2, box_y + 2, fill_w, box_h - 4,
                           OLED_DISPLAY_NORMAL);
    }
  }

  /* Peak hold line */
  if (peak > RF_NOISE_FLOOR &&
      (data->threshold_dbm <= RF_NOISE_FLOOR || peak >= data->threshold_dbm)) {
    int peak_prox =
        (int) (peak - RF_NOISE_FLOOR) * 100 / (RF_MAX_SIGNAL - RF_NOISE_FLOOR);
    if (peak_prox > 100)
      peak_prox = 100;
    int peak_x = box_x + 2 + (peak_prox * inner_w / 100);
    if (peak_x >= box_x + 2 && peak_x <= box_x + box_w - 2) {
      oled_screen_draw_vline(peak_x, box_y, box_h, OLED_DISPLAY_NORMAL);
    }
  }

  /* Threshold marker if active */
  if (data->threshold_dbm > RF_NOISE_FLOOR) {
    int th_prox = (int) (data->threshold_dbm - RF_NOISE_FLOOR) * 100 /
                  (RF_MAX_SIGNAL - RF_NOISE_FLOOR);
    if (th_prox > 100)
      th_prox = 100;
    int th_x = box_x + 2 + (th_prox * inner_w / 100);
    if (th_x >= box_x + 2 && th_x <= box_x + box_w - 2) {
      for (int y = box_y; y < box_y + box_h; y += 2) {
        oled_screen_draw_pixel(th_x, y, OLED_DISPLAY_NORMAL);
      }
    }
  }

  /* Large digital readout on Page 5 */
  char buf[32];
  if (data->drone_count[ch] > 0) {
    snprintf(buf, sizeof(buf), "PROX:%3d%% DRN:%-2u", prox,
             data->drone_count[ch]);
  } else {
    snprintf(buf, sizeof(buf), "PROX:%3d%% PK:%3d", prox,
             (peak < RF_NOISE_FLOOR) ? RF_NOISE_FLOOR : peak);
  }
  oled_screen_display_text(buf, 0, 5, OLED_DISPLAY_NORMAL);
}

static void render_peak_tracker_view(const rf_spectrum_data_t* data) {
  char buf[32];
  uint8_t pk_ch = data->peak_channel;
  if (!rf_channel_is_indexable(pk_ch)) {
    pk_ch = rf_channel_clamp(data->protocol, data->current_channel);
  }
  int8_t rssi = data->peak_rssi;
  int8_t peak = data->peak_hold_dbm;
  if (peak < RF_NOISE_FLOOR)
    peak = RF_NOISE_FLOOR;
  uint16_t freq = data->peak_freq_mhz;
  if (freq == 0) {
    freq = rf_channel_to_frequency_mhz(data->protocol, pk_ch);
  }

  char ch_prefix = (data->protocol == RF_PROTOCOL_ZIGBEE) ? 'Z' : 'C';
  bool active = (rssi > RF_NOISE_FLOOR || data->peak_latch_timer > 0);

  /* 1. CHANNEL (2x large scale, 16px tall) */
  if (active) {
    snprintf(buf, sizeof(buf), "%c%02u", ch_prefix, pk_ch);
  } else {
    snprintf(buf, sizeof(buf), "%c--", ch_prefix);
  }
  int ch_w = menu_gfx_text_width(buf, 2);
  menu_gfx_text((128 - ch_w) / 2, 2, buf, 2, true);

  /* Corner mode identifier */
  menu_gfx_text(104, 2, "PEK", 1, true);

  /* 2. FREQUENCY (Clean and centered with accent lines) */
  if (active) {
    snprintf(buf, sizeof(buf), "%u MHz", freq);
  } else {
    snprintf(buf, sizeof(buf), "SEARCHING");
  }
  int f_w = menu_gfx_text_width(buf, 1);
  int f_x = (128 - f_w) / 2;
  menu_gfx_text(f_x, 20, buf, 1, true);
  if (f_x > 14) {
    oled_screen_draw_hline(8, 23, f_x - 12, OLED_DISPLAY_NORMAL);
    oled_screen_draw_hline(f_x + f_w + 4, 23, 120 - (f_x + f_w + 4),
                           OLED_DISPLAY_NORMAL);
  }

  /* 3. POWER / DBM (2x large scale, 16px tall) */
  if (active) {
    int8_t show_rssi = (rssi > RF_NOISE_FLOOR) ? rssi : RF_NOISE_FLOOR;
    snprintf(buf, sizeof(buf), "%ddBm", show_rssi);
  } else {
    snprintf(buf, sizeof(buf), "---dBm");
  }
  int dbm_w = menu_gfx_text_width(buf, 2);
  menu_gfx_text((128 - dbm_w) / 2, 30, buf, 2, true);

  /* 4. COMPACT TRACK BAR (Styled at the bottom) */
  const int bar_x = 14;
  const int bar_y = 48;
  const int bar_w = 100;
  const int bar_h = 5;
  const int inner_w = bar_w - 2;  // 98

  oled_screen_draw_rect(bar_x, bar_y, bar_w, bar_h, OLED_DISPLAY_NORMAL);

  int prox = 0;
  if (rssi > RF_NOISE_FLOOR) {
    prox =
        (int) (rssi - RF_NOISE_FLOOR) * 100 / (RF_MAX_SIGNAL - RF_NOISE_FLOOR);
    if (prox < 0)
      prox = 0;
    if (prox > 100)
      prox = 100;
    int fill_w = prox * inner_w / 100;
    if (fill_w > inner_w)
      fill_w = inner_w;
    if (fill_w > 0) {
      oled_screen_draw_box(bar_x + 1, bar_y + 1, fill_w, bar_h - 2,
                           OLED_DISPLAY_NORMAL);
    }
  }

  /* Peak hold marker */
  if (peak > RF_NOISE_FLOOR) {
    int peak_prox =
        (int) (peak - RF_NOISE_FLOOR) * 100 / (RF_MAX_SIGNAL - RF_NOISE_FLOOR);
    if (peak_prox > 100)
      peak_prox = 100;
    int peak_x = bar_x + 1 + (peak_prox * inner_w / 100);
    if (peak_x >= bar_x + 1 && peak_x <= bar_x + bar_w - 2) {
      oled_screen_draw_vline(peak_x, bar_y - 1, bar_h + 2, OLED_DISPLAY_NORMAL);
    }
  }

  /* 5. CLEAN WINDOW FOOTER (Contextual state or historical peak) */
  if (rf_channel_is_indexable(pk_ch) && data->jammer_alert[pk_ch]) {
    snprintf(buf, sizeof(buf), "! JAMMER %d%% !",
             data->jammer_confidence[pk_ch]);
    int a_w = menu_gfx_text_width(buf, 1);
    menu_gfx_text((128 - a_w) / 2, 56, buf, 1, true);
  } else if (rf_channel_is_indexable(pk_ch) && data->drone_count[pk_ch] > 0) {
    snprintf(buf, sizeof(buf), "DRN: %s",
             data->last_drone_id[0] ? data->last_drone_id : "DETECTED");
    int a_w = menu_gfx_text_width(buf, 1);
    menu_gfx_text((128 - a_w) / 2, 56, buf, 1, true);
  } else {
    menu_gfx_text(4, 56, ">PEK", 1, true);
    if (peak > RF_NOISE_FLOOR) {
      snprintf(buf, sizeof(buf), "MAX:%3ddB", peak);
      menu_gfx_text(64, 56, buf, 1, true);
    }
  }
}

void rf_spectrum_screen_render(const rf_spectrum_data_t* data) {
  oled_screen_clear_buffer();

  if (data->view_mode == SPECTRUM_VIEW_PEAK_TRACKER) {
    render_peak_tracker_view(data);
  } else {
    render_header(data);
    switch (data->view_mode) {
      case SPECTRUM_VIEW_BARS:
        render_bars_view(data);
        break;
      case SPECTRUM_VIEW_SCOPE:
        render_scope_view(data);
        break;
      case SPECTRUM_VIEW_WATERFALL:
        render_waterfall_view(data);
        break;
      case SPECTRUM_VIEW_FOXHUNT:
        render_foxhunt_view(data);
        break;
      default:
        render_bars_view(data);
        break;
    }
    render_footer(data);
  }

  /* Alert overlay banner (if active) */
  if (data->view_mode != SPECTRUM_VIEW_PEAK_TRACKER && data->alert_timer > 0 &&
      data->alert_msg[0] != '\0') {
    oled_screen_display_text_center((char*) data->alert_msg, 1,
                                    OLED_DISPLAY_INVERT);
  }

  oled_screen_display_show();
}

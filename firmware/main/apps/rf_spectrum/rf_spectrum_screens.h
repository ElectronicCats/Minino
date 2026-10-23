/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Screen rendering for Minino RF Spectrum Analyzer on SSD1306/SH1106 OLED (128x64).
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "rf_spectrum.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  uint8_t current_channel;                      /* 1 - 14 */
  int8_t  threshold_dbm;                        /* Threshold filter (-95 to -20 dBm, -95 = OFF) */
  rf_edit_mode_t edit_mode;                     /* RF_EDIT_CHANNEL or RF_EDIT_THRESHOLD */
  int8_t  channel_rssi[RF_MAX_CHANNELS + 1];    /* Instantaneous RSSI (-95 to -20 dBm) */
  int8_t  channel_peak[RF_MAX_CHANNELS + 1];    /* Peak hold RSSI */
  uint16_t channel_packets[RF_MAX_CHANNELS + 1];/* Packet counter */
  spectrum_view_mode_t view_mode;
  bool    audio_enabled;                        /* Buzzer Geiger audio active */
  bool    sd_logging;                           /* MicroSD active logging */
  bool    gps_fix;                              /* GPS satellite fix */
  rf_protocol_t protocol;                       /* RF_PROTOCOL_WIFI or RF_PROTOCOL_ZIGBEE */
  int8_t  history[128];                         /* Rolling scope buffer */
  uint8_t hist_idx;
  uint8_t waterfall[36][RF_MAX_CHANNELS + 1];   /* 36 historical rows */
  uint8_t wf_head;
  uint16_t deauth_count[RF_MAX_CHANNELS + 1];   /* RF IDS: Deauth frame count per channel */
  bool     jammer_alert[RF_MAX_CHANNELS + 1];   /* RF IDS: Jamming detection flag */
  bool     pcap_logging;                        /* MicroSD standard PCAP capture active */
  uint32_t pcap_packets;                        /* Total PCAP packets captured */
  uint16_t drone_count[RF_MAX_CHANNELS + 1];    /* OpenDroneID frames detected per channel */
  char     last_drone_id[24];                   /* UAS ID of last detected drone */
  int8_t   last_drone_rssi;                     /* RSSI of last detected drone */
  uint8_t  last_drone_ch;                       /* Channel of last detected drone */
  char     alert_msg[20];                       /* Active visual alert banner text */
  uint8_t  alert_timer;                         /* Alert banner display timer ticks */
  uint8_t  peak_channel;                        /* Channel of strongest signal detected */
  int8_t   peak_rssi;                           /* Instantaneous RSSI of peak signal */
  int8_t   peak_hold_dbm;                       /* Session peak RSSI */
  uint16_t peak_freq_mhz;                       /* Calculated frequency in MHz */
  uint8_t  peak_latch_timer;                    /* Transient display latch timer */
  bool     tri_channel_sweep;                   /* Fast sweep only non-overlapping 1, 6, 11 */
  int8_t   channel_rssi_ema[RF_MAX_CHANNELS + 1]; /* EMA smoothed RSSI */
  uint8_t  jammer_confidence[RF_MAX_CHANNELS + 1];/* Jammer confidence index 0..100% */
} rf_spectrum_data_t;

/**
 * @brief Render current spectrum data onto OLED display buffer
 */
void rf_spectrum_screen_render(const rf_spectrum_data_t* data);

#ifdef __cplusplus
}
#endif

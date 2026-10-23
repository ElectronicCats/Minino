/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Minino RF Spectrum Analyzer
 * Real-time 2.4 GHz spectrum & channel power visualization on OLED.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Storage is sized for the widest channel plan we track, not for Wi-Fi alone:
   Zigbee runs 11..26, and current_channel is used to index channel_rssi[],
   channel_peak[], drone_count[], ... so anything above RF_WIFI_MAX_CHANNELS
   read past the end of those arrays. */
#define RF_WIFI_MAX_CHANNELS  13
#define RF_ZIGBEE_MIN_CHANNEL 11
#define RF_ZIGBEE_MAX_CHANNEL 26
#define RF_MAX_CHANNELS       RF_ZIGBEE_MAX_CHANNEL
#define RF_NOISE_FLOOR  -95
#define RF_MAX_SIGNAL   -25

typedef enum {
  SPECTRUM_VIEW_BARS = 0,         /* 14 Wi-Fi channels bar chart (2.4 GHz overview) */
  SPECTRUM_VIEW_SCOPE = 1,        /* Single channel live power scope / curve */
  SPECTRUM_VIEW_WATERFALL = 2,    /* Rolling 2D waterfall */
  SPECTRUM_VIEW_FOXHUNT = 3,      /* Fox Hunting / Direction Finding high-gain meter */
  SPECTRUM_VIEW_PEAK_TRACKER = 4  /* Peak Tracker: Strongest signal dBm, Channel & Frequency */
} spectrum_view_mode_t;

#define SPECTRUM_VIEW_MAX 5

typedef enum {
  RF_EDIT_CHANNEL = 0,        /* UP/DOWN buttons tune channel */
  RF_EDIT_THRESHOLD = 1,      /* UP/DOWN buttons tune dBm threshold filter */
  RF_EDIT_FILTER = 2          /* UP/DOWN buttons toggle filter mode (ALL vs TRI 1,6,11) */
} rf_edit_mode_t;

typedef enum {
  RF_PROTOCOL_WIFI = 0,    /* Wi-Fi 2.4 GHz channels 1..13 (2412 - 2472 MHz) */
  RF_PROTOCOL_ZIGBEE = 1   /* IEEE 802.15.4 / Zigbee channels 11..26 (2405 - 2480 MHz) */
} rf_protocol_t;

/**
 * @brief Lowest tunable channel for a protocol (1 for Wi-Fi, 11 for Zigbee)
 */
uint8_t rf_channel_min(rf_protocol_t proto);

/**
 * @brief Highest tunable channel for a protocol (14 for Wi-Fi, 26 for Zigbee)
 */
uint8_t rf_channel_max(rf_protocol_t proto);

/**
 * @brief Clamp a channel into the range of a protocol.
 *
 * Every read of current_channel that indexes a per-channel array must go
 * through this: the array is sized RF_MAX_CHANNELS wide, but the tuner can
 * otherwise hand us a value outside it.
 */
uint8_t rf_channel_clamp(rf_protocol_t proto, uint8_t channel);

/**
 * @brief True when the channel is inside the storage range (1..RF_MAX_CHANNELS)
 */
bool rf_channel_is_indexable(uint8_t channel);
 
/**
 * @brief Calculate the center frequency in MHz for a given channel and protocol
 */
uint16_t rf_channel_to_frequency_mhz(rf_protocol_t proto, uint8_t channel);

/**
 * @brief Next channel in sequence for auto-sweep.
 * If tri_channel_only is true and proto is Wi-Fi, cycles 1 -> 6 -> 11 -> 1.
 */
uint8_t rf_channel_next_sweep(rf_protocol_t proto, uint8_t current_channel, bool tri_channel_only);

/**
 * @brief Exponential Moving Average (EMA) filter for RSSI smoothing.
 * @param current_ema Current smoothed value (or <= RF_NOISE_FLOOR to reset)
 * @param new_rssi New instantaneous RSSI sample
 * @param alpha_pct Alpha weight percentage (1..100, e.g. 30 = 30%)
 */
int8_t rf_filter_ema_rssi(int8_t current_ema, int8_t new_rssi, uint8_t alpha_pct);

/**
 * @brief Evaluator for true RF / 802.11 jamming conditions.
 * Discriminates between legitimate AP traffic, adjacent channel spillover,
 * Deauth/Disassociation Layer-2 flooding, and Layer-1 RF jamming/noise.
 */
bool rf_jammer_evaluate(int8_t rssi, uint16_t beacons, uint16_t deauths,
                        uint16_t fcs_errors, uint16_t valid_frames,
                        bool adjacent_ap);

/**
 * @brief Launch the RF Spectrum Analyzer app from menu or console
 */
void rf_spectrum_app_begin(void);

/**
 * @brief Stop the RF Spectrum Analyzer and release radio resources
 */
void rf_spectrum_app_stop(void);

/**
 * @brief Set the RSSI threshold filter in dBm (-95 to -20 dBm, -95 = OFF / unfiltered)
 */
void rf_spectrum_set_threshold(int8_t threshold_dbm);

/**
 * @brief Get current RSSI threshold filter in dBm
 */
int8_t rf_spectrum_get_threshold(void);

/**
 * @brief Set active Wi-Fi channel (1-14)
 */
void rf_spectrum_set_channel(uint8_t channel);

/**
 * @brief Get active Wi-Fi channel
 */
uint8_t rf_spectrum_get_channel(void);

/**
 * @brief Enable or disable audio feedback (Geiger clicks on Buzzer)
 */
void rf_spectrum_set_audio_enabled(bool enabled);
bool rf_spectrum_get_audio_enabled(void);
void rf_spectrum_toggle_audio(void);

/**
 * @brief Enable or disable MicroSD CSV logging
 */
void rf_spectrum_set_sd_log_enabled(bool enabled);
bool rf_spectrum_get_sd_log_enabled(void);
void rf_spectrum_toggle_sd_log(void);

/**
 * @brief Set or get active RF protocol / band (Wi-Fi vs Zigbee)
 */
void rf_spectrum_set_protocol(rf_protocol_t proto);
rf_protocol_t rf_spectrum_get_protocol(void);

/**
 * @brief Get total deauth/disassoc frames detected on channel (0 = all channels)
 */
uint16_t rf_spectrum_get_deauth_count(uint8_t channel);

/**
 * @brief Check if jamming condition is detected on channel (0 = any channel)
 */
bool rf_spectrum_has_jammer_alert(uint8_t channel);

/**
 * @brief Enable or disable tri-channel sweep (Wi-Fi 1, 6, 11)
 */
void rf_spectrum_set_tri_channel_enabled(bool enabled);
bool rf_spectrum_get_tri_channel_enabled(void);
void rf_spectrum_toggle_tri_channel(void);

/**
 * @brief Clear all active RF IDS alerts and counters
 */
void rf_spectrum_clear_alerts(void);

/**
 * @brief Enable or disable direct PCAP capture to MicroSD
 */
void rf_spectrum_set_pcap_enabled(bool enabled);
bool rf_spectrum_get_pcap_enabled(void);
void rf_spectrum_toggle_pcap(void);

/**
 * @brief Get total OpenDroneID frames detected on channel (0 = all channels)
 */
uint16_t rf_spectrum_get_drone_count(uint8_t channel);

/**
 * @brief Get UAS ID of last detected drone
 */
const char* rf_spectrum_get_last_drone_id(void);

/**
 * @brief Export spectrum state as JSON for web dashboard
 */
size_t rf_spectrum_get_status_json(char* buf, size_t max_len);

#ifdef __cplusplus
}
#endif

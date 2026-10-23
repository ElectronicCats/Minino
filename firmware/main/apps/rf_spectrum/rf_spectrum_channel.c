/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Channel-range helpers for the RF Spectrum Analyzer.
 *
 * Kept free of ESP-IDF dependencies on purpose: the per-channel arrays
 * (channel_rssi, channel_peak, drone_count, ...) are sized RF_MAX_CHANNELS and
 * indexed by current_channel, so the bounds live in one place that the host
 * test suite can exercise directly.
 */

#include "rf_spectrum.h"

uint8_t rf_channel_min(rf_protocol_t proto) {
  return (proto == RF_PROTOCOL_ZIGBEE) ? RF_ZIGBEE_MIN_CHANNEL : 1;
}

uint8_t rf_channel_max(rf_protocol_t proto) {
  return (proto == RF_PROTOCOL_ZIGBEE) ? RF_ZIGBEE_MAX_CHANNEL
                                       : RF_WIFI_MAX_CHANNELS;
}

uint8_t rf_channel_clamp(rf_protocol_t proto, uint8_t channel) {
  uint8_t lo = rf_channel_min(proto);
  uint8_t hi = rf_channel_max(proto);
  if (channel < lo) {
    return lo;
  }
  if (channel > hi) {
    return hi;
  }
  return channel;
}

bool rf_channel_is_indexable(uint8_t channel) {
  return channel >= 1 && channel <= RF_MAX_CHANNELS;
}

uint16_t rf_channel_to_frequency_mhz(rf_protocol_t proto, uint8_t channel) {
  if (proto == RF_PROTOCOL_WIFI && channel == 14) {
    return 2484u;
  }
  uint8_t ch = rf_channel_clamp(proto, channel);
  if (proto == RF_PROTOCOL_ZIGBEE) {
    return 2405u + (uint16_t) (ch - RF_ZIGBEE_MIN_CHANNEL) * 5u;
  }
  return 2407u + (uint16_t) ch * 5u;
}

uint8_t rf_channel_next_sweep(rf_protocol_t proto,
                              uint8_t current_channel,
                              bool tri_channel_only) {
  if (proto == RF_PROTOCOL_WIFI) {
    if (tri_channel_only) {
      if (current_channel == 1)
        return 6;
      if (current_channel == 6)
        return 11;
      return 1;
    }
    return (current_channel % RF_WIFI_MAX_CHANNELS) + 1;
  }
  if (current_channel < RF_ZIGBEE_MIN_CHANNEL ||
      current_channel >= RF_ZIGBEE_MAX_CHANNEL) {
    return RF_ZIGBEE_MIN_CHANNEL;
  }
  return current_channel + 1;
}

int8_t rf_filter_ema_rssi(int8_t current_ema,
                          int8_t new_rssi,
                          uint8_t alpha_pct) {
  if (current_ema <= RF_NOISE_FLOOR) {
    return new_rssi;
  }
  if (new_rssi <= RF_NOISE_FLOOR) {
    return current_ema;
  }
  if (alpha_pct == 0)
    return current_ema;
  if (alpha_pct >= 100)
    return new_rssi;
  int32_t val = (int32_t) alpha_pct * (int32_t) new_rssi +
                (100 - (int32_t) alpha_pct) * (int32_t) current_ema;
  if (val >= 0) {
    val += 50;
  } else {
    val -= 50;
  }
  return (int8_t) (val / 100);
}

bool rf_jammer_evaluate(int8_t rssi,
                        uint16_t beacons,
                        uint16_t deauths,
                        uint16_t fcs_errors,
                        uint16_t valid_frames,
                        bool adjacent_ap) {
  /* An active AP sending valid beacons is legitimate Wi-Fi */
  if (beacons > 0) {
    return false;
  }

  /* Adjacent channel power bleeding from a legitimate AP */
  if (adjacent_ap && deauths < 8 && fcs_errors < 15) {
    return false;
  }

  /* Layer 2 Deauth / Disassociation flood attack */
  if (deauths >= 10) {
    return true;
  }

  /* Layer 1 RF noise / continuous jammer / corrupted frame surge */
  if (rssi >= -42) {
    if (fcs_errors >= 20 || (fcs_errors >= 10 && valid_frames < 3)) {
      return true;
    }
  }

  return false;
}
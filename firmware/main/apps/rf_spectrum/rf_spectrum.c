/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Minino RF Spectrum Analyzer
 * Real-time 2.4 GHz spectrum & channel power visualization on OLED.
 */

#include "rf_spectrum.h"
#include "app_manager.h"
#include "radio_lease.h"
#include <stdio.h>
#include <string.h>
#include "buzzer.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "keyboard_module.h"
#include "menus_module.h"
#include "preferences.h"
#include "rf_spectrum_screens.h"
#include "rf_spectrum_drone.h"
#include "rf_spectrum_pcap.h"
#include "rf_spectrum_web.h"
#include "sd_card.h"
#include "wifi_controller.h"

static const char* TAG = "rf_spectrum";

static void rf_spectrum_start(void);
static void rf_spectrum_input_cb(uint8_t button_name, uint8_t button_event);

static const minino_app_t s_app = {
    .name = "rf_spectrum",
    .on_enter = rf_spectrum_start,
    .on_input = rf_spectrum_input_cb,
    .on_exit = rf_spectrum_app_stop,
};

static volatile bool s_running = false;
static TaskHandle_t s_spectrum_task_handle = NULL;
static rf_spectrum_data_t s_data = {.current_channel = 1,
                                    .threshold_dbm = -85,
                                    .edit_mode = RF_EDIT_CHANNEL,
                                    .audio_enabled = true,
                                    .sd_logging = false,
                                    .gps_fix = false,
                                    .protocol = RF_PROTOCOL_WIFI,
                                    .peak_channel = 1,
                                    .peak_rssi = RF_NOISE_FLOOR,
                                    .peak_hold_dbm = RF_NOISE_FLOOR,
                                    .peak_freq_mhz = 2412,
                                    .peak_latch_timer = 0};
static int64_t s_last_click_us = 0;
static bool s_sd_header_written = false;
static uint8_t s_jammer_streak[RF_MAX_CHANNELS + 1] = {0};
static uint16_t s_channel_beacons[RF_MAX_CHANNELS + 1] = {0};
static uint16_t s_channel_fcs_errors[RF_MAX_CHANNELS + 1] = {0};
static uint16_t s_channel_valid_frames[RF_MAX_CHANNELS + 1] = {0};
static uint16_t s_channel_deauth_win[RF_MAX_CHANNELS + 1] = {0};
static uint8_t s_ap_presence_decay[RF_MAX_CHANNELS + 1] = {0};

static void save_preferences(void) {
  preferences_put_char("SPEC_TH", s_data.threshold_dbm);
  preferences_put_uchar("SPEC_CH", s_data.current_channel);
  preferences_put_uchar("SPEC_AUD", s_data.audio_enabled ? 1 : 0);
  preferences_put_uchar("SPEC_VM", (uint8_t)s_data.view_mode);
  preferences_put_uchar("SPEC_TRI", s_data.tri_channel_sweep ? 1 : 0);
}

static void spectrum_promiscuous_rx_cb(void* buf, wifi_promiscuous_pkt_type_t type) {
  (void)type;
  if (!s_running || buf == NULL) {
    return;
  }

  const wifi_promiscuous_pkt_t* pkt = (const wifi_promiscuous_pkt_t*)buf;
  int8_t rssi = pkt->rx_ctrl.rssi;
  uint8_t ch = pkt->rx_ctrl.channel;

  /* Filter out packets below the configured threshold */
  if (s_data.threshold_dbm > RF_NOISE_FLOOR && rssi < s_data.threshold_dbm) {
    return;
  }

  /* Audio Geiger click if enabled and packet is on focused channel or strong */
  if (s_data.audio_enabled && (ch == s_data.current_channel || rssi >= -50)) {
    int64_t now_us = esp_timer_get_time();
    int64_t min_interval_us = ((-rssi) - 20) * 800LL;
    if (min_interval_us < 18000) min_interval_us = 18000;
    if (now_us - s_last_click_us >= min_interval_us) {
      s_last_click_us = now_us;
      buzzer_play_for(6);
    }
  }

  if (ch >= 1 && ch <= RF_MAX_CHANNELS) {
    if (rssi > s_data.channel_rssi[ch]) {
      s_data.channel_rssi[ch] = rssi;
    }
    s_data.channel_rssi_ema[ch] = rf_filter_ema_rssi(s_data.channel_rssi_ema[ch], rssi, 30);
    if (rssi > s_data.channel_peak[ch]) {
      s_data.channel_peak[ch] = rssi;
    }
    s_data.channel_packets[ch]++;

    /* Physical Layer frame check: rx_state != 0 means corrupted/FCS failure */
    if (pkt->rx_ctrl.rx_state != 0) {
      s_channel_fcs_errors[ch]++;
    } else {
      s_channel_valid_frames[ch]++;
    }

    /* Direct PCAP streaming to MicroSD if active */
    if (s_data.pcap_logging) {
      rf_spectrum_pcap_write_packet(pkt->payload, pkt->rx_ctrl.sig_len);
      s_data.pcap_packets = rf_spectrum_pcap_get_packet_count();
    }

    /* OpenDroneID passive detection */
    drone_detection_t drone_info;
    if (rf_spectrum_drone_detect(pkt->payload, pkt->rx_ctrl.sig_len, &drone_info)) {
      s_data.drone_count[ch]++;
      s_data.last_drone_ch = ch;
      s_data.last_drone_rssi = rssi;
      strncpy(s_data.last_drone_id, drone_info.uas_id, sizeof(s_data.last_drone_id) - 1);
      snprintf(s_data.alert_msg, sizeof(s_data.alert_msg), "! DRONE CH %d !", ch);
      s_data.alert_timer = 35; /* ~700ms display */
      if (s_data.audio_enabled) {
        buzzer_play_for(30);
      }
    }

    /* RF IDS: Inspect 802.11 Frame Control Byte 0 */
    if (pkt->rx_ctrl.sig_len >= 10) {
      uint8_t fc = pkt->payload[0];
      /* 0x80 = Beacon frame (transmitted by APs periodically) */
      if ((fc & 0xFC) == 0x80) {
        s_channel_beacons[ch]++;
        s_ap_presence_decay[ch] = 100; /* Active legitimate AP */
      }
      /* 0xC0 = Deauthentication, 0xA0 = Disassociation */
      if (fc == 0xC0 || fc == 0xA0) {
        s_channel_deauth_win[ch]++;
        s_data.deauth_count[ch]++;
        snprintf(s_data.alert_msg, sizeof(s_data.alert_msg), "! DEAUTH CH %d !", ch);
        s_data.alert_timer = 25; /* ~500ms display */
        if (s_data.audio_enabled) {
          buzzer_play_for(20);
        }
      }
    }
  }
}

static uint8_t rssi_to_intensity(int8_t rssi) {
  if (rssi <= -85) return 0;
  if (rssi <= -75) return 1;
  if (rssi <= -65) return 2;
  if (rssi <= -50) return 3;
  return 4;
}

static void log_to_sd_card(void) {
  if (!s_data.sd_logging || !sd_card_is_mounted()) {
    return;
  }

  const char* log_path = "/sdcard/spectrum_log.csv";

  if (!s_sd_header_written) {
    char header[] = "Timestamp_ms,Protocol,Channel,RSSI_dBm,Peak_dBm,Packets,Threshold_dBm\n";
    sd_card_append_to_file(log_path, header);
    s_sd_header_written = true;
  }

  char line[128];
  int64_t ms = esp_timer_get_time() / 1000;
  uint8_t c = rf_channel_clamp(s_data.protocol, s_data.current_channel);
  snprintf(line, sizeof(line), "%lld,%s,%d,%d,%d,%u,%d\n",
           (long long)ms,
           (s_data.protocol == RF_PROTOCOL_ZIGBEE) ? "Zigbee" : "WiFi",
           c,
           s_data.channel_rssi[c],
           s_data.channel_peak[c],
           s_data.channel_packets[c],
           s_data.threshold_dbm);
  sd_card_append_to_file(log_path, line);
}

static void rf_spectrum_task(void* arg) {
  (void)arg;
  uint8_t sweep_ch = 1;
  uint32_t tick = 0;

  ESP_LOGI(TAG, "RF Spectrum analyzer task started");

  while (s_running) {
    tick++;

    /* 1. Frequency tuning / Channel management */
    if (s_data.view_mode == SPECTRUM_VIEW_SCOPE || s_data.view_mode == SPECTRUM_VIEW_FOXHUNT) {
      wifi_driver_set_channel(s_data.current_channel);
      vTaskDelay(pdMS_TO_TICKS(20));
    } else {
      sweep_ch = rf_channel_next_sweep(s_data.protocol, sweep_ch, s_data.tri_channel_sweep);
      wifi_driver_set_channel(sweep_ch);
      vTaskDelay(pdMS_TO_TICKS(s_data.view_mode == SPECTRUM_VIEW_PEAK_TRACKER ? 12 : 20));
    }

    /* Peak tracker evaluation: identify strongest active signal across the band */
    uint8_t best_ch = 0;
    int8_t best_rssi = RF_NOISE_FLOOR;
    for (int c = 1; c <= RF_WIFI_MAX_CHANNELS; c++) {
      if (s_data.channel_rssi[c] > best_rssi) {
        best_rssi = s_data.channel_rssi[c];
        best_ch = (uint8_t) c;
      }
    }

    if (best_ch > 0 && best_rssi > RF_NOISE_FLOOR &&
        (s_data.threshold_dbm <= RF_NOISE_FLOOR || best_rssi >= s_data.threshold_dbm)) {
      s_data.peak_channel = best_ch;
      s_data.peak_rssi = best_rssi;
      s_data.peak_freq_mhz = rf_channel_to_frequency_mhz(s_data.protocol, best_ch);
      if (best_rssi > s_data.peak_hold_dbm) {
        s_data.peak_hold_dbm = best_rssi;
      }
      s_data.peak_latch_timer = 25; /* ~300ms latch to hold transient pulse on display */
    } else if (s_data.peak_latch_timer > 0) {
      s_data.peak_latch_timer--;
      if (s_data.peak_latch_timer == 0) {
        s_data.peak_rssi = RF_NOISE_FLOOR;
      }
    } else {
      s_data.peak_rssi = RF_NOISE_FLOOR;
    }

    /* Check jamming condition with multi-factor heuristic */
    for (int c = 1; c <= RF_MAX_CHANNELS; c++) {
      bool adjacent_ap = false;
      if (c > 1 && s_ap_presence_decay[c - 1] > 0) adjacent_ap = true;
      if (c < RF_WIFI_MAX_CHANNELS && s_ap_presence_decay[c + 1] > 0) adjacent_ap = true;

      bool jam_detected = rf_jammer_evaluate(s_data.channel_rssi[c],
                                             s_channel_beacons[c],
                                             s_channel_deauth_win[c],
                                             s_channel_fcs_errors[c],
                                             s_channel_valid_frames[c],
                                             adjacent_ap);

      if (jam_detected) {
        if (s_jammer_streak[c] < 50) s_jammer_streak[c]++;
        if (s_jammer_streak[c] >= 20) {
          s_data.jammer_alert[c] = true;
          s_data.jammer_confidence[c] = (uint8_t)((s_jammer_streak[c] * 100) / 50);
          if (s_data.alert_timer == 0) {
            snprintf(s_data.alert_msg, sizeof(s_data.alert_msg), "! JAMMER CH %d !", c);
            s_data.alert_timer = 20;
          }
          if (s_data.audio_enabled && (tick % 15 == 0)) {
            buzzer_play_for(15);
          }
        }
      } else {
        if (s_jammer_streak[c] > 0) s_jammer_streak[c]--;
        if (s_jammer_streak[c] == 0) {
          s_data.jammer_alert[c] = false;
          s_data.jammer_confidence[c] = 0;
        }
      }
    }

    /* Reset sliding observation window counters every ~1 second (50 ticks) */
    if ((tick % 50) == 0) {
      for (int c = 1; c <= RF_MAX_CHANNELS; c++) {
        s_channel_beacons[c] = 0;
        s_channel_fcs_errors[c] = 0;
        s_channel_valid_frames[c] = 0;
        s_channel_deauth_win[c] = 0;
      }
    }

    /* Alert display decay timer */
    if (s_data.alert_timer > 0) {
      s_data.alert_timer--;
      if (s_data.alert_timer == 0) {
        s_data.alert_msg[0] = '\0';
      }
    }

    /* 2. Record to scope buffer (for Scope View) */
    uint8_t ch = rf_channel_clamp(s_data.protocol, s_data.current_channel);
    int8_t cur_rssi = s_data.channel_rssi[ch];
    s_data.history[s_data.hist_idx] = cur_rssi;
    s_data.hist_idx = (s_data.hist_idx + 1) % 128;

    /* 3. Record to waterfall buffer every ~60ms */
    if ((tick % 3) == 0) {
      for (int c = 1; c <= RF_MAX_CHANNELS; c++) {
        int8_t rssi = s_data.channel_rssi[c];
        if (s_data.threshold_dbm > RF_NOISE_FLOOR && rssi < s_data.threshold_dbm) {
          s_data.waterfall[s_data.wf_head][c] = 0;
        } else {
          s_data.waterfall[s_data.wf_head][c] = rssi_to_intensity(rssi);
        }
      }
      s_data.wf_head = (s_data.wf_head + 1) % 36;
    }

    /* 4. Smooth decay: slowly pull signals towards noise floor */
    for (int c = 1; c <= RF_MAX_CHANNELS; c++) {
      if (s_ap_presence_decay[c] > 0) {
        s_ap_presence_decay[c]--;
      }
      if (s_data.channel_rssi[c] > RF_NOISE_FLOOR) {
        s_data.channel_rssi[c] -= 1;
      }
      if (s_data.channel_rssi_ema[c] > RF_NOISE_FLOOR) {
        s_data.channel_rssi_ema[c] -= 1;
      }
      if ((tick % 20) == 0 && s_data.channel_peak[c] > RF_NOISE_FLOOR) {
        s_data.channel_peak[c] -= 1;
      }
    }

    /* 5. Periodically log to MicroSD if active (~every 1 second) */
    if ((tick % 50) == 0) {
      log_to_sd_card();
    }

    /* 6. Render onto OLED */
    rf_spectrum_screen_render(&s_data);
  }

  s_spectrum_task_handle = NULL;
  vTaskDelete(NULL);
}

static void rf_spectrum_input_cb(uint8_t button_name, uint8_t button_event) {
  if (button_event == BUTTON_PRESS_DOWN) {
    switch (button_name) {
      case BUTTON_BOOT:
        s_data.edit_mode = (rf_edit_mode_t)((s_data.edit_mode + 1) % 3);
        ESP_LOGI(TAG, "Edit mode: %d", s_data.edit_mode);
        break;

      case BUTTON_UP:
        if (s_data.edit_mode == RF_EDIT_CHANNEL) {
          uint8_t max_ch = rf_channel_max(s_data.protocol);
          uint8_t min_ch = rf_channel_min(s_data.protocol);
          s_data.current_channel = (s_data.current_channel >= max_ch) ? min_ch : s_data.current_channel + 1;
          ESP_LOGI(TAG, "Selected channel: %d", s_data.current_channel);
          save_preferences();
        } else if (s_data.edit_mode == RF_EDIT_THRESHOLD) {
          if (s_data.threshold_dbm < -30) {
            s_data.threshold_dbm += 5;
          }
          ESP_LOGI(TAG, "Threshold set to: %d dBm", s_data.threshold_dbm);
          save_preferences();
        } else {
          s_data.tri_channel_sweep = !s_data.tri_channel_sweep;
          ESP_LOGI(TAG, "Sweep filter: %s", s_data.tri_channel_sweep ? "TRI (1,6,11)" : "ALL");
          save_preferences();
        }
        break;

      case BUTTON_DOWN:
        if (s_data.edit_mode == RF_EDIT_CHANNEL) {
          uint8_t max_ch = rf_channel_max(s_data.protocol);
          uint8_t min_ch = rf_channel_min(s_data.protocol);
          s_data.current_channel = (s_data.current_channel <= min_ch) ? max_ch : s_data.current_channel - 1;
          ESP_LOGI(TAG, "Selected channel: %d", s_data.current_channel);
          save_preferences();
        } else if (s_data.edit_mode == RF_EDIT_THRESHOLD) {
          if (s_data.threshold_dbm > RF_NOISE_FLOOR) {
            s_data.threshold_dbm -= 5;
          }
          ESP_LOGI(TAG, "Threshold set to: %d dBm", s_data.threshold_dbm);
          save_preferences();
        } else {
          s_data.tri_channel_sweep = !s_data.tri_channel_sweep;
          ESP_LOGI(TAG, "Sweep filter: %s", s_data.tri_channel_sweep ? "TRI (1,6,11)" : "ALL");
          save_preferences();
        }
        break;

      case BUTTON_RIGHT:
        s_data.view_mode = (spectrum_view_mode_t)((s_data.view_mode + 1) % SPECTRUM_VIEW_MAX);
        ESP_LOGI(TAG, "Switched view mode to: %d", s_data.view_mode);
        save_preferences();
        break;

      case BUTTON_LEFT:
        ESP_LOGI(TAG, "Exiting RF Spectrum Analyzer");
        app_manager_exit();
        break;

      default:
        break;
    }
  } else if (button_event == BUTTON_LONG_PRESS_START) {
    if (button_name == BUTTON_BOOT) {
      rf_spectrum_toggle_audio();
    } else if (button_name == BUTTON_RIGHT) {
      rf_spectrum_toggle_sd_log();
    } else if (button_name == BUTTON_DOWN) {
      rf_spectrum_toggle_pcap();
    } else if (button_name == BUTTON_UP) {
      rf_spectrum_set_protocol((s_data.protocol == RF_PROTOCOL_WIFI) ? RF_PROTOCOL_ZIGBEE : RF_PROTOCOL_WIFI);
    }
  }
}

static void rf_spectrum_start(void) {
  ESP_LOGI(TAG, "Initializing RF Spectrum Analyzer Suite...");

  /* Load user preferences from NVS */
  int8_t loaded_th = preferences_get_char("SPEC_TH", -85);
  if (loaded_th < RF_NOISE_FLOOR || loaded_th > -20) {
    loaded_th = -85;
  }
  uint8_t loaded_ch = preferences_get_uchar("SPEC_CH", 1);
  if (loaded_ch < 1 || loaded_ch > RF_MAX_CHANNELS) {
    loaded_ch = 1;
  }
  bool loaded_aud = (preferences_get_uchar("SPEC_AUD", 1) != 0);
  bool loaded_tri = (preferences_get_uchar("SPEC_TRI", 0) != 0);
  spectrum_view_mode_t loaded_vm = (spectrum_view_mode_t)preferences_get_uchar("SPEC_VM", SPECTRUM_VIEW_BARS);
  if (loaded_vm >= SPECTRUM_VIEW_MAX) {
    loaded_vm = SPECTRUM_VIEW_BARS;
  }

  memset(&s_data, 0, sizeof(s_data));
  s_data.current_channel = loaded_ch;
  s_data.threshold_dbm = loaded_th;
  s_data.audio_enabled = loaded_aud;
  s_data.tri_channel_sweep = loaded_tri;
  s_data.view_mode = loaded_vm;
  s_data.edit_mode = RF_EDIT_CHANNEL;
  s_data.protocol = RF_PROTOCOL_WIFI;
  s_data.sd_logging = false;
  s_data.pcap_logging = false;
  s_sd_header_written = false;
  s_data.peak_channel = 1;
  s_data.peak_rssi = RF_NOISE_FLOOR;
  s_data.peak_hold_dbm = RF_NOISE_FLOOR;
  s_data.peak_freq_mhz = 2412;
  s_data.peak_latch_timer = 0;

  for (int c = 1; c <= RF_MAX_CHANNELS; c++) {
    s_data.channel_rssi[c] = RF_NOISE_FLOOR;
    s_data.channel_rssi_ema[c] = RF_NOISE_FLOOR;
    s_data.channel_peak[c] = RF_NOISE_FLOOR;
    s_channel_beacons[c] = 0;
    s_channel_fcs_errors[c] = 0;
    s_channel_valid_frames[c] = 0;
    s_channel_deauth_win[c] = 0;
    s_ap_presence_decay[c] = 0;
    s_jammer_streak[c] = 0;
  }
  for (int i = 0; i < 128; i++) {
    s_data.history[i] = RF_NOISE_FLOOR;
  }

  /* Ensure Wi-Fi radio is initialized in station/promiscuous mode */
  if (!wifi_driver_get_initialized() && !wifi_driver_init_sta()) {
    ESP_LOGE(TAG, "Wi-Fi driver unavailable (BLE holds the radio?)");
    app_manager_exit();
    return;
  }

  if (radio_lease_prom_acquire(RADIO_PROM_SPECTRUM,
                               spectrum_promiscuous_rx_cb) != ESP_OK) {
    ESP_LOGW(TAG, "Could not claim promiscuous mode");
  }

  /* Start web dashboard */
  rf_spectrum_web_start();

  s_running = true;

  /* Start background measurement & rendering task */
  BaseType_t ret = xTaskCreate(rf_spectrum_task, "rf_spectrum_task", 4096, NULL, 5,
                               &s_spectrum_task_handle);
  if (ret != pdPASS) {
    ESP_LOGE(TAG, "Failed to create spectrum task");
    app_manager_exit();
  }
}

void rf_spectrum_app_begin(void) {
  app_manager_enter(&s_app);
}

void rf_spectrum_app_stop(void) {
  if (!s_running) {
    return;
  }

  s_running = false;

  /* Detach promiscuous callback. Only the last consumer actually turns the
     driver off, so a sniffer or the detector sharing the slot keeps working. */
  radio_lease_prom_release(RADIO_PROM_SPECTRUM);

  /* Stop PCAP capture if active */
  if (s_data.pcap_logging) {
    rf_spectrum_pcap_stop();
    s_data.pcap_logging = false;
  }

  /* Stop web dashboard */
  rf_spectrum_web_stop();

  /* Wait for task to exit cleanly */
  int wait_count = 0;
  while (s_spectrum_task_handle != NULL && wait_count < 20) {
    vTaskDelay(pdMS_TO_TICKS(10));
    wait_count++;
  }

  save_preferences();
  ESP_LOGI(TAG, "RF Spectrum Analyzer stopped.");
}

void rf_spectrum_set_threshold(int8_t threshold_dbm) {
  if (threshold_dbm < RF_NOISE_FLOOR) {
    threshold_dbm = RF_NOISE_FLOOR;
  } else if (threshold_dbm > -20) {
    threshold_dbm = -20;
  }
  s_data.threshold_dbm = threshold_dbm;
  save_preferences();
  ESP_LOGI(TAG, "Threshold updated to: %d dBm", s_data.threshold_dbm);
}

int8_t rf_spectrum_get_threshold(void) {
  return s_data.threshold_dbm;
}

void rf_spectrum_set_channel(uint8_t channel) {
  if (!rf_channel_is_indexable(channel)) {
    ESP_LOGW(TAG, "Channel %u is outside the 1..%d range, ignoring",
             channel, RF_MAX_CHANNELS);
    return;
  }
  s_data.current_channel = channel;
  save_preferences();
}

uint8_t rf_spectrum_get_channel(void) {
  return s_data.current_channel;
}

void rf_spectrum_set_audio_enabled(bool enabled) {
  s_data.audio_enabled = enabled;
  save_preferences();
  ESP_LOGI(TAG, "Audio Geiger: %s", enabled ? "ENABLED" : "MUTED");
}

bool rf_spectrum_get_audio_enabled(void) {
  return s_data.audio_enabled;
}

void rf_spectrum_toggle_audio(void) {
  rf_spectrum_set_audio_enabled(!s_data.audio_enabled);
}

void rf_spectrum_set_sd_log_enabled(bool enabled) {
  s_data.sd_logging = enabled;
  if (enabled && !s_sd_header_written) {
    s_sd_header_written = false;
  }
  ESP_LOGI(TAG, "SD Card Logging: %s", enabled ? "ENABLED" : "DISABLED");
}

bool rf_spectrum_get_sd_log_enabled(void) {
  return s_data.sd_logging;
}

void rf_spectrum_toggle_sd_log(void) {
  rf_spectrum_set_sd_log_enabled(!s_data.sd_logging);
}

void rf_spectrum_set_protocol(rf_protocol_t proto) {
  s_data.protocol = proto;
  /* Re-tune into the new range, and clamp: the previous channel may sit
     outside the band we are switching to. */
  s_data.current_channel =
      rf_channel_clamp(proto, (proto == RF_PROTOCOL_ZIGBEE) ? 15 : 1);
  ESP_LOGI(TAG, "Protocol switched to: %s", (proto == RF_PROTOCOL_ZIGBEE) ? "Zigbee/802.15.4" : "Wi-Fi 2.4GHz");
}

rf_protocol_t rf_spectrum_get_protocol(void) {
  return s_data.protocol;
}

uint16_t rf_spectrum_get_deauth_count(uint8_t channel) {
  if (channel == 0) {
    uint16_t total = 0;
    for (int c = 1; c <= RF_MAX_CHANNELS; c++) {
      total += s_data.deauth_count[c];
    }
    return total;
  }
  if (channel <= RF_MAX_CHANNELS) {
    return s_data.deauth_count[channel];
  }
  return 0;
}

bool rf_spectrum_has_jammer_alert(uint8_t channel) {
  if (channel == 0) {
    for (int c = 1; c <= RF_MAX_CHANNELS; c++) {
      if (s_data.jammer_alert[c]) return true;
    }
    return false;
  }
  if (channel <= RF_MAX_CHANNELS) {
    return s_data.jammer_alert[channel];
  }
  return false;
}

void rf_spectrum_set_tri_channel_enabled(bool enabled) {
  s_data.tri_channel_sweep = enabled;
  save_preferences();
}

bool rf_spectrum_get_tri_channel_enabled(void) {
  return s_data.tri_channel_sweep;
}

void rf_spectrum_toggle_tri_channel(void) {
  rf_spectrum_set_tri_channel_enabled(!s_data.tri_channel_sweep);
}

void rf_spectrum_clear_alerts(void) {
  memset((void*)s_data.deauth_count, 0, sizeof(s_data.deauth_count));
  memset((void*)s_data.drone_count, 0, sizeof(s_data.drone_count));
  memset((void*)s_data.jammer_alert, 0, sizeof(s_data.jammer_alert));
  s_data.last_drone_id[0] = '\0';
  s_data.alert_timer = 0;
  s_data.alert_msg[0] = '\0';
}

void rf_spectrum_set_pcap_enabled(bool enabled) {
  if (enabled == s_data.pcap_logging) return;
  if (enabled) {
    if (rf_spectrum_pcap_start()) {
      s_data.pcap_logging = true;
      s_data.pcap_packets = 0;
      ESP_LOGI(TAG, "PCAP logging enabled");
    } else {
      ESP_LOGW(TAG, "Cannot start PCAP logging: MicroSD not mounted or file error");
    }
  } else {
    rf_spectrum_pcap_stop();
    s_data.pcap_logging = false;
    ESP_LOGI(TAG, "PCAP logging stopped");
  }
}

bool rf_spectrum_get_pcap_enabled(void) {
  return s_data.pcap_logging;
}

void rf_spectrum_toggle_pcap(void) {
  rf_spectrum_set_pcap_enabled(!s_data.pcap_logging);
}

uint16_t rf_spectrum_get_drone_count(uint8_t channel) {
  if (channel == 0) {
    uint16_t total = 0;
    for (int c = 1; c <= RF_MAX_CHANNELS; c++) {
      total += s_data.drone_count[c];
    }
    return total;
  }
  if (channel <= RF_MAX_CHANNELS) {
    return s_data.drone_count[channel];
  }
  return 0;
}

const char* rf_spectrum_get_last_drone_id(void) {
  return s_data.last_drone_id;
}

size_t rf_spectrum_get_status_json(char* buf, size_t max_len) {
  if (!buf || max_len == 0) return 0;
  uint16_t total_deauth = rf_spectrum_get_deauth_count(0);
  uint16_t total_drones = rf_spectrum_get_drone_count(0);
  bool has_jam = rf_spectrum_has_jammer_alert(0);
  uint8_t cur = rf_channel_clamp(s_data.protocol, s_data.current_channel);
  size_t written = 0;
  /* Truncated at worst case: returns the length that *would* have been
     written, so callers must compare against the buffer size rather than
     trusting the return value. */
  size_t space = max_len - 1;

#define JSON_APPEND(...)                              \
  do {                                                \
    int _n = snprintf(buf + written, space - written,  \
                      __VA_ARGS__);                   \
    if (_n < 0) return written;                       \
    written += (size_t) _n;                           \
    if (written > space) { written = space; goto out; } \
  } while (0)

  JSON_APPEND(
    "{\"ch\":%d,\"rssi\":%d,\"peak\":%d,\"th\":%d,\"audio\":%s,\"sd\":%s,"
    "\"pcap\":%s,\"pcap_pkts\":%u,\"proto\":\"%s\",\"view\":%d,\"deauth\":%u,"
    "\"drones\":%u,\"last_drone\":\"%s\",\"jammer\":%s,\"alert\":\"%s\","
    "\"channels\":[",
    (int) cur, s_data.channel_rssi[cur], s_data.channel_peak[cur],
    s_data.threshold_dbm, s_data.audio_enabled ? "true" : "false",
    s_data.sd_logging ? "true" : "false",
    s_data.pcap_logging ? "true" : "false", (unsigned int)s_data.pcap_packets,
    (s_data.protocol == RF_PROTOCOL_ZIGBEE) ? "Zigbee" : "Wi-Fi",
    s_data.view_mode, total_deauth, total_drones, s_data.last_drone_id,
    has_jam ? "true" : "false", s_data.alert_msg);

  for (int c = 1; c <= RF_MAX_CHANNELS; c++) {
    JSON_APPEND("%s%d", (c == 1) ? "" : ",", s_data.channel_rssi[c]);
  }
  JSON_APPEND("]}");

#undef JSON_APPEND
out:
  buf[written] = '\0';
  return written;
}


#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define RF_NOISE_FLOOR  -95
#define RF_MAX_CHANNELS 14

typedef enum { RF_PROTOCOL_WIFI = 0, RF_PROTOCOL_ZIGBEE = 1 } rf_protocol_t;

static inline void rf_spectrum_set_threshold(int8_t th) {
  (void) th;
}
static inline int8_t rf_spectrum_get_threshold(void) {
  return -85;
}
static inline void rf_spectrum_set_channel(uint8_t ch) {
  (void) ch;
}
static inline uint8_t rf_spectrum_get_channel(void) {
  return 1;
}

static inline void rf_spectrum_set_audio_enabled(bool en) {
  (void) en;
}
static inline bool rf_spectrum_get_audio_enabled(void) {
  return true;
}
static inline void rf_spectrum_toggle_audio(void) {}

static inline void rf_spectrum_set_sd_log_enabled(bool en) {
  (void) en;
}
static inline bool rf_spectrum_get_sd_log_enabled(void) {
  return false;
}
static inline void rf_spectrum_toggle_sd_log(void) {}

static inline void rf_spectrum_set_protocol(rf_protocol_t p) {
  (void) p;
}
static inline rf_protocol_t rf_spectrum_get_protocol(void) {
  return RF_PROTOCOL_WIFI;
}

static inline size_t rf_spectrum_get_status_json(char* b, size_t m) {
  (void) b;
  (void) m;
  return 0;
}
static inline uint16_t rf_spectrum_get_deauth_count(uint8_t ch) {
  (void) ch;
  return 0;
}
static inline bool rf_spectrum_has_jammer_alert(uint8_t ch) {
  (void) ch;
  return false;
}
static inline void rf_spectrum_clear_alerts(void) {}
static inline void rf_spectrum_set_pcap_enabled(bool en) {
  (void) en;
}
static inline bool rf_spectrum_get_pcap_enabled(void) {
  return false;
}
static inline void rf_spectrum_toggle_pcap(void) {}
static inline uint16_t rf_spectrum_get_drone_count(uint8_t ch) {
  (void) ch;
  return 0;
}
static inline const char* rf_spectrum_get_last_drone_id(void) {
  return "";
}

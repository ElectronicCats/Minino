#pragma once
#include <stdint.h>
typedef void (*ieee_sniffer_cb_t)(uint8_t* packet, uint8_t packet_len);
static inline void ieee_sniffer_register_cb(ieee_sniffer_cb_t cb) {
  (void) cb;
}
static inline void ieee_sniffer_set_channel(uint8_t ch) {
  (void) ch;
}
static inline void ieee_sniffer_begin(void* arg) {
  (void) arg;
}
static inline void ieee_sniffer_stop(void) {}

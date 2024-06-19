#pragma once
#include <stdint.h>
typedef enum {
  RADIO_SELECT_ZIGBEE_SWITCH,
  RADIO_SELECT_ZIGBEE_SNIFFER,
  RADIO_SELECT_THREAD,
} radio_select_options_t;

uint8_t radio_selector_get_selected_option();
void radio_selector_set_zigbee_switch();
void radio_selector_set_zigbee_sniffer();
void radio_selector_set_thread();

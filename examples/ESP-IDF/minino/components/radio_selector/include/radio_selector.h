#pragma once
#include <stdbool.h>
typedef enum {
  RADIO_SELECT_THREAD,
  RADIO_SELECT_ZIGBEE_SWITCH,
  RADIO_SELECT_ZIGBEE_SNIFFER
} radio_select_options_t;

void radio_selector_enable_thread();
void radio_selector_disable_thread();
bool radio_selector_is_thread_enabled();

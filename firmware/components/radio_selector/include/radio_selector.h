#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef enum {
  RADIO_SELECT_ZIGBEE_SWITCH,
  RADIO_SELECT_ZIGBEE_SNIFFER,
  RADIO_SELECT_THREAD,
  RADIO_SELECT_ZIGBEE_LIGHT,
  RADIO_SELECT_SURVEILLANCE,
} radio_select_options_t;

uint8_t radio_selector_get_selected_option();
void radio_selector_set_zigbee_switch();
void radio_selector_set_zigbee_sniffer();
void radio_selector_set_thread();
void radio_selector_set_zigbee_light();
void radio_selector_set_surveillance();
bool radio_selector_is_platform_configured();
void radio_selector_set_platform_configured(bool configured);
bool radio_selector_is_stack_initialized();
void radio_selector_set_stack_initialized(bool initialized);

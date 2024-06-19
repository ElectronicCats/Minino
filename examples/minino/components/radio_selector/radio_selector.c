#include "radio_selector.h"

uint8_t radio_selected_option;
uint8_t radio_selector_get_selected_option() {
  return radio_selected_option;
}
void radio_selector_set_zigbee_switch() {
  radio_selected_option = RADIO_SELECT_ZIGBEE_SWITCH;
}
void radio_selector_set_zigbee_sniffer() {
  radio_selected_option = RADIO_SELECT_ZIGBEE_SNIFFER;
}
void radio_selector_set_thread() {
  radio_selected_option = RADIO_SELECT_THREAD;
}

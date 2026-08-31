#include "radio_selector.h"

#include "esp_attr.h"
#include "esp_ieee802154.h"

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
void radio_selector_set_zigbee_light() {
  radio_selected_option = RADIO_SELECT_ZIGBEE_LIGHT;
}
void radio_selector_set_surveillance() {
  radio_selected_option = RADIO_SELECT_SURVEILLANCE;
}
static bool platform_configured = false;
static bool stack_initialized = false;
bool radio_selector_is_platform_configured() {
  return platform_configured;
}
void radio_selector_set_platform_configured(bool configured) {
  platform_configured = configured;
}
bool radio_selector_is_stack_initialized() {
  return stack_initialized;
}
void radio_selector_set_stack_initialized(bool initialized) {
  stack_initialized = initialized;
}

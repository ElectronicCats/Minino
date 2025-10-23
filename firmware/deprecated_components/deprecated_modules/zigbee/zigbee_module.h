#ifndef ZIGBEE_MODULE_H
#define ZIGBEE_MODULE_H
#define TAG_ZIGBEE_MODULE "zigbee_module:main"
/**
 * @brief Begin the Zigbee module
 *
 * @param app_selected The selected app
 */
void zigbee_module_begin(int app_selected);

void zigbee_module_switch_enter();

void zigbee_module_sniffer_enter();

#endif  // ZIGBEE_MODULE_H

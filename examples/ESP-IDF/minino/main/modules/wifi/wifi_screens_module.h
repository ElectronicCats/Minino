#include "esp_wifi.h"
#ifndef WIFI_SCREENS_MODULE_H
  #define WIFI_SCREENS_MODULE_H
  #define TAG_WIFI_SCREENS_MODULE "module:wifi_screens"

/**
 * @brief Display the wifi module scanning screen
 *
 */
void wifi_screens_module_scanning(void);

/**
 * @brief Display the wifi module scanned screen
 *
 * @param ap_records The pointer to the scanned AP records
 */
void wifi_screens_module_animate_attacking(wifi_ap_record_t* ap_record);

/**
 * @brief Display the wifi module scanned screen
 *
 * @param ap_records The pointer to the scanned AP records
 * @param scanned_records The number of scanned records
 * @param current_option The current option selected
 */
void wifi_screens_module_display_scanned_networks(wifi_ap_record_t* ap_records,
                                                  int scanned_records,
                                                  int current_option);

/**
 * @brief Display the wifi module details screen
 *
 * @param ap_record The pointer to the AP record
 * @param page The page to display
 */
void wifi_screens_module_display_details_network(wifi_ap_record_t* ap_record,
                                                 int page);

/**
 * @brief Display the wifi module attack selector screen
 *
 * @param attack_options The list of attack options
 * @param list_count The number of options
 * @param current_option The current option selected
 */
void wifi_screens_module_display_attack_selector(char* attack_options[],
                                                 int list_count,
                                                 int current_option);

void wifi_screens_module_display_captive_pass(char* ssid,
                                              char* user,
                                              char* pass);

void wifi_screens_module_display_captive_user_pass(char* ssid,
                                                   char* user,
                                                   char* pass);

/**
 * @brief Display the wifi module portals selector screen
 *
 * @param attack_options The list of portals options
 * @param list_count The number of options
 * @param current_option The current option selected
 */
void wifi_screens_module_display_captive_selector(char* attack_options[],
                                                  int list_count,
                                                  int current_option);
#endif  // WIFI_SCREENS_MODULE_H

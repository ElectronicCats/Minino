#pragma once

#include <stdbool.h>
#include "wifi_ap_config.h"

#define JOIN_TIMEOUT_MS (5000)
#define MAXIMUM_RETRY   (3)

typedef void (*app_callback)(bool state);

void wifi_ap_manager_deinit();
int wifi_ap_manager_connect_ap(const char* ssid, const char* pass);
int wifi_ap_manager_connect_index(int index);
void wifi_ap_manager_unregister_callback();
int wifi_ap_manager_connect_index_cb(int index, app_callback cb);
int wifi_ap_manager_delete_ap_by_index(int index);
bool wifi_ap_manager_is_connect();
void wifi_ap_manager_list_aps();
int wifi_ap_manager_get_count();
void wifi_ap_manager_get_aps(char** aps_list);

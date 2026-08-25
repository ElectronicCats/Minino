#pragma once

#include "wifi_ap_config.h"
#include "esp_netif.h"

extern esp_netif_t* esp_netif_sta;
extern esp_netif_t* esp_netif_ap;

void wifi_ap_init(void);

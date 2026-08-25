#pragma once

#include "esp_netif.h"
#include "wifi_ap_config.h"

extern esp_netif_t* esp_netif_sta;
extern esp_netif_t* esp_netif_ap;

void wifi_ap_init(void);

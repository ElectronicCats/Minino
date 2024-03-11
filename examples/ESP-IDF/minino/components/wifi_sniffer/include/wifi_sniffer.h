#ifndef WIFI_SNIFFER_H
#define WIFI_SNIFFER_H

#include <time.h>
#include "esp_event.h"
#include "esp_wifi.h"

#define SSID_MAX_LEN (32 + 1)  // max length of a SSID
#define MD5_LEN (32 + 1)       // length of md5 hash
#define CHANNEL_MAX 13         // US = 11, EU = 13, JP = 14

typedef struct {
    uint8_t channel;
    uint8_t addr[6];
    char* ssid;
    time_t timestamp;
    char* hash;
    int8_t rssi;
    uint8_t sn;
    char htci[5];

} wifi_sniffer_record_t;

typedef void (*wifi_sniffer_cb_t)(wifi_sniffer_record_t record);

void wifi_sniffer_init(void);
void wifi_sniffer_deinit(void);
void wifi_sniffer_task(void* pvParameters);
void wifi_sniffer_register_cb(wifi_sniffer_cb_t cb);

uint8_t wifi_sniffer_get_channel(void);

#endif
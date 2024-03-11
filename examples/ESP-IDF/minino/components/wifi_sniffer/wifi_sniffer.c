#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <arpa/inet.h>
#include "esp_intr_alloc.h"
#include "md5.h"
#include "nvs_flash.h"
#include "wifi_sniffer.h"

static wifi_sniffer_cb_t wifi_sniffer_cb = NULL;
int channel_current = 1;
static const char* TAG = "wifi_sniffer";
TaskHandle_t wifi_sniffer_task_handle;

const wifi_promiscuous_filter_t wifi_filter = {  // Idk what this does
    .filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_DATA};

typedef struct {
    int16_t fctl;             // frame control
    int16_t duration;         // duration id
    uint8_t da[6];            // receiver address
    uint8_t sa[6];            // sender address
    uint8_t bssid[6];         // filtering address
    int16_t seqctl;           // sequence control
    unsigned char payload[];  // network data
} __attribute__((packed)) wifi_mgmt_hdr;

static void wifi_sniffer_run(void* buff, wifi_promiscuous_pkt_type_t type);

void wifi_sniffer_init(void) {
    nvs_flash_init();

    /* setup wifi */
    ESP_LOGI(TAG, "Setting up wifi");
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_storage(WIFI_STORAGE_RAM);
    esp_wifi_set_mode(WIFI_MODE_NULL);
    esp_wifi_start();
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_promiscuous_filter(&wifi_filter);
    esp_wifi_set_promiscuous_rx_cb(&wifi_sniffer_run);
    esp_wifi_set_channel(channel_current, WIFI_SECOND_CHAN_NONE);
    // Start the wifi sniffer task
    xTaskCreate(&wifi_sniffer_task, "wifi_sniffer_task", 2048, NULL, 5, &wifi_sniffer_task_handle);
}

void wifi_sniffer_deinit(void) {
    esp_wifi_set_promiscuous(false);
    esp_wifi_stop();
    esp_wifi_deinit();
    // Stop the wifi sniffer task
    vTaskDelete(wifi_sniffer_task_handle);
}

void wifi_sniffer_task(void* pvParameter) {
    while (true) {
        wifi_sniffer_record_t record = {
            .channel = channel_current,
            .ssid = "",
            .rssi = 0,
            .addr = {0},
            .hash = "",
            .htci = "",
            .sn = 0,
            .timestamp = 0,
        };

        wifi_sniffer_cb(record);

        printf("Channel: %d\n", channel_current);
        if (channel_current > CHANNEL_MAX) {
            channel_current = 1;
        }
        esp_wifi_set_channel(channel_current++, WIFI_SECOND_CHAN_NONE);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void wifi_sniffer_register_cb(wifi_sniffer_cb_t cb) {
    wifi_sniffer_cb = cb;
    ESP_LOGI(TAG, "Registered callback");
}

static void get_hash(unsigned char* data, int len_res, char hash[MD5_LEN]) {
    uint8_t pkt_hash[16];

    md5((uint8_t*)data, len_res, pkt_hash);

    ESP_LOGI(TAG,
                "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
                pkt_hash[0], pkt_hash[1], pkt_hash[2], pkt_hash[3], pkt_hash[4],
                pkt_hash[5], pkt_hash[6], pkt_hash[7], pkt_hash[8], pkt_hash[9],
                pkt_hash[10], pkt_hash[11], pkt_hash[12], pkt_hash[13], pkt_hash[14],
                pkt_hash[15]);
}

static void get_ssid(unsigned char* data, char ssid[SSID_MAX_LEN], uint8_t ssid_len) {
    int i, j;

    for (i = 26, j = 0; i < 26 + ssid_len; i++, j++) {
        ssid[j] = data[i];
    }

    ssid[j] = '\0';
}

static int get_sn(unsigned char* data) {
    int sn;
    char num[5] = "\0";

    ESP_LOGI(TAG, "%02x%02x", data[22], data[23]);
    sscanf(num, "%x", &sn);

    return sn;
}

static void get_ht_capabilites_info(unsigned char* data, char htci[5], int pkt_len, int ssid_len) {
    int ht_start = 25 + ssid_len + 19;

    if (data[ht_start - 1] > 0 &&
        ht_start < pkt_len - 4) {  // HT capabilities is present
        if (data[ht_start - 4] ==
            1)  // DSSS parameter is set -> need to shift of three bytes
                ESP_LOGI(TAG, "%02x%02x", data[ht_start + 3], data[ht_start + 1 + 3]);
            else
                ESP_LOGI(TAG, "%02x%02x", data[ht_start], data[ht_start + 1]);
    }
}

static void wifi_sniffer_run(void* buff, wifi_promiscuous_pkt_type_t type) {
    int pkt_len, fc, sn = 0;
    char ssid[SSID_MAX_LEN] = "\0", hash[MD5_LEN] = "\0", htci[5] = "\0";
    uint8_t ssid_len;
    time_t ts;

    wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buff;
    wifi_mgmt_hdr* mgmt = (wifi_mgmt_hdr*)pkt->payload;

    fc = ntohs(mgmt->fctl);

    if ((fc & 0xFF00) == 0x4000) {  // only look for probe request packets
        time(&ts);

        ssid_len = pkt->payload[25];
        if (ssid_len > 0)
            get_ssid(pkt->payload, ssid, ssid_len);

        pkt_len = pkt->rx_ctrl.sig_len;
        get_hash(pkt->payload, pkt_len - 4, hash);

        sn = get_sn(pkt->payload);

        get_ht_capabilites_info(pkt->payload, htci, pkt_len, ssid_len);

        // ESP_LOGI(TAG, "ADDR=%02x:%02x:%02x:%02x:%02x:%02x, "
        //     "SSID=%s, "
        //     "TIMESTAMP=%d, "
        //     "HASH=%s, "
        //     "RSSI=%02d, "
        //     "SN=%d, "
        //     "HT CAP. INFO=%s",
        //     mgmt->sa[0], mgmt->sa[1], mgmt->sa[2], mgmt->sa[3], mgmt->sa[4],
        //     mgmt->sa[5], ssid, (int)ts, hash, pkt->rx_ctrl.rssi, sn, htci);

        wifi_sniffer_record_t record = {
            .channel = channel_current,
            .ssid = ssid,
            .rssi = pkt->rx_ctrl.rssi,
            .addr = {mgmt->sa[0], mgmt->sa[1], mgmt->sa[2], mgmt->sa[3], mgmt->sa[4], mgmt->sa[5]},
            .hash = hash,
            .htci = {htci[0], htci[1], htci[2], htci[3], htci[4]},
            .sn = sn,
            .timestamp = ts,
        };

        wifi_sniffer_cb(record);
        // vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

uint8_t wifi_sniffer_get_channel(void) {
    return channel_current;
}

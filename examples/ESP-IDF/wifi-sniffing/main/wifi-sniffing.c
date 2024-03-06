#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
// #include "esp_netif.h"
#include "esp_intr_alloc.h"
#include "nvs_flash.h"
#include "md5.h"
#include <arpa/inet.h>

#define SSID_MAX_LEN (32 + 1) // max length of a SSID
#define MD5_LEN (32 + 1)      // length of md5 hash

// US = 11, EU = 13, JP = 14
#define CHANNEL_MAX 13
int channel_current = 1;

const wifi_promiscuous_filter_t wifi_filter = { // Idk what this does
    .filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_DATA};

typedef struct {
  int16_t fctl;            // frame control
  int16_t duration;        // duration id
  uint8_t da[6];           // receiver address
  uint8_t sa[6];           // sender address
  uint8_t bssid[6];        // filtering address
  int16_t seqctl;          // sequence control
  unsigned char payload[]; // network data
} __attribute__((packed)) wifi_mgmt_hdr;

static void get_hash(unsigned char *data, int len_res, char hash[MD5_LEN]) {
  uint8_t pkt_hash[16];

  md5((uint8_t *)data, len_res, pkt_hash);

  sprintf(hash,
          "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
          pkt_hash[0], pkt_hash[1], pkt_hash[2], pkt_hash[3], pkt_hash[4],
          pkt_hash[5], pkt_hash[6], pkt_hash[7], pkt_hash[8], pkt_hash[9],
          pkt_hash[10], pkt_hash[11], pkt_hash[12], pkt_hash[13], pkt_hash[14],
          pkt_hash[15]);
}

static void get_ssid(unsigned char *data, char ssid[SSID_MAX_LEN],
                     uint8_t ssid_len) {
  int i, j;

  for (i = 26, j = 0; i < 26 + ssid_len; i++, j++) {
    ssid[j] = data[i];
  }

  ssid[j] = '\0';
}

static int get_sn(unsigned char *data) {
  int sn;
  char num[5] = "\0";

  sprintf(num, "%02x%02x", data[22], data[23]);
  sscanf(num, "%x", &sn);

  return sn;
}

static void get_ht_capabilites_info(unsigned char *data, char htci[5],
                                    int pkt_len, int ssid_len) {
  int ht_start = 25 + ssid_len + 19;

  if (data[ht_start - 1] > 0 &&
      ht_start < pkt_len - 4) { // HT capabilities is present
    if (data[ht_start - 4] ==
        1) // DSSS parameter is set -> need to shift of three bytes
      sprintf(htci, "%02x%02x", data[ht_start + 3], data[ht_start + 1 + 3]);
    else
      sprintf(htci, "%02x%02x", data[ht_start], data[ht_start + 1]);
  }
}

static void sniffer(void *buff, wifi_promiscuous_pkt_type_t type) {
  int pkt_len, fc, sn = 0;
  char ssid[SSID_MAX_LEN] = "\0", hash[MD5_LEN] = "\0", htci[5] = "\0";
  uint8_t ssid_len;
  time_t ts;

  wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *)buff;
  wifi_mgmt_hdr *mgmt = (wifi_mgmt_hdr *)pkt->payload;

  fc = ntohs(mgmt->fctl);

  if ((fc & 0xFF00) == 0x4000) { // only look for probe request packets
    time(&ts);

    ssid_len = pkt->payload[25];
    if (ssid_len > 0)
      get_ssid(pkt->payload, ssid, ssid_len);

    pkt_len = pkt->rx_ctrl.sig_len;
    get_hash(pkt->payload, pkt_len - 4, hash);

    sn = get_sn(pkt->payload);

    get_ht_capabilites_info(pkt->payload, htci, pkt_len, ssid_len);

    char msg[256];
    sprintf(msg,
            "ADDR=%02x:%02x:%02x:%02x:%02x:%02x, "
            "SSID=%s, "
            "TIMESTAMP=%d, "
            "HASH=%s, "
            "RSSI=%02d, "
            "SN=%d, "
            "HT CAP. INFO=%s",
            mgmt->sa[0], mgmt->sa[1], mgmt->sa[2], mgmt->sa[3], mgmt->sa[4],
            mgmt->sa[5], ssid, (int)ts, hash, pkt->rx_ctrl.rssi, sn, htci);
    printf("%s\n", msg);
  }
}

void app_main(void) {
  nvs_flash_init();

  /* setup wifi */
  printf("Setting up wifi\n");
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&cfg);
  esp_wifi_set_storage(WIFI_STORAGE_RAM);
  esp_wifi_set_mode(WIFI_MODE_NULL);
  esp_wifi_start();
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_filter(&wifi_filter);
  esp_wifi_set_promiscuous_rx_cb(&sniffer);
  esp_wifi_set_channel(channel_current, WIFI_SECOND_CHAN_NONE);

  while (true) {
    printf("channel: %d\n", channel_current);
    if (channel_current > CHANNEL_MAX) {
      channel_current = 1;
    }
    esp_wifi_set_channel(channel_current++, WIFI_SECOND_CHAN_NONE);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

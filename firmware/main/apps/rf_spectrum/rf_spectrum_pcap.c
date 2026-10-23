/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MicroSD PCAP Streaming Engine for Minino RF Spectrum.
 */

#include "rf_spectrum_pcap.h"
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "files_ops.h"
#include "sd_card.h"

static const char* TAG = "rf_spectrum_pcap";

/* Standard libpcap global file header format */
typedef struct __attribute__((packed)) {
  uint32_t magic_number;   /* 0xa1b2c3d4 */
  uint16_t version_major;  /* 2 */
  uint16_t version_minor;  /* 4 */
  int32_t  thiszone;       /* GMT to local correction: 0 */
  uint32_t sigfigs;        /* accuracy of timestamps: 0 */
  uint32_t snaplen;        /* max length of captured packets: 65535 */
  uint32_t network;        /* data link type: 105 (IEEE 802.11) */
} pcap_file_header_t;

/* Standard libpcap packet record header */
typedef struct __attribute__((packed)) {
  uint32_t ts_sec;         /* timestamp seconds */
  uint32_t ts_usec;        /* timestamp microseconds */
  uint32_t incl_len;       /* number of octets of packet saved in file */
  uint32_t orig_len;       /* actual length of packet */
} pcap_packet_header_t;

static FILE* s_pcap_file = NULL;
static bool s_pcap_active = false;
static uint32_t s_packet_count = 0;
static char s_current_filename[PCAP_PATH_MAX] = {0};

esp_err_t rf_spectrum_pcap_start(void) {
  if (s_pcap_active) {
    return ESP_OK;
  }

  /* Verify SD card is mounted; attempt mount if not */
  if (!sd_card_is_mounted()) {
    esp_err_t mount_err = sd_card_mount();
    if (mount_err != ESP_OK) {
      ESP_LOGW(TAG, "Cannot start PCAP: MicroSD not mounted (%s)", esp_err_to_name(mount_err));
      return ESP_ERR_NOT_FOUND;
    }
  }

  /* Obtain incremental filename: /sdcard/spectrum_00.pcap, spectrum_01.pcap... */
  if (!files_ops_incremental_name_buf("/sdcard", "spectrum", ".pcap", s_current_filename, sizeof(s_current_filename))) {
    ESP_LOGE(TAG, "Failed to generate incremental PCAP filename");
    return ESP_FAIL;
  }

  s_pcap_file = fopen(s_current_filename, "wb");
  if (!s_pcap_file) {
    ESP_LOGE(TAG, "Failed to open '%s' for writing", s_current_filename);
    return ESP_FAIL;
  }

  /* Write PCAP 24-byte global header with 802.11 link type */
  pcap_file_header_t hdr = {
      .magic_number = 0xa1b2c3d4,
      .version_major = 2,
      .version_minor = 4,
      .thiszone = 0,
      .sigfigs = 0,
      .snaplen = 65535,
      .network = 105 /* LINKTYPE_IEEE802_11 */
  };

  if (fwrite(&hdr, sizeof(hdr), 1, s_pcap_file) != 1) {
    ESP_LOGE(TAG, "Failed to write PCAP global header to '%s'", s_current_filename);
    fclose(s_pcap_file);
    s_pcap_file = NULL;
    return ESP_FAIL;
  }

  fflush(s_pcap_file);
  s_packet_count = 0;
  s_pcap_active = true;

  ESP_LOGI(TAG, "PCAP recording started: %s (Link Type 802.11)", s_current_filename);
  return ESP_OK;
}

void rf_spectrum_pcap_stop(void) {
  if (!s_pcap_active && !s_pcap_file) {
    return;
  }

  if (s_pcap_file) {
    fflush(s_pcap_file);
    fclose(s_pcap_file);
    s_pcap_file = NULL;
  }

  s_pcap_active = false;
  ESP_LOGI(TAG, "PCAP recording stopped: %s (%lu packets saved)",
           s_current_filename, (unsigned long)s_packet_count);
}

bool rf_spectrum_pcap_is_active(void) {
  return s_pcap_active;
}

void rf_spectrum_pcap_write_packet(const void* payload, uint32_t len) {
  if (!s_pcap_active || !s_pcap_file || !payload || len == 0) {
    return;
  }

  int64_t now_us = esp_timer_get_time();
  pcap_packet_header_t pkt_hdr = {
      .ts_sec = (uint32_t)(now_us / 1000000ULL),
      .ts_usec = (uint32_t)(now_us % 1000000ULL),
      .incl_len = len,
      .orig_len = len
  };

  if (fwrite(&pkt_hdr, sizeof(pkt_hdr), 1, s_pcap_file) == 1) {
    if (fwrite(payload, len, 1, s_pcap_file) == 1) {
      s_packet_count++;
      /* Flush periodically every 50 packets to guarantee data durability */
      if ((s_packet_count % 50) == 0) {
        fflush(s_pcap_file);
      }
    }
  }
}

uint32_t rf_spectrum_pcap_get_packet_count(void) {
  return s_packet_count;
}

const char* rf_spectrum_pcap_get_filename(void) {
  return s_current_filename;
}

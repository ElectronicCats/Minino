/* cmd_pcap example — declarations of command registration functions.

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#pragma once

#include "pcap.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Pcap File Header
 *
 */
typedef struct {
  uint32_t magic;     /*!< Magic Number */
  uint16_t major;     /*!< Major Version */
  uint16_t minor;     /*!< Minor Version */
  uint32_t zone;      /*!< Time Zone Offset */
  uint32_t sigfigs;   /*!< Timestamp Accuracy */
  uint32_t snaplen;   /*!< Max Length to Capture */
  uint32_t link_type; /*!< Link Layer Type */
} pcap_file_header_t;

/**
 * @brief Pcap Packet Header
 *
 */
typedef struct {
  uint32_t
      seconds; /*!< Number of seconds since January 1st, 1970, 00:00:00 GMT */
  uint32_t microseconds;   /*!< Number of microseconds when the packet was
                              captured (offset from seconds) */
  uint32_t capture_length; /*!< Number of bytes of captured data, no longer than
                              packet_length */
  uint32_t packet_length;  /*!< Actual length of current packet */
} pcap_packet_header_t;

/**
 * @brief Pcap Runtime Handle
 *
 */
struct pcap_file_t {
  FILE* file;                 /*!< File handle */
  pcap_link_type_t link_type; /*!< Pcap Link Type */
  unsigned int major_version; /*!< Pcap version: major */
  unsigned int minor_version; /*!< Pcap version: minor */
  unsigned int time_zone;     /*!< Pcap timezone code */
  uint32_t endian_magic;      /*!< Magic value related to endian format */
};

typedef void (*summary_cb_t)(FILE* pcap_file);

/**
 * @brief Register summary callback
 *
 * @param cb callback function
 */
void wifi_sniffer_register_summary_cb(summary_cb_t cb);

/**
 * @brief Get the file size
 *
 * @param file file pointer
 *
 * @return long
 */
long pcap_cmd_get_file_size(FILE* file);

/**
 * @brief Capture a pcap package with parameters
 *
 * @param payload pointer of the captured data
 * @param length length of captured data
 * @param seconds second of capture time
 * @param microseconds microsecond of capture time
 * @return esp_err_t
 *      - ESP_OK on success
 *      - ESP_FAIL on error
 */
esp_err_t packet_capture(void* payload,
                         uint32_t length,
                         uint32_t seconds,
                         uint32_t microseconds);

/**
 * @brief Tell the pcap component to start sniff and write
 *
 * @param link_type link type of the captured package
 * @return esp_err_t
 *      - ESP_OK on success
 *      - ESP_FAIL on error
 */
esp_err_t sniff_packet_start(pcap_link_type_t link_type);

/**
 * @brief Tell the pcap component to stop sniff
 *
 * @return esp_err_t
 *      - ESP_OK on success
 *      - ESP_FAIL on error
 */
esp_err_t sniff_packet_stop(void);

/**
 * @brief Register pcap command
 *
 */
void register_pcap_cmd(void);

/**
 * @brief Do pcap command
 *
 */
int do_pcap_cmd(int argc, char** argv);

#ifdef __cplusplus
}
#endif

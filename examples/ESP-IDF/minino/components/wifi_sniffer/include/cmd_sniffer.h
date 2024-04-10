/* cmd_sniffer example — declarations of command registration functions.

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdbool.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Supported Sniffer Interface
 *
 */
typedef enum {
  SNIFFER_INTF_UNKNOWN = 0,
  SNIFFER_INTF_WLAN, /*!< WLAN interface */
  SNIFFER_INTF_ETH,  /*!< Ethernet interface */
} sniffer_intf_t;

/**
 * @brief WLAN Sniffer Filter
 *
 */
typedef enum {
  SNIFFER_WLAN_FILTER_MGMT = 0, /*!< MGMT */
  SNIFFER_WLAN_FILTER_CTRL,     /*!< CTRL */
  SNIFFER_WLAN_FILTER_DATA,     /*!< DATA */
  SNIFFER_WLAN_FILTER_MISC,     /*!< MISC */
  SNIFFER_WLAN_FILTER_MPDU,     /*!< MPDU */
  SNIFFER_WLAN_FILTER_AMPDU,    /*!< AMPDU */
  SNIFFER_WLAN_FILTER_FCSFAIL,  /*!< When this bit is set, the hardware will
                                   receive packets for which frame check sequence
                                   failed */
  SNIFFER_WLAN_FILTER_MAX
} sniffer_wlan_filter_t;

typedef struct {
  bool is_running;
  sniffer_intf_t interf;
  uint32_t interf_num;
  uint32_t channel;
  uint32_t filter;
  int32_t packets_to_sniff;
  TaskHandle_t task;
  QueueHandle_t work_queue;
  SemaphoreHandle_t sem_task_over;
} sniffer_runtime_t;

typedef void (*sniffer_cb_t)(sniffer_runtime_t* sniffer);
typedef void (*sniffer_animation_cb_t)(void);

void register_sniffer_cmd(void);
int do_sniffer_cmd(int argc, char** argv);
void wifi_sniffer_register_cb(sniffer_cb_t callback);
void wifi_sniffer_register_animation_cbs(sniffer_animation_cb_t start_cb,
                                         sniffer_animation_cb_t stop_cb);

#ifdef __cplusplus
}
#endif

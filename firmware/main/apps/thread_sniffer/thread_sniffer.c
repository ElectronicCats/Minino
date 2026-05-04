#include "thread_sniffer.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "uart_sender.h"

#include "files_ops.h"
#include "flash_fs.h"
#include "pcap.h"
#include "sd_card.h"

#define THREAD_SNIFFER_PCAP_LINKTYPE 230

#define PCAP_FILE_NAME_MAX_LEN  32
#define PCAP_MEMORY_BUFFER_SIZE 4096

#define THREAD_SNIFFER_QUEUE_SIZE                32
#define THREAD_SNIFFER_PROCESS_PACKET_TIMEOUT_MS 100

#define SD_CARD           "/sdcard"
#define FLASH_FS          "/internal"
#define APPS_PATH         "apps"
#define THREAD_PATH       APPS_PATH "/thread"
#define THREAD_PCAPS_PATH THREAD_PATH "/pcaps"

#define TAG "thread_sniffer"

typedef struct {
  bool is_opened;
  bool is_writing;
  bool link_type_set;
  char filename[PCAP_FILE_NAME_MAX_LEN];
  pcap_file_handle_t pcap_handle;
  pcap_link_type_t link_type;
} thread_pcap_handler_t;

typedef struct {
  void* payload;
  uint32_t length;
  uint32_t seconds;
  uint32_t microseconds;
} sniffer_packet_info_t;

thread_pcap_handler_t thread_pcap = {0};
thread_sniffer_show_event_cb_t thread_sniffer_show_event_cb = NULL;
static QueueHandle_t packet_rx_queue = NULL;
static TaskHandle_t handler_task_handle = NULL;
static uint32_t packets_count = 0;
static uint8_t current_channel = 11;

static esp_err_t pcap_start();
static esp_err_t pcap_stop();
static esp_err_t pcap_capture(void* payload,
                              uint32_t length,
                              uint32_t seconds,
                              uint32_t microseconds);

void on_pcap_receive(const otRadioFrame* aFrame, bool aIsTx, void* aContext);
static void thread_sniffer_show_event(thread_sniffer_events_t event,
                                      void* context);
static void debug_handler_task();

static void create_pcaps_dir() {
  if (sd_card_mount() == ESP_OK) {
    ESP_LOGI(TAG, "SD card mounted, creating pcaps dir");
    sd_card_create_dir(APPS_PATH);
    sd_card_create_dir(THREAD_PATH);
    sd_card_create_dir(THREAD_PCAPS_PATH);
  } else {
    ESP_LOGW(TAG, "SD card not available for pcaps dir creation");
  }
}

void thread_sniffer_init() {
  ESP_LOGI(TAG, "Initializing thread sniffer");
  create_pcaps_dir();
  openthread_init();
  esp_log_level_set("OPENTHREAD", ESP_LOG_NONE);
  packet_rx_queue =
      xQueueCreate(THREAD_SNIFFER_QUEUE_SIZE, sizeof(sniffer_packet_info_t));
  if (packet_rx_queue == NULL) {
    ESP_LOGE(TAG, "Failed to create packet queue");
    return;
  }
  ESP_LOGI(TAG, "Packet queue created (size=%d, item=%d bytes)",
           THREAD_SNIFFER_QUEUE_SIZE, sizeof(sniffer_packet_info_t));
  xTaskCreate(debug_handler_task, "debug_handler_task", 8192, NULL, 20,
              &handler_task_handle);
  ESP_LOGI(TAG, "Handler task created");
}

void thread_sniffer_run() {
  ESP_LOGI(TAG, "Starting thread sniffer");
  pcap_start();
  packets_count = 0;
  thread_sniffer_show_event(THREAD_SNIFFER_START_EV, NULL);
  thread_sniffer_show_event(THREAD_SNIFFER_NEW_PACKET_EV, &packets_count);
  otError err = openthread_enable_promiscous_mode(&on_pcap_receive);
  openthread_set_channel(current_channel);
  if (err != OT_ERROR_NONE) {
    ESP_LOGE(TAG, "Failed to enable promiscuous mode: %d", err);
  } else {
    ESP_LOGI(TAG, "Promiscuous mode enabled on channel %d", current_channel);
  }
}

void thread_sniffer_stop() {
  ESP_LOGI(TAG, "Stopping thread sniffer (total packets: %lu)", packets_count);
  openthread_disable_promiscous_mode();
  sniffer_packet_info_t discarded;
  while (xQueueReceive(packet_rx_queue, &discarded, 0) == pdTRUE) {
    free(discarded.payload);
  }
  pcap_stop();
  thread_sniffer_show_event(THREAD_SNIFFER_STOP_EV, NULL);
}

static void chek_for_fatal_error(esp_err_t err, const char* err_tag) {
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Fatal error: %s (0x%x)", err_tag, err);
    thread_sniffer_show_event(THREAD_SNIFFER_FATAL_ERROR_EV, (void*) err_tag);
  }
}
static void chek_for_fatal_false(bool ok, const char* err_tag) {
  if (!ok) {
    ESP_LOGE(TAG, "Fatal error: %s", err_tag);
    thread_sniffer_show_event(THREAD_SNIFFER_FATAL_ERROR_EV, (void*) err_tag);
  }
}

static esp_err_t pcap_start() {
  FILE* fp = NULL;
  bool save_in_sd = false;
  if (sd_card_mount() == ESP_OK) {
    save_in_sd = true;
    ESP_LOGI(TAG, "Saving PCAP to SD card");
  } else if (flash_fs_mount() == ESP_OK) {
    save_in_sd = false;
    ESP_LOGI(TAG, "Saving PCAP to internal flash");
  } else {
    ESP_LOGE(TAG, "No storage available for PCAP");
    chek_for_fatal_false(false, "FAILED TO CREATE PCAP FILE");
  }

  char* pcap_path = (char*) malloc(100);
  if (!pcap_path) {
    ESP_LOGE(TAG, "Failed to allocate memory for pcap_path");
    return ESP_ERR_NO_MEM;
  }
  char* pcap_dir = (char*) malloc(30);
  if (!pcap_dir) {
    ESP_LOGE(TAG, "Failed to allocate memory for pcap_dir");
    free(pcap_path);
    return ESP_ERR_NO_MEM;
  }
  sprintf(pcap_dir, "%s/%s", SD_CARD, THREAD_PCAPS_PATH);
  files_ops_incremental_name(save_in_sd ? pcap_dir : FLASH_FS, "thread",
                             ".pcap", pcap_path);
  ESP_LOGI(TAG, "PCAP file path: %s", pcap_path);
  fp = fopen(pcap_path, "w");
  chek_for_fatal_false(fp, "open file failed");
  pcap_config_t pcap_cfg = {
      .fp = fp,
      .major_version = PCAP_DEFAULT_VERSION_MAJOR,
      .minor_version = PCAP_DEFAULT_VERSION_MINOR,
      .time_zone = PCAP_DEFAULT_TIME_ZONE_GMT,
  };
  chek_for_fatal_error(pcap_new_session(&pcap_cfg, &thread_pcap.pcap_handle),
                       "pcap init failed");
  thread_pcap.is_opened = true;
  chek_for_fatal_error(
      pcap_write_header(thread_pcap.pcap_handle, THREAD_SNIFFER_PCAP_LINKTYPE),
      "Write header failed");
  fflush(pcap_cfg.fp);
  thread_pcap.is_writing = true;
  ESP_LOGI(TAG, "PCAP session started successfully");

  thread_sniffer_show_event(THREAD_SNIFFER_DESTINATION_EV, &save_in_sd);

  free(pcap_dir);
  free(pcap_path);
  return ESP_OK;
}

static esp_err_t pcap_stop() {
  chek_for_fatal_error(pcap_del_session(thread_pcap.pcap_handle),
                       "stop pcap session failed");
  thread_pcap.is_opened = false;
  thread_pcap.is_writing = false;
  thread_pcap.link_type_set = false;
  thread_pcap.pcap_handle = NULL;
  ESP_LOGI(TAG, "PCAP session stopped");
  return ESP_OK;
}

void on_pcap_receive(const otRadioFrame* aFrame, bool aIsTx, void* aContext) {
  ESP_LOGD(TAG, "on_pcap_receive: len=%d ch=%d ts=%llu tx=%d", aFrame->mLength,
           aFrame->mChannel, aFrame->mInfo.mRxInfo.mTimestamp, aIsTx);

  sniffer_packet_info_t packet_info = {0};

  uint8_t* payload_copy = malloc(aFrame->mLength);
  if (!payload_copy) {
    ESP_LOGE(TAG, "on_pcap_receive: malloc failed for len=%d", aFrame->mLength);
    return;
  }
  memcpy(payload_copy, aFrame->mPsdu, aFrame->mLength);

  packet_info.payload = payload_copy;
  packet_info.length = aFrame->mLength;
  packet_info.seconds = aFrame->mInfo.mRxInfo.mTimestamp / 1000000u;
  packet_info.microseconds = aFrame->mInfo.mRxInfo.mTimestamp % 1000000u;

  if (xQueueSendToBack(packet_rx_queue, &packet_info, 0) != pdTRUE) {
    ESP_LOGW(TAG, "on_pcap_receive: queue full, dropping packet (len=%d)",
             aFrame->mLength);
    free(payload_copy);
  }
}

static esp_err_t pcap_capture(void* payload,
                              uint32_t length,
                              uint32_t seconds,
                              uint32_t microseconds) {
  if (pcap_capture_packet(thread_pcap.pcap_handle, payload, length, seconds,
                          microseconds) != ESP_OK) {
    ESP_LOGE(TAG, "pcap_capture_packet failed (len=%lu)", length);
    return ESP_FAIL;
  }
  return ESP_OK;
}

static void __attribute__((unused)) thread_packet_debug(const otRadioFrame* aFrame) {
  otLogHexDumpInfo info;

  info.mDataBytes = aFrame->mPsdu;
  info.mDataLength = aFrame->mLength;
  info.mTitle = "New Packet";
  info.mIterator = 0;

  printf("\n");

  while (otLogGenerateNextHexDumpLine(&info) == OT_ERROR_NONE) {
    printf("%s\n", info.mLine);
  }
}

static void debug_handler_task() {
  ESP_LOGI(TAG, "Handler task started, waiting for packets...");
  sniffer_packet_info_t packet_info;
  while (xQueueReceive(packet_rx_queue, &packet_info, portMAX_DELAY) !=
         pdFALSE) {
    packets_count++;
    ESP_LOGI(TAG, "Packet #%lu received: len=%lu ts=%lu.%06lu", packets_count,
             packet_info.length, packet_info.seconds, packet_info.microseconds);
    thread_sniffer_show_event(THREAD_SNIFFER_NEW_PACKET_EV, &packets_count);
    if (thread_pcap.is_writing && thread_pcap.pcap_handle != NULL) {
      esp_err_t cap_ret =
          pcap_capture(packet_info.payload, packet_info.length,
                       packet_info.seconds, packet_info.microseconds);
      if (cap_ret != ESP_OK) {
        ESP_LOGW(TAG, "PCAP write failed for packet #%lu", packets_count);
      }
    }
    uart_sender_send_packet(UART_SENDER_PACKET_TYPE_THREAD, packet_info.payload,
                            packet_info.length);
    free(packet_info.payload);
  }
  ESP_LOGE(TAG, "Handler task terminated unexpectedly");
  vTaskDelete(NULL);
}

void thread_sniffer_set_channel(uint8_t channel) {
  current_channel = channel;
  packets_count = 0;
  openthread_set_channel(channel);
  ESP_LOGI(TAG, "Channel set to %d", channel);
}

void thread_sniffer_set_show_event_cb(thread_sniffer_show_event_cb_t cb) {
  thread_sniffer_show_event_cb = cb;
}

static void thread_sniffer_show_event(thread_sniffer_events_t event,
                                      void* context) {
  if (thread_sniffer_show_event_cb != NULL) {
    thread_sniffer_show_event_cb(event, context);
  }
}

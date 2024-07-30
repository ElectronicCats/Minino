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

#define SD_CARD  "/sdcard"
#define FLASH_FS "/internal"

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
static uint32_t packets_count = 0;

static esp_err_t pcap_start();
static esp_err_t pcap_stop();
static esp_err_t pcap_capture(void* payload,
                              uint32_t length,
                              uint32_t seconds,
                              uint32_t microseconds);

void on_pcap_receive(const otRadioFrame* aFrame, bool aIsTx);
static void thread_sniffer_show_event(thread_sniffer_events_t event,
                                      void* context);
static void debug_handler_task();

void thread_sniffer_init() {
  openthread_init();
  esp_log_level_set("OPENTHREAD", ESP_LOG_NONE);
  packet_rx_queue =
      xQueueCreate(THREAD_SNIFFER_QUEUE_SIZE, sizeof(sniffer_packet_info_t));
  xTaskCreate(debug_handler_task, "debug_handler_task", 8192, NULL, 20, NULL);
}

void thread_sniffer_run() {
  pcap_start();
  printf("START SESSION\n");
  packets_count = 0;
  thread_sniffer_show_event(THREAD_SNIFFER_START_EV, NULL);
  thread_sniffer_show_event_cb(THREAD_SNIFFER_NEW_PACKET_EV, packets_count);
  openthread_enable_promiscous_mode(&on_pcap_receive);
}

void thread_sniffer_stop() {
  printf("STOP SESSION\n");
  pcap_stop();
  thread_sniffer_show_event(THREAD_SNIFFER_STOP_EV, NULL);
  openthread_disable_promiscous_mode();
}

static void chek_for_fatal_error(esp_err_t err, const char* err_tag) {
  if (err != ESP_OK) {
    thread_sniffer_show_event_cb(THREAD_SNIFFER_FATAL_ERROR_EV, err_tag);
  }
}
static void chek_for_fatal_false(bool ok, const char* err_tag) {
  if (!ok) {
    thread_sniffer_show_event_cb(THREAD_SNIFFER_FATAL_ERROR_EV, err_tag);
  }
}

static esp_err_t pcap_start() {
  esp_err_t ret = ESP_OK;
  FILE* fp = NULL;
  bool save_in_sd = false;
  if (sd_card_mount() == ESP_OK) {
    save_in_sd = true;
  } else if (flash_fs_mount() == ESP_OK) {
    save_in_sd = false;
  } else {
    chek_for_fatal_false(false, "FAILED TO CREATE PCAP FILE");
  }

  char* pcap_path = (char*) malloc(100);
  files_ops_incremental_name(save_in_sd ? SD_CARD : FLASH_FS, "thread", ".pcap",
                             pcap_path);
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
  thread_pcap.is_writing = true;

  thread_sniffer_show_event(THREAD_SNIFFER_DESTINATION_EV, &save_in_sd);

  free(pcap_path);
  return ESP_OK;
  // err:
  if (fp) {
    fclose(fp);
  }
  thread_pcap.is_opened = false;
  return ret;
}

static esp_err_t pcap_stop() {
  esp_err_t ret = ESP_OK;
  char* err_str = malloc(30);
  chek_for_fatal_error(pcap_del_session(thread_pcap.pcap_handle),
                       "stop pcap session failed");
  thread_pcap.is_opened = false;
  thread_pcap.is_writing = false;
  thread_pcap.link_type_set = false;
  thread_pcap.pcap_handle = NULL;
  free(err_str);
  return ESP_OK;
}

void on_pcap_receive(const otRadioFrame* aFrame, bool aIsTx) {
  BaseType_t task;
  xQueueSendToBackFromISR(packet_rx_queue, aFrame, &task);
}

static esp_err_t pcap_capture(void* payload,
                              uint32_t length,
                              uint32_t seconds,
                              uint32_t microseconds) {
  if (pcap_capture_packet(thread_pcap.pcap_handle, payload, length, seconds,
                          microseconds) != ESP_OK) {
    printf("PCAP CAPTURE FAILED\n");
    return ESP_FAIL;
  }
  return ESP_OK;
}

static void thread_packet_debug(const otRadioFrame* aFrame) {
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
  otRadioFrame packet;
  while (xQueueReceive(packet_rx_queue, &packet, portMAX_DELAY) != pdFALSE) {
    packets_count++;
    thread_sniffer_show_event_cb(THREAD_SNIFFER_NEW_PACKET_EV, packets_count);
    pcap_capture(packet.mPsdu, packet.mLength,
                 packet.mInfo.mRxInfo.mTimestamp / 1000000u,
                 packet.mInfo.mRxInfo.mTimestamp % 1000000u);
    // thread_packet_debug(&packet);
    uart_sender_send_packet(UART_SENDER_PACKET_TYPE_THREAD, packet.mPsdu,
                            packet.mLength);
  }
  ESP_LOGE("debug_handler_task", "Terminated");
  vTaskDelete(NULL);
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
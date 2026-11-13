#include "warthread_module.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "gps_module.h"
#include "ieee_sniffer.h"
#include "lwip/def.h"
#include "menus_module.h"
#include "oled_screen.h"
#include "radio_selector.h"
#include "sd_card.h"
#include "thread_sniffer_bitmaps.h"
#include "wardriving_common.h"
#include "wardriving_screens_module.h"
#include "warthread_parser.h"
#include "wifi_service.h"

#define FILE_NAME                WARTH_DIR_NAME "/WarThread"
#define THREAD_FCF_MLE_PACKET    0xd841
#define THREAD_FCF_IEEE_PACKET   0x9869
#define THREAD_FCF_BEACON_PACKET 0xc000
#define PROTOCOL_TYPE_IEEE       "IEEE 802.15.4"
#define PROTOCOL_TYPE_MLE        "MLE"
#define THREAD_CHANNEL_TIME_MS   3000

#define WARTHREAD_PSDU_MAX        128
#define WARTHREAD_FRAME_QUEUE_LEN 24
#define WARTHREAD_BASE_FRAME_LEN  16

static const char* TAG = "warthread";

typedef struct {
  uint8_t len;
  uint8_t psdu[WARTHREAD_PSDU_MAX];
} warthread_frame_t;

static TaskHandle_t scanning_thread_animation_task_handle = NULL;
static TaskHandle_t scanning_thread_channel_task_handle = NULL;
static TaskHandle_t warthread_record_task_handle = NULL;
static TaskHandle_t ieee_sniffer_task_handle = NULL;
static QueueHandle_t warthread_frame_queue = NULL;

static thread_module_t context_session;
static gps_t* gps_ctx = NULL;
static volatile bool running_thread_scanner_animation = false;
static volatile bool running_thread_channel_hop = false;
static volatile bool warthread_record_running = false;
static volatile bool running = false;
static uint8_t current_channel = IEEE_SNIFFER_CHANNEL_DEFAULT;

static uint16_t csv_lines = 0;
static char* csv_file_name = NULL;
static char* csv_file_buffer = NULL;

static const char* warthread_csv_header = FORMAT_VERSION
    ",appRelease=" APP_VERSION ",model=" MODEL ",release=" RELEASE
    ",device=" DEVICE ",display=" DISPLAY ",board=" BOARD ",brand=" BRAND
    ",star=" STAR ",body=" BODY ",subBody=" SUB_BODY
    "\n"
    "DestinationPAN,Destination,ExtendedSource,Channel,UDPSource,"
    "UDPDestination,Protocol,"
    "CurrentLatitude,CurrentLongitude,AltitudeMeters,AccuracyMeters,RCOIs,"
    "MfgrId,Type";

static void update_file_name(const char* full_date_time) {
  if (csv_file_name != NULL) {
    snprintf(csv_file_name, strlen(FILE_NAME) + 40, "%s_%s.csv", FILE_NAME,
             full_date_time);
  }
}

static volatile uint32_t total_packets_sniffed = 0;

static void wardriving_screens_thread_animation_task(void* arg) {
  oled_screen_clear_buffer();

  while (running) {
    static uint8_t idx = 0;
    oled_screen_clear_buffer();

    /* Left: animated Thread Sniffer logo (32x32) */
    oled_screen_display_bitmap(thread_sniffer_bitmap_arr[idx], 4, 16, 32, 32,
                               OLED_DISPLAY_NORMAL);
    idx = (idx + 1 > 3) ? 0 : (idx + 1);

    /* Right: live Thread Wardriving telemetry (x=40) */
    char ch_str[16];
    char pkt_str[16];
    char gps_str[16];

    snprintf(ch_str, sizeof(ch_str), "Ch: %02d", current_channel);
    snprintf(pkt_str, sizeof(pkt_str), "Pkt: %-5lu",
             (unsigned long) total_packets_sniffed);

    if (gps_ctx == NULL || gps_ctx->sats_in_use == 0) {
      snprintf(gps_str, sizeof(gps_str), "GPS: Search");
    } else {
      snprintf(gps_str, sizeof(gps_str), "GPS: %u sat",
               (unsigned int) gps_ctx->sats_in_use);
    }

    oled_screen_display_text("THREAD WD", 40, 0, OLED_DISPLAY_INVERT);
    oled_screen_display_text(ch_str, 40, 2, OLED_DISPLAY_NORMAL);
    oled_screen_display_text(pkt_str, 40, 4, OLED_DISPLAY_NORMAL);
    oled_screen_display_text(gps_str, 40, 6, OLED_DISPLAY_NORMAL);

    oled_screen_display_show();
    vTaskDelay(pdMS_TO_TICKS(400));
  }
  scanning_thread_animation_task_handle = NULL;
  vTaskDelete(NULL);
}

static void wardriving_thread_channel_hop_task(void* arg) {
  (void) arg;
  running_thread_channel_hop = true;
  uint32_t elapsed = 0;
  while (running_thread_channel_hop && running) {
    vTaskDelay(pdMS_TO_TICKS(100));
    if (!running_thread_channel_hop || !running) {
      break;
    }
    elapsed += 100;
    if (elapsed < THREAD_CHANNEL_TIME_MS) {
      continue;
    }
    elapsed = 0;
    current_channel++;
    if (current_channel > IEEE_SNIFFER_CHANNEL_MAX) {
      current_channel = IEEE_SNIFFER_CHANNEL_MIN;
    }
    ieee_sniffer_set_channel(current_channel);
    ESP_LOGI(TAG, "Changing channel: %d", current_channel);
  }
  scanning_thread_channel_task_handle = NULL;
  vTaskDelete(NULL);
}

static esp_err_t wardriving_module_verify_sd_card(void) {
  ESP_LOGI(TAG, "Verifying SD card");
  esp_err_t err = sd_card_mount();
  if (err == ESP_ERR_NOT_SUPPORTED) {
    wardriving_screens_module_format_sd_card();
  } else if (err != ESP_OK) {
    wardriving_screens_module_no_sd_card();
  }
  return err;
}

static void thread_module_cb_event(uint8_t button_name, uint8_t button_event) {
  if (button_event != BUTTON_PRESS_DOWN) {
    return;
  }
  switch (button_name) {
    case BUTTON_LEFT:
    case BUTTON_BOOT:
      menus_module_exit_app();
      break;
    default:
      break;
  }
}

static void warthread_process_frame(const uint8_t* psdu, uint8_t frame_len) {
  if (psdu == NULL || frame_len < WARTHREAD_BASE_FRAME_LEN || gps_ctx == NULL) {
    return;
  }

  warthread_parsed_frame_t parsed;
  if (!warthread_parse_frame(psdu, frame_len, &parsed)) {
    return;
  }

  char udp_source_str[16] = {0};
  char udp_destination_str[16] = {0};
  char csv_line_buffer[CSV_LINE_SIZE] = {0};

  if (parsed.is_mle) {
    snprintf(udp_source_str, sizeof(udp_source_str), "%u",
             parsed.udp_source_port);
    snprintf(udp_destination_str, sizeof(udp_destination_str), "%u",
             parsed.udp_destination_port);
  }

  snprintf(csv_line_buffer, sizeof(csv_line_buffer),
           "0x%04x,0x%04x,%s,%d,%s,%s,%s,%f,%f,%f,%f,%s,%s,%s\n",
           parsed.destination_pan, parsed.destination, parsed.extd_source_str,
           current_channel, udp_source_str, udp_destination_str,
           parsed.protocol_type, gps_ctx->latitude, gps_ctx->longitude,
           gps_ctx->altitude, GPS_ACCURACY, "", "", "Thread");

  if (context_session.session_records_count >= MAX_CSV_LINES) {
    ESP_LOGW(TAG, "Max CSV lines reached, rotating buffer to file");
    char* new_date = get_str_date_time(gps_ctx);
    if (new_date != NULL) {
      if (context_session.session_str != NULL) {
        free(context_session.session_str);
      }
      context_session.session_str = strdup(new_date);
    }
    context_session.session_records_count = 0;
    update_file_name(context_session.session_str);
    sd_card_write_file(csv_file_name, csv_file_buffer);
    csv_lines = CSV_HEADER_LINES;
    if (csv_file_buffer != NULL) {
      free(csv_file_buffer);
    }
    csv_file_buffer = malloc(CSV_FILE_SIZE);
    if (csv_file_buffer == NULL) {
      ESP_LOGE(TAG, "Failed to allocate memory for csv_file_buffer");
      return;
    }
    snprintf(csv_file_buffer, CSV_FILE_SIZE, "%s\n", warthread_csv_header);
  } else {
    sd_card_append_to_file(csv_file_name, csv_line_buffer);
  }

  context_session.session_records_count++;
  wardriving_screens_module_scanning(context_session.session_records_count,
                                     gps_module_get_signal_strength(gps_ctx));
}

static void warthread_record_task(void* arg) {
  warthread_frame_t frame;
  while (warthread_record_running) {
    if (xQueueReceive(warthread_frame_queue, &frame, pdMS_TO_TICKS(200)) ==
        pdTRUE) {
      warthread_process_frame(frame.psdu, frame.len);
    }
  }
  warthread_record_task_handle = NULL;
  vTaskDelete(NULL);
}

static void warthread_packet_callback(uint8_t* packet, uint8_t packet_length) {
  if (packet == NULL || packet_length < WARTHREAD_BASE_FRAME_LEN) {
    return;
  }
  total_packets_sniffed++;
  if (warthread_frame_queue == NULL || gps_ctx == NULL ||
      gps_ctx->sats_in_use == 0) {
    return;
  }
  warthread_frame_t item;
  item.len =
      (packet_length > WARTHREAD_PSDU_MAX) ? WARTHREAD_PSDU_MAX : packet_length;
  memcpy(item.psdu, packet, item.len);
  xQueueSendToBack(warthread_frame_queue, &item, 0);
}

static void thread_gps_event_handler_cb(gps_t* gps) {
  gps_ctx = gps;

  if (gps == NULL || gps->sats_in_use == 0) {
    return;
  }

  if (context_session.session_str == NULL ||
      strlen(context_session.session_str) == 0) {
    esp_err_t err = sd_card_create_dir(WARTH_DIR_NAME);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to create %s directory", WARTH_DIR_NAME);
      return;
    }
    char* date_str = get_str_date_time(gps);
    context_session.session_str =
        date_str ? strdup(date_str) : strdup("session");
    context_session.session_records_count = 0;
    update_file_name(context_session.session_str);
    ESP_LOGI(TAG, "Creating Session File: %s", context_session.session_str);
    sd_card_write_file(csv_file_name, csv_file_buffer);
    csv_lines = CSV_HEADER_LINES;

    if (csv_file_buffer != NULL) {
      free(csv_file_buffer);
    }
    csv_file_buffer = malloc(CSV_FILE_SIZE);
    if (csv_file_buffer == NULL) {
      ESP_LOGE(TAG, "Failed to allocate memory for csv_file_buffer");
      return;
    }
    snprintf(csv_file_buffer, CSV_FILE_SIZE, "%s\n", warthread_csv_header);
  }
}

void warthread_module_begin(void) {
  menus_module_set_app_state(true, thread_module_cb_event);
  ESP_LOGI(TAG, "Thread Wardriving module begin");
  wardriving_screens_module_loading_text();
  if (wardriving_module_verify_sd_card() != ESP_OK) {
    return;
  }

  wifi_service_deinit();
  radio_selector_set_zigbee_sniffer();

  context_session.session_str = NULL;
  context_session.session_records_count = 0;
  total_packets_sniffed = 0;
  csv_lines = CSV_HEADER_LINES;

  csv_file_buffer = malloc(CSV_FILE_SIZE);
  if (csv_file_buffer == NULL) {
    ESP_LOGE(TAG, "Failed to allocate memory for csv_file_buffer");
    return;
  }

  csv_file_name = malloc(strlen(FILE_NAME) + 60);
  if (csv_file_name == NULL) {
    ESP_LOGE(TAG, "Failed to allocate memory for csv_file_name");
    free(csv_file_buffer);
    csv_file_buffer = NULL;
    return;
  }
  running = true;

  snprintf(csv_file_name, strlen(FILE_NAME) + 60, "%s.csv", FILE_NAME);
  snprintf(csv_file_buffer, CSV_FILE_SIZE, "%s\n", warthread_csv_header);

  current_channel = IEEE_SNIFFER_CHANNEL_DEFAULT;

  /* Start live animation & telemetry UI task immediately */
  if (xTaskCreate(wardriving_screens_thread_animation_task, "scan_thread_anim",
                  2048, NULL, 5,
                  &scanning_thread_animation_task_handle) != pdPASS) {
    ESP_LOGE(TAG, "Could not create UI animation task");
    scanning_thread_animation_task_handle = NULL;
  }

  if (xTaskCreate(wardriving_thread_channel_hop_task, "thread_hop", 2048, NULL,
                  5, &scanning_thread_channel_task_handle) != pdPASS) {
    ESP_LOGE(TAG, "Could not create the channel-hop task");
  }

  gps_module_register_cb(thread_gps_event_handler_cb);
  gps_module_start_scan();

  if (warthread_frame_queue == NULL) {
    warthread_frame_queue =
        xQueueCreate(WARTHREAD_FRAME_QUEUE_LEN, sizeof(warthread_frame_t));
  }

  if (warthread_frame_queue != NULL && warthread_record_task_handle == NULL) {
    warthread_record_running = true;
    if (xTaskCreate(warthread_record_task, "warthread_rec", 2560, NULL, 4,
                    &warthread_record_task_handle) != pdPASS) {
      ESP_LOGE(TAG, "Could not create the War-Thread recorder task");
      warthread_record_running = false;
    }
  }

  ieee_sniffer_register_cb(warthread_packet_callback);
  ieee_sniffer_set_channel(current_channel);
  if (xTaskCreate(ieee_sniffer_begin, "ieee_sniff", 2560, NULL, 5,
                  &ieee_sniffer_task_handle) != pdPASS) {
    ESP_LOGE(TAG, "Failed to create IEEE sniffer task");
    ieee_sniffer_task_handle = NULL;
  }
}

void warthread_module_exit(void) {
  if (!running) {
    return;
  }
  running = false;
  running_thread_channel_hop = false;
  // Do NOT force-delete scanning_thread_animation_task_handle with vTaskDelete:
  // if it is inside oled_screen_display_show(), force-deleting it leaves
  // oled_mutex locked forever, freezing the OLED screen on "THREAD WD"!
  for (int i = 0; i < 150 && (scanning_thread_animation_task_handle != NULL ||
                              scanning_thread_channel_task_handle != NULL);
       i++) {
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  scanning_thread_channel_task_handle = NULL;
  scanning_thread_animation_task_handle = NULL;
  gps_module_stop_read();
  gps_module_unregister_cb();

  sd_card_unmount();

  if (csv_file_buffer != NULL) {
    free(csv_file_buffer);
    csv_file_buffer = NULL;
  }
  if (csv_file_name != NULL) {
    free(csv_file_name);
    csv_file_name = NULL;
  }
  if (context_session.session_str != NULL) {
    free(context_session.session_str);
    context_session.session_str = NULL;
  }

  ieee_sniffer_stop();
  vTaskDelay(pdMS_TO_TICKS(30));
  ieee_sniffer_task_handle = NULL;
  ieee_sniffer_register_cb(NULL);

  warthread_record_running = false;
  if (warthread_record_task_handle != NULL) {
    for (int i = 0; i < 50 && warthread_record_task_handle != NULL; i++) {
      vTaskDelay(pdMS_TO_TICKS(10));
    }
    if (warthread_record_task_handle != NULL) {
      vTaskDelete(warthread_record_task_handle);
      warthread_record_task_handle = NULL;
    }
  }

  if (warthread_frame_queue != NULL) {
    vQueueDelete(warthread_frame_queue);
    warthread_frame_queue = NULL;
  }
  wifi_service_init(WIFI_MODE_STA);
}

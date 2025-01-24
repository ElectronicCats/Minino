#include "warthread_module.h"
#include <string.h>
#include "esp_log.h"
#include "esp_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "general_submenu.h"
#include "gps_module.h"
#include "lwip/def.h"
#include "menus_module.h"
#include "oled_screen.h"
#include "open_thread.h"
#include "radio_selector.h"
#include "sd_card.h"
#include "thread_sniffer_bitmaps.h"
#include "wardriving_common.h"
#include "wardriving_screens_module.h"

#define FILE_NAME WARTH_DIR_NAME "/WarThread"

#define THREAD_FCF_MLE_PACKET    0xd841
#define THREAD_FCF_IEEE_PACKET   0x9869
#define THREAD_FCF_BEACON_PACKET 0xc000
#define PROTOCOL_TYPE_IEEE       "IEEE 802.15.4"
#define PROTOCOL_TYPE_MLE        "MLE"
#define THREAD_CHANNEL_TIME      3000

static const char* TAG = "warthread";

static TaskHandle_t thread_task_sniffer = NULL;
static TaskHandle_t scanning_thread_animation_task_handle = NULL;
static TaskHandle_t scanning_thread_channel_task_handle = NULL;
static thread_module_t context_session;
static gps_t* gps_ctx;
static bool running_thread_scanner_animation = false;
static bool running_thread_channel_hopp = false;
static uint8_t current_channel = 11;

static uint16_t csv_lines;
char* csv_file_name = NULL;
char* csv_file_buffer = NULL;

const char* warthread_csv_header = FORMAT_VERSION
    ",appRelease=" APP_VERSION ",model=" MODEL ",release=" RELEASE
    ",device=" DEVICE ",display=" DISPLAY ",board=" BOARD ",brand=" BRAND
    ",star=" STAR ",body=" BODY ",subBody=" SUB_BODY
    "\n"
    // IEEE 802.15.4 fields
    "DestinationPAN,Destination,ExtendedSource,Channel,UDPSource,"
    "UDPDestination,Protocol,"
    // GPS fields
    "CurrentLatitude,CurrentLongitude,AltitudeMeters,AccuracyMeters,RCOIs,"
    "MfgrId,Type";

static void thread_gps_event_handler_cb(gps_t* gps);

static void update_file_name(char* full_date_time) {
  sprintf(csv_file_name, "%s_%s.csv", FILE_NAME, full_date_time);
}

static void wardriving_screens_thread_animation_task() {
  oled_screen_clear_buffer();

  while (true) {
    static uint8_t idx = 0;
    oled_screen_display_bitmap(thread_sniffer_bitmap_arr[idx], 0, 0, 32, 32,
                               OLED_DISPLAY_NORMAL);
    idx = ++idx > 3 ? 0 : idx;
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
  vTaskDelete(NULL);
}

static void wardriving_thread_channel_hopp_task() {
  running_thread_channel_hopp = true;
  while (running_thread_channel_hopp) {
    openthread_set_dataset(current_channel, 0x1234);
    vTaskDelay(THREAD_CHANNEL_TIME / portTICK_PERIOD_MS);
    current_channel++;
    if (current_channel > 26) {
      current_channel = 11;
    }
    ESP_LOGI(TAG, "Channging channel: %d", current_channel);
  }
  vTaskDelete(NULL);
}

static esp_err_t wardriving_module_verify_sd_card() {
  ESP_LOGI(TAG, "Verifying SD card");
  esp_err_t err = sd_card_mount();
  return err;
}

static void thread_module_cb_event(uint8_t button_name, uint8_t button_event) {
  if (button_event != BUTTON_PRESS_DOWN) {
    return;
  }
  switch (button_name) {
    case BUTTON_LEFT:
      warthread_module_exit();
      menus_module_restart();
      break;
    case BUTTON_UP:
    case BUTTON_DOWN:
    case BUTTON_RIGHT:
    default:
      break;
  }
}

static void thread_gps_event_handler_cb(gps_t* gps) {
  gps_ctx = gps;

  // ESP_LOGI(TAG,
  //          "Satellites in use: %d, signal: %s \r\n"
  //          "\t\t\t\t\t\tlatitude   = %.05f°N\r\n"
  //          "\t\t\t\t\t\tlongitude = %.05f°E\r\n",
  //          gps->sats_in_use, gps_module_get_signal_strength(gps),
  //          gps->latitude, gps->longitude);

  if (strcmp(context_session.session_str, "") == 0) {
    esp_err_t err = sd_card_create_dir(WARTH_DIR_NAME);

    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to create %s directory", WARTH_DIR_NAME);
      return;
    }
    ESP_LOGI(TAG, "New session");
    context_session.session_str = get_str_date_time(gps);
    context_session.session_records_count = 0;
    update_file_name(context_session.session_str);
    ESP_LOGI(TAG, "Creating Session File: %s", context_session.session_str);
    sd_card_write_file(csv_file_name, csv_file_buffer);
    csv_lines = CSV_HEADER_LINES;
    free(csv_file_buffer);

    ESP_LOGI(TAG, "Free heap size before allocation: %" PRIu32 " bytes",
             esp_get_free_heap_size());
    ESP_LOGI(TAG, "Allocating %d bytes for csv_file_buffer", CSV_FILE_SIZE);
    csv_file_buffer = malloc(CSV_FILE_SIZE);
    if (csv_file_buffer == NULL) {
      ESP_LOGE(TAG, "Failed to allocate memory for csv_file_buffer");
      return;
    }

    sprintf(csv_file_buffer, "%s\n",
            warthread_csv_header);  // Append header to csv file
  }

  if (gps->sats_in_use == 0) {
    ESP_LOGW(TAG, "No GPS signal");
    wardriving_screens_module_no_gps_signal();
    vTaskSuspend(scanning_thread_animation_task_handle);
    running_thread_scanner_animation = false;
    return;
  }

  if (running_thread_scanner_animation == false) {
    running_thread_scanner_animation = true;
    vTaskResume(scanning_thread_animation_task_handle);
    wardriving_screens_module_scanning(context_session.session_records_count,
                                       gps_module_get_signal_strength(gps));
  }
}

static void warthread_packet_handler(const otRadioFrame* aFrame, bool aIsTx) {
  if (gps_ctx->sats_in_use == 0) {
    ESP_LOGW(TAG, "No GPS signa dont saved");
    return;
  }

  uint8_t position = 0;
  char* protocol_type = malloc(24);
  char* udp_source_str = malloc(8);
  char* udp_destination_str = malloc(8);
  // IEEE 802.15.4 Base Frame
  uint16_t frame_control_field = *((uint16_t*) &aFrame->mPsdu[position]);
  position += sizeof(uint16_t);
  // uint8_t sequence_number = *((uint8_t*) &aFrame->mPsdu[position]);
  position += sizeof(uint8_t);
  uint16_t destination_pan = *((uint16_t*) &aFrame->mPsdu[position]);
  position += sizeof(uint16_t);
  uint16_t destination = *((uint16_t*) &aFrame->mPsdu[position]);
  position += sizeof(uint16_t);
  uint8_t extd_source[8] = {0};
  char* extd_source_str = malloc(24);
  for (uint8_t idx = 0; idx < sizeof(extd_source); idx++) {
    extd_source[idx] = aFrame->mPsdu[position + sizeof(extd_source) - 1 - idx];
  }
  position += sizeof(extd_source);
  sprintf(extd_source_str, ZB_ADDRESS_FORMAT, extd_source[0], extd_source[1],
          extd_source[2], extd_source[3], extd_source[4], extd_source[5],
          extd_source[6], extd_source[7]);

  // uint8_t fcs = *((uint8_t*) &aFrame->mPsdu[position]);
  position += sizeof(uint8_t);
  if (frame_control_field == THREAD_FCF_IEEE_PACKET) {
    strcpy(protocol_type, PROTOCOL_TYPE_IEEE);
    sprintf(udp_source_str, "%s", "");
    sprintf(udp_destination_str, "%s", "");
  } else if (frame_control_field == THREAD_FCF_MLE_PACKET) {
    strcpy(protocol_type, PROTOCOL_TYPE_MLE);
    // 2 bytes from IPHC Header offset
    position += sizeof(uint8_t);
    // uint8_t mle_destination = *((uint8_t*) &aFrame->mPsdu[position]);
    position += sizeof(uint8_t);
    // Header Compresion offset
    position += sizeof(uint8_t);
    uint16_t mle_udp_source_port =
        lwip_ntohs(*((uint16_t*) &aFrame->mPsdu[position]));
    position += sizeof(uint16_t);
    uint16_t mle_udp_destination_port =
        lwip_ntohs(*((uint16_t*) &aFrame->mPsdu[position]));
    position += sizeof(uint16_t);
    // uint16_t mle_udp_checksum =
    //     lwip_ntohs(*((uint16_t*) &aFrame->mPsdu[position]));
    position += sizeof(uint16_t);

    sprintf(udp_source_str, "%d", mle_udp_source_port);
    sprintf(udp_destination_str, "%d", mle_udp_destination_port);

  } else {
    return;
  }

  char* csv_line_buffer = malloc(CSV_LINE_SIZE);

  // ""DestinationPAN,Destination,ExtendedSource,Channel,UDPSource,UDPDestination,Protocol,""
  sprintf(csv_line_buffer,
          "0x%04x,0x%04x,%s,%d,%s,%s,%s,%f,%f,%f,%f,%s,%s,%s\n",
          destination_pan, destination, extd_source_str, current_channel,
          udp_source_str, udp_destination_str, protocol_type,
          // CurrentLatitude
          gps_ctx->latitude,
          // CurrentLongitude
          gps_ctx->longitude,
          // AltitudeMeters
          gps_ctx->altitude,
          // AccuracyMeters
          GPS_ACCURACY,
          // RCOIs
          "",
          // MfgrId
          "",
          // Type
          "Thread");

  if (context_session.session_records_count >= MAX_CSV_LINES) {
    ESP_LOGW(TAG, "Max CSV lines reached, writing to file");
    context_session.session_str = get_str_date_time(gps_ctx);
    context_session.session_records_count = 0;
    update_file_name(context_session.session_str);
    sd_card_write_file(csv_file_name, csv_file_buffer);
    csv_lines = CSV_HEADER_LINES;
    free(csv_file_buffer);

    ESP_LOGI(TAG, "Free heap size before allocation: %" PRIu32 " bytes",
             esp_get_free_heap_size());
    ESP_LOGI(TAG, "Allocating %d bytes for csv_file_buffer", CSV_FILE_SIZE);
    csv_file_buffer = malloc(CSV_FILE_SIZE);
    if (csv_file_buffer == NULL) {
      ESP_LOGE(TAG, "Failed to allocate memory for csv_file_buffer");
      return;
    }

    sprintf(csv_file_buffer, "%s\n",
            warthread_csv_header);  // Append header to csv file
  } else {
    sd_card_append_to_file(csv_file_name, csv_line_buffer);
  }

  context_session.session_records_count++;
  wardriving_screens_module_scanning(context_session.session_records_count,
                                     gps_module_get_signal_strength(gps_ctx));
  if (csv_line_buffer != NULL) {
    free(csv_line_buffer);
  }
  if (protocol_type != NULL) {
    free(protocol_type);
    free(udp_source_str);
    free(udp_destination_str);
  }
}

void warthread_module_begin() {
  ESP_LOGI(TAG, "Thread module begin");
  if (wardriving_module_verify_sd_card() != ESP_OK) {
    return;
  }

  context_session.session_str = "";

  csv_lines = CSV_HEADER_LINES;
  ESP_LOGI(TAG, "Free heap size before allocation: %" PRIu32 " bytes",
           esp_get_free_heap_size());
  ESP_LOGI(TAG, "Allocating %d bytes for csv_file_buffer", CSV_FILE_SIZE);
  csv_file_buffer = malloc(CSV_FILE_SIZE);
  if (csv_file_buffer == NULL) {
    ESP_LOGE(TAG, "Failed to allocate memory for csv_file_buffer");
    return;
  }

  csv_file_name = malloc(strlen(FILE_NAME) + 30);
  if (csv_file_name == NULL) {
    ESP_LOGE(TAG, "Failed to allocate memory for csv_file_name");
    return;
  }

  sprintf(csv_file_name, "%s.csv", FILE_NAME);
  sprintf(csv_file_buffer, "%s\n", warthread_csv_header);

  radio_selector_set_thread();
  openthread_init();
  vTaskDelay(pdMS_TO_TICKS(200));
  openthread_set_dataset(current_channel, 0x1234);

  xTaskCreate(wardriving_screens_thread_animation_task,
              "scanning_wifi_animation_task", 4096, NULL, 5,
              &scanning_thread_animation_task_handle);
  vTaskSuspend(scanning_thread_animation_task_handle);
  xTaskCreate(wardriving_thread_channel_hopp_task, "task_channel_hop", 4096,
              NULL, 5, &scanning_thread_channel_task_handle);
  gps_module_register_cb(thread_gps_event_handler_cb);
  gps_module_start_scan();

  openthread_enable_promiscous_mode(&warthread_packet_handler);

  menus_module_set_app_state(true, thread_module_cb_event);
}

void warthread_module_exit() {
  running_thread_channel_hopp = false;
  sd_card_read_file(csv_file_name);
  sd_card_unmount();
  free(csv_file_buffer);
  free(csv_file_name);
  vTaskDelay(pdMS_TO_TICKS(500));
  openthread_disable_promiscous_mode();
  vTaskDelay(pdMS_TO_TICKS(500));
  openthread_deinit();
  vTaskDelay(pdMS_TO_TICKS(500));
}
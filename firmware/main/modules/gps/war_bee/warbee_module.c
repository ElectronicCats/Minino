#include "warbee_module.h"
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "general_submenu.h"
#include "gps_module.h"
#include "ieee_sniffer.h"
#include "menus_module.h"
#include "oled_screen.h"
#include "radio_selector.h"
#include "sd_card.h"
#include "wardriving_common.h"
#include "wardriving_screens_module.h"
#include "zigbee_bitmaps.h"

#define FILE_NAME WARBEE_DIR_NAME "/Warbee"

static const char* TAG = "warbee";

static TaskHandle_t zigbee_task_sniffer = NULL;
static TaskHandle_t scanning_zigbee_animation_task_handle = NULL;
static warbee_module_t context_session;
static gps_t* gps_ctx;
static bool running_zigbee_scanner_animation = false;

static uint16_t csv_lines;
char* csv_file_name = NULL;
char* csv_file_buffer = NULL;

const char* war_bee_csv_header = FORMAT_VERSION
    ",appRelease=" APP_VERSION ",model=" MODEL ",release=" RELEASE
    ",device=" DEVICE ",display=" DISPLAY ",board=" BOARD ",brand=" BRAND
    ",star=" STAR ",body=" BODY ",subBody=" SUB_BODY
    "\n"
    // IEEE 802.15.4 fields
    "SourcePanID,SourceADDR,DestinationADDR,Channel,RSSI,SecurityEnabled,"
    "FrameType,"
    // GPS fields
    "CurrentLatitude,CurrentLongitude,AltitudeMeters,AccuracyMeters,RCOIs,"
    "MfgrId,Type";

static void warbee_gps_event_handler_cb(gps_t* gps);
static void warbee_packet_dissector(uint8_t* packet, uint8_t packet_length);

static void update_file_name(char* full_date_time) {
  sprintf(csv_file_name, "%s_%s.csv", FILE_NAME, full_date_time);
}

static void wardriving_screens_zigbee_animation_task() {
  oled_screen_clear_buffer();

  while (true) {
    static uint8_t idx = 0;
    oled_screen_display_bitmap(zigbee_bitmap_allArray[idx], 0, 0, 32, 32,
                               OLED_DISPLAY_NORMAL);
    idx = ++idx > 3 ? 0 : idx;
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
  vTaskDelete(NULL);
}

static esp_err_t wardriving_module_verify_sd_card() {
  ESP_LOGI(TAG, "Verifying SD card");
  esp_err_t err = sd_card_mount();
  if (err == ESP_ERR_NOT_SUPPORTED) {
    wardriving_screens_module_format_sd_card();
  } else if (err != ESP_OK) {
    wardriving_screens_module_no_sd_card();
  }
  return err;
}

static void warbee_module_cb_event(uint8_t button_name, uint8_t button_event) {
  if (button_event != BUTTON_PRESS_DOWN) {
    return;
  }
  switch (button_name) {
    case BUTTON_LEFT:
      warbee_module_exit();
      menus_module_restart();
      break;
    case BUTTON_UP:
    case BUTTON_DOWN:
    case BUTTON_RIGHT:
    default:
      break;
  }
}

static char* get_packet_type(uint8_t frame_type) {
  switch (frame_type) {
    case FRAME_TYPE_BEACON:
      return "Beacon";
    case FRAME_TYPE_DATA:
      return "Data";
    case FRAME_TYPE_MAC_COMMAND:
      return "MAC Command";
    default:
      return "Unknown";
  }
}

static void warbee_gps_event_handler_cb(gps_t* gps) {
  static uint32_t counter = 0;
  counter++;

  gps_ctx = gps;

  ESP_LOGI("Warbee",
           "Satellites in use: %d, signal: %s \r\n"
           "\t\t\t\t\t\tlatitude   = %.05f°N\r\n"
           "\t\t\t\t\t\tlongitude = %.05f°E\r\n",
           gps->sats_in_use, gps_module_get_signal_strength(gps), gps->latitude,
           gps->longitude);

  if (strcmp(context_session.session_str, "") == 0) {
    esp_err_t err = sd_card_create_dir(WARBEE_DIR_NAME);

    if (err != ESP_OK) {
      ESP_LOGE("Warbee", "Failed to create %s directory", WARBEE_DIR_NAME);
      return;
    }
    ESP_LOGI("Warbee", "New session");
    context_session.session_str = get_str_date_time(gps);
    context_session.session_records_count = 0;
    update_file_name(context_session.session_str);
    ESP_LOGI("Warbee", "Creating Session File: %s",
             context_session.session_str);
    sd_card_write_file(csv_file_name, csv_file_buffer);
    csv_lines = CSV_HEADER_LINES;
    free(csv_file_buffer);

    ESP_LOGI("Warbee", "Free heap size before allocation: %" PRIu32 " bytes",
             esp_get_free_heap_size());
    ESP_LOGI("Warbee", "Allocating %d bytes for csv_file_buffer",
             CSV_FILE_SIZE);
    csv_file_buffer = malloc(CSV_FILE_SIZE);
    if (csv_file_buffer == NULL) {
      ESP_LOGE("Warbee", "Failed to allocate memory for csv_file_buffer");
      return;
    }

    sprintf(csv_file_buffer, "%s\n",
            war_bee_csv_header);  // Append header to csv file
  }

  if (gps->sats_in_use == 0) {
    ESP_LOGW("Warbee", "No GPS signal");
    wardriving_screens_module_no_gps_signal();
    vTaskSuspend(scanning_zigbee_animation_task_handle);
    running_zigbee_scanner_animation = false;
    return;
  }

  if (running_zigbee_scanner_animation == false) {
    vTaskResume(scanning_zigbee_animation_task_handle);
    wardriving_screens_module_scanning(context_session.session_records_count,
                                       gps_module_get_signal_strength(gps));
    running_zigbee_scanner_animation = true;
  }
}

static void warbee_packet_dissector(uint8_t* packet, uint8_t packet_length) {
  if (gps_ctx->sats_in_use == 0) {
    ESP_LOGW("Warbee", "No GPS signa dont savel");
    return;
  }

  char* csv_line_buffer = malloc(CSV_LINE_SIZE);
  uint8_t position = 0;
  mac_fcs_t* fcs = (mac_fcs_t*) &packet[position];
  position += sizeof(uint16_t);

  if (csv_lines == CSV_HEADER_LINES) {
    ESP_LOGI("Warbee", "CSV Full");
  }
  uint16_t pan_id = 0;
  uint8_t dst_addr[8] = {0};
  uint8_t src_addr[8] = {0};
  uint16_t short_dst_addr = 0;
  uint16_t short_src_addr = 0;

  char* dst_addr_str = malloc(24);
  char* src_addr_str = malloc(24);

  switch (fcs->frameType) {
    case FRAME_TYPE_DATA:
    case FRAME_TYPE_MAC_COMMAND:
      uint8_t sequence_number = packet[position];
      position += sizeof(uint8_t);

      switch (fcs->destAddrType) {
        case ADDR_MODE_NONE:
          printf("Originating from the PAN coordinator\n");
          break;
        // Device is sending to a short address
        case ADDR_MODE_SHORT:
          pan_id = *((uint16_t*) &packet[position]);
          position += sizeof(uint16_t);
          short_dst_addr = *((uint16_t*) &packet[position]);
          position += sizeof(uint16_t);
          if (pan_id == 0xFFFF && short_dst_addr == 0xFFFF) {
            pan_id = *((uint16_t*) &packet[position]);  // srcPan
            position += sizeof(uint16_t);
            printf("Broadcast on PAN %04x\n", pan_id);
          } else {
            printf("Destination PAN:               0x%04x\n", pan_id);
            printf("Destination    :               0x%04x\n", short_dst_addr);
          }
          sprintf(dst_addr_str, "0x%04x", short_dst_addr);
          break;
        case ADDR_MODE_LONG:
          pan_id = *((uint16_t*) &packet[position]);
          position += sizeof(uint16_t);
          for (uint8_t idx = 0; idx < sizeof(dst_addr); idx++) {
            dst_addr[idx] = packet[position + sizeof(dst_addr) - 1 - idx];
          }
          position += sizeof(dst_addr);
          sprintf(dst_addr_str, ZB_ADDRESS_FORMAT, dst_addr[0], dst_addr[1],
                  dst_addr[2], dst_addr[3], dst_addr[4], dst_addr[5],
                  dst_addr[6], dst_addr[7]);
          printf("On PAN %04x to long address %s\n", pan_id, dst_addr_str);
          break;
        default: {
          ESP_LOGE(TAG_IEEE_SNIFFER,
                   "With reserved destination address type, ignoring packet\n");
          return;
        }
      }

      switch (fcs->srcAddrType) {
        case ADDR_MODE_NONE: {
          printf("Originating from the PAN coordinator\n");
          break;
        }
        case ADDR_MODE_SHORT: {
          short_src_addr = *((uint16_t*) &packet[position]);
          position += sizeof(uint16_t);
          printf("Source:                        0x%04x\n", short_src_addr);
          sprintf(src_addr_str, "0x%04x", short_src_addr);
          break;
        }
        case ADDR_MODE_LONG: {
          for (uint8_t idx = 0; idx < sizeof(src_addr); idx++) {
            src_addr[idx] = packet[position + sizeof(src_addr) - 1 - idx];
          }
          position += sizeof(src_addr);
          printf(
              "Originating from long address "
              "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\n",
              src_addr[0], src_addr[1], src_addr[2], src_addr[3], src_addr[4],
              src_addr[5], src_addr[6], src_addr[7]);
          sprintf(src_addr_str, ZB_ADDRESS_FORMAT, src_addr[0], src_addr[1],
                  src_addr[2], src_addr[3], src_addr[4], src_addr[5],
                  src_addr[6], src_addr[7]);
          break;
        }
        default: {
          ESP_LOGE(TAG_IEEE_SNIFFER,
                   "With reserved source address type, ignoring packet");
          return;
        }
      }

      break;
    case FRAME_TYPE_BEACON:
    default:
      printf("Packet ignored because of frame type (%u)\n", fcs->frameType);
      break;
  }
  // "Source,DestinationPAN,Channel,RSSI,"
  sprintf(csv_line_buffer, "0x%04x,%s,%s,%d,%d,%s,%s,%f,%f,%f,%f,%s,%s,%s\n",
          // SourcePanID
          pan_id,
          // SourceDestination
          src_addr_str,
          // DestinationPAN
          dst_addr_str,
          // Channel
          ieee_sniffer_get_channel(),
          // RSSI
          ieee_sniffer_get_rssi(),
          // SecurityEnabled
          fcs->secure ? "Yes" : "No",
          // FrameType
          get_packet_type(fcs->frameType),
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
          "Zigbee");
  // xQueueSendToBackFromISR(packet_rx_queue, packet, NULL);
  ESP_LOGI("Warbee", "CSV Line: %s", csv_line_buffer);

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
            war_bee_csv_header);  // Append header to csv file
  } else {
    sd_card_append_to_file(csv_file_name, csv_line_buffer);
  }

  context_session.session_records_count++;
  free(csv_line_buffer);
  free(dst_addr_str);
}

void warbee_module_begin() {
  if (wardriving_module_verify_sd_card() != ESP_OK) {
    return;
  }
  ESP_LOGI("Warbee", "Warbee module begin");
  csv_lines = CSV_HEADER_LINES;
  ESP_LOGI("Warbee", "Free heap size before allocation: %" PRIu32 " bytes",
           esp_get_free_heap_size());
  ESP_LOGI("Warbee", "Allocating %d bytes for csv_file_buffer", CSV_FILE_SIZE);
  csv_file_buffer = malloc(CSV_FILE_SIZE);
  if (csv_file_buffer == NULL) {
    ESP_LOGE("Warbee", "Failed to allocate memory for csv_file_buffer");
    return;
  }

  csv_file_name = malloc(strlen(FILE_NAME) + 30);
  if (csv_file_name == NULL) {
    ESP_LOGE("Warbee", "Failed to allocate memory for csv_file_name");
    return;
  }

  sprintf(csv_file_name, "%s.csv", FILE_NAME);
  sprintf(csv_file_buffer, "%s\n", war_bee_csv_header);

  context_session.session_str = "";

  radio_selector_set_zigbee_sniffer();
  ieee_sniffer_register_cb(warbee_packet_dissector);
  xTaskCreate(ieee_sniffer_channel_hop, "ieee_sniffer_task", 4096, NULL, 5,
              &zigbee_task_sniffer);
  xTaskCreate(wardriving_screens_zigbee_animation_task,
              "scanning_wifi_animation_task", 4096, NULL, 5,
              &scanning_zigbee_animation_task_handle);
  vTaskSuspend(scanning_zigbee_animation_task_handle);
  gps_module_register_cb(warbee_gps_event_handler_cb);
  gps_module_start_scan();

  menus_module_set_app_state(true, warbee_module_cb_event);
}

void warbee_module_exit() {
  ESP_LOGI("Warbee", "Warbee module end");
  ieee_sniffer_stop();
  sd_card_read_file(csv_file_name);
  sd_card_unmount();
  free(csv_file_buffer);
  free(csv_file_name);
}
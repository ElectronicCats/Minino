#include "warbee_module.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
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
#include "wifi_service.h"
#include "zigbee_bitmaps.h"

#define FILE_NAME              WARBEE_DIR_NAME "/Warbee"
#define ZIGBEE_CHANNEL_TIME_MS 1500

static const char* TAG = "warbee";

static TaskHandle_t zigbee_task_sniffer = NULL;
static TaskHandle_t zigbee_task_hop = NULL;
static TaskHandle_t scanning_zigbee_animation_task_handle = NULL;
static warbee_module_t context_session;
static gps_t* gps_ctx = NULL;
static volatile uint32_t total_packets_sniffed = 0;
static volatile bool running = false;
static volatile bool auto_hop = true;
static uint8_t current_channel = IEEE_SNIFFER_CHANNEL_DEFAULT;

static uint16_t csv_lines;
static char* csv_file_name = NULL;
static char* csv_file_buffer = NULL;

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
  if (csv_file_name != NULL) {
    snprintf(csv_file_name, strlen(FILE_NAME) + 40, "%s_%s.csv", FILE_NAME,
             full_date_time);
  }
}

static void wardriving_screens_zigbee_animation_task(void* arg) {
  oled_screen_clear_buffer();

  while (running) {
    static uint8_t idx = 0;
    oled_screen_clear_buffer();

    /* Left: animated Zigbee logo (32x32) */
    oled_screen_display_bitmap(zigbee_bitmap_allArray[idx], 4, 16, 32, 32,
                               OLED_DISPLAY_NORMAL);
    idx = (idx + 1 > 3) ? 0 : (idx + 1);

    /* Right: live Zigbee Wardriving telemetry (x=40) */
    char ch_str[16];
    char pkt_str[16];
    char gps_str[16];

    snprintf(ch_str, sizeof(ch_str), "Ch:%02d %s", current_channel,
             auto_hop ? "[AUTO]" : "[LOCK]");
    snprintf(pkt_str, sizeof(pkt_str), "Pkt: %-5lu",
             (unsigned long) total_packets_sniffed);

    if (gps_ctx == NULL || gps_ctx->sats_in_use == 0) {
      snprintf(gps_str, sizeof(gps_str), "GPS: Search");
    } else {
      snprintf(gps_str, sizeof(gps_str), "GPS: %u sat",
               (unsigned int) gps_ctx->sats_in_use);
    }

    oled_screen_display_text("ZIGBEE WD", 40, 0, OLED_DISPLAY_INVERT);
    oled_screen_display_text(ch_str, 40, 2, OLED_DISPLAY_NORMAL);
    oled_screen_display_text(pkt_str, 40, 4, OLED_DISPLAY_NORMAL);
    oled_screen_display_text(gps_str, 40, 6, OLED_DISPLAY_NORMAL);

    oled_screen_display_show();
    vTaskDelay(pdMS_TO_TICKS(400));
  }
  scanning_zigbee_animation_task_handle = NULL;
  vTaskDelete(NULL);
}

static void wardriving_zigbee_channel_hop_task(void* arg) {
  uint32_t elapsed = 0;
  while (running) {
    vTaskDelay(pdMS_TO_TICKS(100));
    if (!running) {
      break;
    }
    if (!auto_hop) {
      elapsed = 0;
      continue;
    }
    elapsed += 100;
    if (elapsed < ZIGBEE_CHANNEL_TIME_MS) {
      continue;
    }
    elapsed = 0;

    current_channel++;
    if (current_channel > IEEE_SNIFFER_CHANNEL_MAX) {
      current_channel = IEEE_SNIFFER_CHANNEL_MIN;
    }
    ieee_sniffer_set_channel(current_channel);
    ESP_LOGI(TAG, "Zigbee hopping to channel: %d", current_channel);
  }
  zigbee_task_hop = NULL;
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

static void warbee_module_cb_event(uint8_t button_name, uint8_t button_event) {
  if (button_event != BUTTON_PRESS_DOWN) {
    return;
  }
  switch (button_name) {
    case BUTTON_LEFT:
    case BUTTON_BOOT:
      menus_module_exit_app();
      break;
    case BUTTON_UP:
      auto_hop = false;
      current_channel = (current_channel >= IEEE_SNIFFER_CHANNEL_MAX)
                            ? IEEE_SNIFFER_CHANNEL_MIN
                            : (current_channel + 1);
      ieee_sniffer_set_channel(current_channel);
      ESP_LOGI(TAG, "Manual channel set: %d [LOCKED]", current_channel);
      break;
    case BUTTON_DOWN:
      auto_hop = false;
      current_channel = (current_channel <= IEEE_SNIFFER_CHANNEL_MIN)
                            ? IEEE_SNIFFER_CHANNEL_MAX
                            : (current_channel - 1);
      ieee_sniffer_set_channel(current_channel);
      ESP_LOGI(TAG, "Manual channel set: %d [LOCKED]", current_channel);
      break;
    case BUTTON_RIGHT:
      auto_hop = !auto_hop;
      ESP_LOGI(TAG, "Auto-hop %s on channel %d",
               auto_hop ? "ENABLED" : "LOCKED", current_channel);
      break;
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
  gps_ctx = gps;

  if (gps == NULL || gps->sats_in_use == 0) {
    return;
  }

  if (context_session.session_str == NULL ||
      strlen(context_session.session_str) == 0) {
    esp_err_t err = sd_card_create_dir(WARBEE_DIR_NAME);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to create %s directory", WARBEE_DIR_NAME);
      return;
    }
    char* date_str = get_str_date_time(gps);
    context_session.session_str =
        date_str ? strdup(date_str) : strdup("session");
    context_session.session_records_count = 0;
    update_file_name(context_session.session_str);
    ESP_LOGI(TAG, "Creating Session File: %s", context_session.session_str);
    if (csv_file_name != NULL && csv_file_buffer != NULL) {
      sd_card_write_file(csv_file_name, csv_file_buffer);
      csv_lines = CSV_HEADER_LINES;
    }
  }
}

static void warbee_packet_dissector(uint8_t* packet, uint8_t packet_length) {
  if (packet == NULL || packet_length < sizeof(uint16_t)) {
    return;
  }

  total_packets_sniffed++;

  uint8_t position = 0;
  mac_fcs_t* fcs = (mac_fcs_t*) &packet[position];
  position += sizeof(uint16_t);

  uint16_t pan_id = 0;
  uint8_t dst_addr[8] = {0};
  uint8_t src_addr[8] = {0};
  uint16_t short_dst_addr = 0;
  uint16_t short_src_addr = 0;
  bool source_pan_consumed = false;

  char dst_addr_str[32] = "N/A";
  char src_addr_str[32] = "N/A";

  switch (fcs->frameType) {
    case FRAME_TYPE_DATA:
    case FRAME_TYPE_MAC_COMMAND:
      position += sizeof(uint8_t);  // skip sequence number

      switch (fcs->destAddrType) {
        case ADDR_MODE_NONE:
          break;
        case ADDR_MODE_SHORT:
          if (position + 4 <= packet_length) {
            pan_id = *((uint16_t*) &packet[position]);
            position += sizeof(uint16_t);
            short_dst_addr = *((uint16_t*) &packet[position]);
            position += sizeof(uint16_t);
            if (pan_id == 0xFFFF && short_dst_addr == 0xFFFF &&
                position + 2 <= packet_length) {
              pan_id = *((uint16_t*) &packet[position]);
              position += sizeof(uint16_t);
              source_pan_consumed = true;
            }
            snprintf(dst_addr_str, sizeof(dst_addr_str), "0x%04x",
                     short_dst_addr);
          }
          break;
        case ADDR_MODE_LONG:
          if (position + 2 + sizeof(dst_addr) <= packet_length) {
            pan_id = *((uint16_t*) &packet[position]);
            position += sizeof(uint16_t);
            for (uint8_t idx = 0; idx < sizeof(dst_addr); idx++) {
              dst_addr[idx] = packet[position + sizeof(dst_addr) - 1 - idx];
            }
            position += sizeof(dst_addr);
            snprintf(dst_addr_str, sizeof(dst_addr_str), ZB_ADDRESS_FORMAT,
                     dst_addr[0], dst_addr[1], dst_addr[2], dst_addr[3],
                     dst_addr[4], dst_addr[5], dst_addr[6], dst_addr[7]);
          }
          break;
        default:
          return;
      }

      /* When PAN ID compression is disabled, the source PAN ID sits between
         the destination address and the source address. Skipping it here
         keeps the source address aligned; the broadcast case above may have
         already consumed it. */
      if (!fcs->panIdCompressed && !source_pan_consumed &&
          fcs->srcAddrType != ADDR_MODE_NONE) {
        if (position + sizeof(uint16_t) > packet_length) {
          return;
        }
        uint16_t source_pan = *((uint16_t*) &packet[position]);
        position += sizeof(uint16_t);
        if (pan_id == 0xFFFF) {
          pan_id = source_pan;
        }
      }

      switch (fcs->srcAddrType) {
        case ADDR_MODE_NONE:
          break;
        case ADDR_MODE_SHORT:
          if (position + 2 <= packet_length) {
            short_src_addr = *((uint16_t*) &packet[position]);
            position += sizeof(uint16_t);
            snprintf(src_addr_str, sizeof(src_addr_str), "0x%04x",
                     short_src_addr);
          }
          break;
        case ADDR_MODE_LONG:
          if (position + sizeof(src_addr) <= packet_length) {
            for (uint8_t idx = 0; idx < sizeof(src_addr); idx++) {
              src_addr[idx] = packet[position + sizeof(src_addr) - 1 - idx];
            }
            position += sizeof(src_addr);
            snprintf(src_addr_str, sizeof(src_addr_str), ZB_ADDRESS_FORMAT,
                     src_addr[0], src_addr[1], src_addr[2], src_addr[3],
                     src_addr[4], src_addr[5], src_addr[6], src_addr[7]);
          }
          break;
        default:
          return;
      }
      break;

    case FRAME_TYPE_BEACON:
      snprintf(dst_addr_str, sizeof(dst_addr_str), "Broadcast");
      if (position + 4 <= packet_length) {
        pan_id = *((uint16_t*) &packet[position]);
        position += sizeof(uint16_t);
        short_src_addr = *((uint16_t*) &packet[position]);
        snprintf(src_addr_str, sizeof(src_addr_str), "0x%04x", short_src_addr);
      }
      break;

    default:
      break;
  }

  ESP_LOGI("Warbee",
           "Zigbee frame: type=%s ch=%d pan=0x%04x src=%s dst=%s rssi=%d",
           get_packet_type(fcs->frameType), current_channel, pan_id,
           src_addr_str, dst_addr_str, ieee_sniffer_get_rssi());

  if (gps_ctx == NULL || gps_ctx->sats_in_use == 0 || csv_file_name == NULL) {
    return;
  }

  char* csv_line_buffer = malloc(CSV_LINE_SIZE);
  if (csv_line_buffer == NULL) {
    return;
  }

  snprintf(csv_line_buffer, CSV_LINE_SIZE,
           "0x%04x,%s,%s,%d,%d,%s,%s,%f,%f,%f,%f,%s,%s,%s\n", pan_id,
           src_addr_str, dst_addr_str, current_channel, ieee_sniffer_get_rssi(),
           fcs->secure ? "Yes" : "No", get_packet_type(fcs->frameType),
           gps_ctx->latitude, gps_ctx->longitude, gps_ctx->altitude,
           GPS_ACCURACY, "", "", "Zigbee");

  if (context_session.session_records_count >= MAX_CSV_LINES) {
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
      snprintf(csv_file_buffer, CSV_FILE_SIZE, "%s\n", war_bee_csv_header);
    }
  } else {
    sd_card_append_to_file(csv_file_name, csv_line_buffer);
  }

  context_session.session_records_count++;
  free(csv_line_buffer);
}

void warbee_module_begin(void) {
  if (running) {
    ESP_LOGW(TAG, "Already running");
    return;
  }

  menus_module_set_app_state(true, warbee_module_cb_event);
  ESP_LOGI("Warbee", "Warbee module begin");
  wardriving_screens_module_loading_text();
  if (wardriving_module_verify_sd_card() != ESP_OK) {
    return;
  }

  /* Free WiFi resources from internal SRAM and release RF PHY */
  wifi_service_deinit();

  context_session.session_str = NULL;
  context_session.session_records_count = 0;
  total_packets_sniffed = 0;
  csv_lines = CSV_HEADER_LINES;
  running = true;
  auto_hop = true;
  current_channel = IEEE_SNIFFER_CHANNEL_DEFAULT;

  csv_file_buffer = malloc(CSV_FILE_SIZE);
  if (csv_file_buffer == NULL) {
    ESP_LOGE("Warbee", "Failed to allocate memory for csv_file_buffer");
    return;
  }

  csv_file_name = malloc(strlen(FILE_NAME) + 60);
  if (csv_file_name == NULL) {
    ESP_LOGE("Warbee", "Failed to allocate memory for csv_file_name");
    free(csv_file_buffer);
    csv_file_buffer = NULL;
    return;
  }

  snprintf(csv_file_name, strlen(FILE_NAME) + 60, "%s.csv", FILE_NAME);
  snprintf(csv_file_buffer, CSV_FILE_SIZE, "%s\n", war_bee_csv_header);

  /* Start live animation & telemetry UI task */
  if (xTaskCreate(wardriving_screens_zigbee_animation_task, "scan_zb_anim",
                  2560, NULL, 5,
                  &scanning_zigbee_animation_task_handle) != pdPASS) {
    ESP_LOGE("Warbee", "could not create the animation task");
    scanning_zigbee_animation_task_handle = NULL;
  }

  /* Start channel hop task */
  if (xTaskCreate(wardriving_zigbee_channel_hop_task, "zb_hop", 2048, NULL, 5,
                  &zigbee_task_hop) != pdPASS) {
    ESP_LOGE("Warbee", "could not create the channel hop task");
    zigbee_task_hop = NULL;
  }

  radio_selector_set_zigbee_sniffer();
  ieee_sniffer_register_cb(warbee_packet_dissector);
  ieee_sniffer_set_channel(current_channel);

  /* Start IEEE sniffer task (ieee_sniffer_begin initializes radio once) */
  if (xTaskCreate(ieee_sniffer_begin, "ieee_sniff", 2560, NULL, 5,
                  &zigbee_task_sniffer) != pdPASS) {
    ESP_LOGE("Warbee", "could not create the sniffer task");
    zigbee_task_sniffer = NULL;
  }

  gps_module_register_cb(warbee_gps_event_handler_cb);
  gps_module_start_scan();
}

void warbee_module_exit(void) {
  if (!running) {
    return;
  }
  ESP_LOGI("Warbee", "Warbee module end");
  running = false;

  // Wait for UI and hop tasks to exit naturally so oled_mutex is never leaked
  for (int i = 0; i < 150 && (scanning_zigbee_animation_task_handle != NULL ||
                              zigbee_task_hop != NULL);
       i++) {
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  scanning_zigbee_animation_task_handle = NULL;
  zigbee_task_hop = NULL;

  ieee_sniffer_stop();
  ieee_sniffer_register_cb(NULL);
  vTaskDelay(pdMS_TO_TICKS(30));
  zigbee_task_sniffer = NULL;

  gps_module_stop_read();
  gps_module_unregister_cb();
  sd_card_unmount();

  if (csv_file_buffer) {
    free(csv_file_buffer);
    csv_file_buffer = NULL;
  }
  if (csv_file_name) {
    free(csv_file_name);
    csv_file_name = NULL;
  }
  if (context_session.session_str != NULL) {
    free(context_session.session_str);
    context_session.session_str = NULL;
  }

  /* Restore WiFi in STA mode */
  wifi_service_init(WIFI_MODE_STA);
}

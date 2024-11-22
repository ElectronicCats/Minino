#include "warbee_module.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ieee_sniffer.h"
#include "radio_selector.h"

#define DIR_NAME       "Wardriving"
#define FILE_NAME      DIR_NAME "/Minino"
#define FORMAT_VERSION "ElecCats-1.0"
#define APP_VERSION    CONFIG_PROJECT_VERSION
#define MODEL          "MININO"
#define RELEASE        APP_VERSION
#define DEVICE         "MININO"
#define DISPLAY        "SH1106 OLED"
#define BOARD          "ESP32C6"
#define BRAND          "Electronic Cats"
#define STAR           "Sol"
#define BODY           "3"
#define SUB_BODY       "0"

static bool running = false;

static TaskHandle_t zigbee_task_sniffer = NULL;
static int current_channel = IEEE_SNIFFER_CHANNEL_DEFAULT;

static char addressing_mode[4][15] = {"None", "Reserved", "Short/16-bit",
                                      "Long/64-bit"};

const char* csv_header = FORMAT_VERSION ",appRelease=" APP_VERSION
                                        ",model=" MODEL ",release=" RELEASE;

static void warbee_packet_dissector(uint8_t* packet, uint8_t packet_length) {
  uint8_t position = 0;

  mac_fcs_t* fcs = (mac_fcs_t*) &packet[position];
  position += sizeof(uint16_t);
  // printf("Frame Control Field\n");
  // printf("└Channel:                      %d\n", current_channel);
  // printf("└Frame type:                   %x\n", fcs->frameType);
  // printf("└Security Enabled:             %s\n", fcs->secure ? "True" :
  // "False"); printf("└Frame pending:                %s\n",
  //        fcs->framePending ? "True" : "False");
  // printf("└Acknowledge request:          %s\n",
  //        fcs->ackReqd ? "True" : "False");
  // printf("└PAN ID Compression:           %s\n",
  //        fcs->panIdCompressed ? "True" : "False");
  // printf("└Reserved:                     %s\n", fcs->rfu1 ? "True" :
  // "False"); printf("└Sequence Number Suppression:  %s\n",
  //        fcs->sequenceNumberSuppression ? "True" : "False");
  // printf("└Information Elements Present: %s\n",
  //        fcs->informationElementsPresent ? "True" : "False");
  // printf("└Destination addressing mode:  %02X %s\n", fcs->destAddrType,
  //        addressing_mode[fcs->destAddrType]);
  // printf("└Frame version:                %x\n", fcs->frameVer);
  // printf("└Source addressing mode:       %02X %s\n", fcs->srcAddrType,
  // addressing_mode[fcs->srcAddrType]);

  if (fcs->rfu1) {
    ESP_LOGE(TAG_IEEE_SNIFFER, "Reserved field 1 is set, ignoring packet");
    return;
  }

  switch (fcs->frameType) {
    case FRAME_TYPE_BEACON:
      printf("Beacon frame\n");
      break;
    case FRAME_TYPE_DATA:
      printf("Data frame\n");
      break;
    case FRAME_TYPE_MAC_COMMAND:
      printf("Beacon Request\n");
      printf("└Channel:                      %d\n", ieee_sniffer_get_channel());
      uint8_t sequence_number = packet[position];
      position += sizeof(uint8_t);
      printf("Sequence number: %u\n", sequence_number);

      uint16_t pan_id = 0;
      uint8_t dst_addr[8] = {0};
      uint8_t src_addr[8] = {0};
      uint16_t short_dst_addr = 0;
      uint16_t short_src_addr = 0;

      switch (fcs->destAddrType) {
        case ADDR_MODE_NONE:
          printf("Originating from the PAN coordinator\n");
          break;
        // Device is sending to a short address
        case ADDR_MODE_SHORT:
          pan_id = *((uint8_t*) &packet[position]);
          position += sizeof(uint8_t);
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
          break;
        default: {
          ESP_LOGE(TAG_IEEE_SNIFFER,
                   "With reserved destination address type, ignoring packet\n");
          return;
        }
      }
      break;
    default:
      printf("Packet ignored because of frame type (%u)\n", fcs->frameType);
      break;
  }

  esp_log_buffer_hex(">", packet, packet_length);
}

static void warbee_channel_hopp_task() {
  while (running) {
    current_channel = (current_channel == IEEE_SNIFFER_CHANNEL_MAX)
                          ? IEEE_SNIFFER_CHANNEL_MIN
                          : (current_channel + 1);
    ieee_sniffer_set_channel(current_channel);

    vTaskDelay(3500 / portTICK_PERIOD_MS);
  }
  vTaskDelete(NULL);
}

void warbee_module_begin() {
  running = true;
  radio_selector_set_zigbee_sniffer();
  ieee_sniffer_register_cb(warbee_packet_dissector);
  xTaskCreate(ieee_sniffer_channel_hop, "ieee_sniffer_task", 4096, NULL, 5,
              &zigbee_task_sniffer);
  xTaskCreate(warbee_channel_hopp_task, "warbee_channel_hopp_task", 4096, NULL,
              5, NULL);
}
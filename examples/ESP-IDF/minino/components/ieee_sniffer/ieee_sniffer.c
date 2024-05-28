#include "ieee_sniffer.h"
#include <esp_mac.h>
#include <stdio.h>
#include <string.h>
#include "esp_ieee802154.h"
#include "esp_log.h"
#include "esp_phy_init.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

static esp_err_t err;
static QueueHandle_t packet_rx_queue = NULL;
static ieee_sniffer_cb_t display_records_cb = NULL;
static int current_channel = IEEE_SNIFFER_CHANNEL_DEFAULT;
static int packets_count = 0;

static void debug_print_packet(uint8_t* packet, uint8_t packet_length);
static void debug_handler_task(void* pvParameters);

static char addressing_mode[4][15] = {"None", "Reserved", "Short/16-bit",
                                      "Long/64-bit"};

void esp_ieee802154_receive_done(uint8_t* frame,
                                 esp_ieee802154_frame_info_t* frame_info) {
  ESP_EARLY_LOGI(TAG_IEEE_SNIFFER, "rx OK, received %d bytes", frame[0]);
  BaseType_t task;
  xQueueSendToBackFromISR(packet_rx_queue, frame, &task);
  esp_ieee802154_receive_handle_done(frame);
}

void ieee_sniffer_register_cb(ieee_sniffer_cb_t callback) {
  display_records_cb = callback;
}

void ieee_sniffer_set_channel(int channel) {
  current_channel = channel;
  if (channel < IEEE_SNIFFER_CHANNEL_MIN) {
    current_channel = IEEE_SNIFFER_CHANNEL_MAX;
  } else if (channel > IEEE_SNIFFER_CHANNEL_MAX) {
    current_channel = IEEE_SNIFFER_CHANNEL_MIN;
  }

  err = esp_ieee802154_set_channel(current_channel);
  if (err != ESP_OK) {
    ESP_LOGE(TAG_IEEE_SNIFFER, "Error setting channel: %s",
             esp_err_to_name(err));
    return;
  }
  ESP_LOGI(TAG_IEEE_SNIFFER, "Channel set to %d", current_channel);
}

void ieee_sniffer_begin(void) {
  packet_rx_queue = xQueueCreate(8, 257);
  xTaskCreate(debug_handler_task, "debug_handler_task", 8192, NULL, 20, NULL);

  err = esp_ieee802154_enable();
  if (err != ESP_OK) {
    ESP_LOGE(TAG_IEEE_SNIFFER, "Error enabling IEEE 802.15.4 driver: %s",
             esp_err_to_name(err));
    return;
  }
  err = esp_ieee802154_set_coordinator(false);
  if (err != ESP_OK) {
    ESP_LOGE(TAG_IEEE_SNIFFER, "Error setting coordinator: %s",
             esp_err_to_name(err));
    return;
  }
  err = esp_ieee802154_set_promiscuous(true);
  if (err != ESP_OK) {
    ESP_LOGE(TAG_IEEE_SNIFFER, "Error setting promiscuous mode: %s",
             esp_err_to_name(err));
    return;
  }
  err = esp_ieee802154_set_channel(current_channel);
  if (err != ESP_OK) {
    ESP_LOGE(TAG_IEEE_SNIFFER, "Error setting channel: %s",
             esp_err_to_name(err));
    return;
  }
  ESP_ERROR_CHECK(esp_ieee802154_set_rx_when_idle(true));
  uint8_t eui64[8] = {0};
  esp_read_mac(eui64, ESP_MAC_IEEE802154);
  uint8_t eui64_rev[8] = {0};
  for (int i = 0; i < 8; i++) {
    eui64_rev[7 - i] = eui64[i];
  }
  esp_ieee802154_set_extended_address(eui64_rev);
  ESP_ERROR_CHECK(esp_ieee802154_receive());

  while (true) {
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void ieee_sniffer_stop(void) {
  err = esp_ieee802154_disable();
  if (err != ESP_OK) {
    ESP_LOGE(TAG_IEEE_SNIFFER, "Error disabling IEEE 802.15.4 driver: %s",
             esp_err_to_name(err));
    return;
  }
  vQueueDelete(packet_rx_queue);
}

static void debug_handler_task(void* pvParameters) {
  uint8_t packet[257];
  while (xQueueReceive(packet_rx_queue, &packet, portMAX_DELAY) != pdFALSE) {
    if (display_records_cb) {
      packets_count++;
      display_records_cb(packets_count, current_channel);
      if (packets_count > LIMIT_PACKETS) {
        packets_count = 0;
      }
    }
    debug_print_packet(&packet[1], packet[0]);
  }
  ESP_LOGE("debug_handler_task", "Terminated");
  vTaskDelete(NULL);
}

static void debug_print_packet(uint8_t* packet, uint8_t packet_length) {
  if (packet_length < sizeof(mac_fcs_t))
    return;  // Can't be a packet if it's shorter than the frame control field

  uint8_t position = 0;

  mac_fcs_t* fcs = (mac_fcs_t*) &packet[position];
  position += sizeof(uint16_t);
  ESP_LOGI(TAG_IEEE_SNIFFER, "Frame Control Field");
  ESP_LOGI(TAG_IEEE_SNIFFER, "└Channel:                      %d",
           current_channel);
  ESP_LOGI(TAG_IEEE_SNIFFER, "└Frame type:                   %x",
           fcs->frameType);
  ESP_LOGI(TAG_IEEE_SNIFFER, "└Security Enabled:             %s",
           fcs->secure ? "True" : "False");
  ESP_LOGI(TAG_IEEE_SNIFFER, "└Frame pending:                %s",
           fcs->framePending ? "True" : "False");
  ESP_LOGI(TAG_IEEE_SNIFFER, "└Acknowledge request:          %s",
           fcs->ackReqd ? "True" : "False");
  ESP_LOGI(TAG_IEEE_SNIFFER, "└PAN ID Compression:           %s",
           fcs->panIdCompressed ? "True" : "False");
  ESP_LOGI(TAG_IEEE_SNIFFER, "└Reserved:                     %s",
           fcs->rfu1 ? "True" : "False");
  ESP_LOGI(TAG_IEEE_SNIFFER, "└Sequence Number Suppression:  %s",
           fcs->sequenceNumberSuppression ? "True" : "False");
  ESP_LOGI(TAG_IEEE_SNIFFER, "└Information Elements Present: %s",
           fcs->informationElementsPresent ? "True" : "False");
  ESP_LOGI(TAG_IEEE_SNIFFER, "└Destination addressing mode:  %02X %s",
           fcs->destAddrType, addressing_mode[fcs->destAddrType]);
  ESP_LOGI(TAG_IEEE_SNIFFER, "└Frame version:                %x",
           fcs->frameVer);
  ESP_LOGI(TAG_IEEE_SNIFFER, "└Source addressing mode:       %02X %s",
           fcs->srcAddrType, addressing_mode[fcs->srcAddrType]);

  if (fcs->rfu1) {
    ESP_LOGE(TAG_IEEE_SNIFFER, "Reserved field 1 is set, ignoring packet");
    return;
  }

  switch (fcs->frameType) {
    case FRAME_TYPE_BEACON: {
      ESP_LOGI(TAG_IEEE_SNIFFER, "Beacon");
      break;
    }
    case FRAME_TYPE_DATA: {
      uint8_t sequence_number = packet[position];
      position += sizeof(uint8_t);
      ESP_LOGI(TAG_IEEE_SNIFFER, "Secuence Number:               %u",
               sequence_number);

      uint16_t pan_id = 0;
      uint8_t dst_addr[8] = {0};
      uint8_t src_addr[8] = {0};
      uint16_t short_dst_addr = 0;
      uint16_t short_src_addr = 0;

      switch (fcs->destAddrType) {
        case ADDR_MODE_NONE: {
          ESP_LOGI(TAG_IEEE_SNIFFER, "Without PAN ID or address field");
          break;
        }
        case ADDR_MODE_SHORT: {
          pan_id = *((uint16_t*) &packet[position]);
          position += sizeof(uint16_t);
          short_dst_addr = *((uint16_t*) &packet[position]);
          position += sizeof(uint16_t);
          if (pan_id == 0xFFFF && short_dst_addr == 0xFFFF) {
            pan_id = *((uint16_t*) &packet[position]);  // srcPan
            position += sizeof(uint16_t);
            ESP_LOGI(TAG_IEEE_SNIFFER, "Broadcast on PAN %04x", pan_id);
          } else {
            ESP_LOGI(TAG_IEEE_SNIFFER, "Destination PAN:               0x%04x",
                     pan_id);
            ESP_LOGI(TAG_IEEE_SNIFFER, "Destination    :               0x%04x",
                     short_dst_addr);
          }
          break;
        }
        case ADDR_MODE_LONG: {
          pan_id = *((uint16_t*) &packet[position]);
          position += sizeof(uint16_t);
          for (uint8_t idx = 0; idx < sizeof(dst_addr); idx++) {
            dst_addr[idx] = packet[position + sizeof(dst_addr) - 1 - idx];
          }
          position += sizeof(dst_addr);
          ESP_LOGI(TAG_IEEE_SNIFFER,
                   "On PAN %04x to long address "
                   "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
                   pan_id, dst_addr[0], dst_addr[1], dst_addr[2], dst_addr[3],
                   dst_addr[4], dst_addr[5], dst_addr[6], dst_addr[7]);
          break;
        }
        default: {
          ESP_LOGE(TAG_IEEE_SNIFFER,
                   "With reserved destination address type, ignoring packet");
          return;
        }
      }

      switch (fcs->srcAddrType) {
        case ADDR_MODE_NONE: {
          ESP_LOGI(TAG_IEEE_SNIFFER, "Originating from the PAN coordinator");
          break;
        }
        case ADDR_MODE_SHORT: {
          short_src_addr = *((uint16_t*) &packet[position]);
          position += sizeof(uint16_t);
          ESP_LOGI(TAG_IEEE_SNIFFER, "Source:                        0x%04x",
                   short_src_addr);
          break;
        }
        case ADDR_MODE_LONG: {
          for (uint8_t idx = 0; idx < sizeof(src_addr); idx++) {
            src_addr[idx] = packet[position + sizeof(src_addr) - 1 - idx];
          }
          position += sizeof(src_addr);
          ESP_LOGI(TAG_IEEE_SNIFFER,
                   "Originating from long address "
                   "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
                   src_addr[0], src_addr[1], src_addr[2], src_addr[3],
                   src_addr[4], src_addr[5], src_addr[6], src_addr[7]);
          break;
        }
        default: {
          ESP_LOGE(TAG_IEEE_SNIFFER,
                   "With reserved source address type, ignoring packet");
          return;
        }
      }

      uint8_t* data = &packet[position];
      uint8_t data_length = packet_length - position - sizeof(uint16_t);
      position += data_length;

      ESP_LOGI(TAG_IEEE_SNIFFER, "Data length: %u", data_length);
      ESP_LOGI(TAG_IEEE_SNIFFER,
               "Data: ==================================================");
      esp_log_buffer_hex(TAG_IEEE_SNIFFER, data, data_length);

      uint16_t checksum = *((uint16_t*) &packet[position]);

      ESP_LOGI(TAG_IEEE_SNIFFER, "Checksum: %04x", checksum);
      break;
    }
    case FRAME_TYPE_ACK: {
      uint8_t sequence_number = packet[position++];
      ESP_LOGI(TAG_IEEE_SNIFFER, "Ack (%u)", sequence_number);
      break;
    }
    default: {
      ESP_LOGE(TAG_IEEE_SNIFFER, "Packet ignored because of frame type (%u)",
               fcs->frameType);
      break;
    }
  }
  esp_log_buffer_hex(TAG_IEEE_SNIFFER, packet, packet_length);
  ESP_LOGI(TAG_IEEE_SNIFFER, "-----------------------");
}

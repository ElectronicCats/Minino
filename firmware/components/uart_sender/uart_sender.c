#include "uart_sender.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "esp_log.h"

#define BLE_ADDRESS_SIZE    4
#define BLE_PDU_INFO_OFFSET 19
#define BLE_ADDRESS_OFFSET  21
#define BLE_PAYLOAD_OFFSET  27
#define ESP_BLE_ADV_MAX_LEN 31

static bool is_uart_sender_initialized = false;

/**
 * @brief Initialize the UART sender
 *
 * @return void
 */
static void uart_sender_init();

static void uart_sender_init() {
  if (is_uart_sender_initialized) {
    return;
  }
  esp_log_set_level_master(ESP_LOG_NONE);
  is_uart_sender_initialized = true;
}

void uart_sender_deinit() {
  is_uart_sender_initialized = false;
  esp_log_set_level_master(ESP_LOG_INFO);
}

void uart_sender_send_packet(uart_sender_packet_type type,
                             uint8_t* packet,
                             uint8_t packet_length) {
  uart_sender_init();
  printf("@S");    // 2 bytes Start of packet
  printf("\xc0");  // Packet info
  uint8_t packet_info = 0xc0;
  uint8_t packet_bytes[2];
  packet_bytes[0] = packet_length;
  packet_bytes[1] = 0x00;
  printf("%02x%02x", packet_bytes[0], packet_bytes[1]);  // 2 bytes length
  printf("\x01\x01\x01\x01");                            // 4 bytes timestamp
  printf("\x11");                                        // 1 byte channel
  for (int i = 0; i < packet_length; i++) {
    printf("%c", packet[i]);
  }
  uint16_t fcs = packet_info + packet_bytes[0] + packet_bytes[1] + 0x1 + 0x1 +
                 0x1 + 0x1 + 0x11;
  for (int i = 0; i < packet_length; i++) {
    fcs += packet[i];
  }
  fcs &= 0xFF;

  printf("%02x", fcs);
  printf("\xca");  // RSSI
  printf("\x80");  // Status
  printf("@E");    // End of packet
  printf("\n");
}

uint32_t calculate_ble_crc24(const uint8_t* data, size_t len) {
  uint32_t crc = 0x555555;  // Inicialización del CRC con 0x555555
  uint32_t poly = 0x65B;    // Polinomio 0x65B

  for (size_t i = 0; i < len; i++) {
    crc ^= ((uint32_t) data[i] << 16);  // XOR el byte actual al CRC

    for (int j = 0; j < 8; j++) {  // Procesar bit a bit
      if (crc & 0x800000) {
        crc = (crc << 1) ^ poly;
      } else {
        crc <<= 1;
      }
    }
  }

  return (crc & 0xFFFFFF);  // Retornar solo los 24 bits inferiores
}

bool is_adv_structure_valid(const uint8_t* data, uint8_t len) {
  if (len < 2)
    return false;
  uint8_t type = data[1];

  switch (type) {
    case 0x0C:
      if (len < 4)
        return false;
      break;
    case 0xFF:
      if (len < 3)
        return false;
      break;
    case 0x01:
      if (len < 3)
        return false;
      break;
    default:
      if (len < 2)
        return false;
  }
  return true;
}

void uart_sender_send_packet_ble(uart_sender_packet_type type,
                                 esp_ble_gap_cb_param_t* packet) {
  uart_sender_init();

  if (packet == NULL)
    return;
  if (packet->scan_rst.adv_data_len > sizeof(packet->scan_rst.ble_adv))
    return;
  if (packet->scan_rst.search_evt != ESP_GAP_SEARCH_INQ_RES_EVT)
    return;
  if (packet->scan_rst.ble_adv[5] == 0x0C)
    return;

  uint8_t packet_bytes[2];
  packet_bytes[0] = packet->scan_rst.adv_data_len;
  packet_bytes[1] = 0x00;

  const uint8_t access_address[4] = {0xD6, 0xBE, 0x89, 0x8E};
  const uint8_t pdu_type = packet->scan_rst.ble_evt_type & 0x0F;
  const uint8_t tx_add = packet->scan_rst.ble_addr_type;
  uint8_t pdu_header = (pdu_type) | ((tx_add & 0x01) << 6);

  uint8_t adv_data[ESP_BLE_ADV_MAX_LEN];
  uint8_t valid_len = 0;
  uint8_t index = 0;

  while (index < packet->scan_rst.adv_data_len) {
    uint8_t len = packet->scan_rst.ble_adv[index];
    if (len == 0 || (index + len >= packet->scan_rst.adv_data_len))
      break;
    if (!is_adv_structure_valid(&packet->scan_rst.ble_adv[index], len + 1))
      break;

    memcpy(&adv_data[valid_len], &packet->scan_rst.ble_adv[index], len + 1);
    valid_len += len + 1;
    index += len + 1;

    if (valid_len >= ESP_BLE_ADV_MAX_LEN)
      break;
  }

  uint8_t total_payload_len = 6 + valid_len;  // 6 bytes addr + ADV data
  uint8_t total_packet_len =
      BLE_ADDRESS_SIZE + 2 + total_payload_len + 3;  // header + crc

  uint8_t temp_packet[64];  // Tamaño suficiente
  index = 0;

  memcpy(&temp_packet[index], access_address, BLE_ADDRESS_SIZE);
  index += BLE_ADDRESS_SIZE;
  // PDU -> Header 2 bytes
  // Header
  temp_packet[index++] = pdu_header;
  temp_packet[index++] = total_payload_len;
  // MAC address (6 bytes)
  memcpy(&temp_packet[index], packet->scan_rst.bda, ESP_BD_ADDR_LEN);
  index += ESP_BD_ADDR_LEN;

  // Valid ADV Data
  memcpy(&temp_packet[index], adv_data, valid_len);
  index += valid_len;
  // CRC 3 bytes
  uint32_t crc = calculate_ble_crc24(&temp_packet[BLE_ADDRESS_SIZE],
                                     2 + total_payload_len);
  temp_packet[index++] = (crc >> 16) & 0xFF;
  temp_packet[index++] = (crc >> 8) & 0xFF;
  temp_packet[index++] = crc & 0xFF;

  printf("@S\xc0");
  printf("%02x%02x", packet_bytes[0], packet_bytes[1]);
  printf("\x01\x01\x01\x01\x01\x11");
  printf("%c", 0x00);
  printf("%c", 0x00);

  for (int i = 0; i < index; i++) {
    printf("%c", temp_packet[i]);
  }

  printf("%c", packet->scan_rst.rssi);
  printf("\x80@E\n");
}
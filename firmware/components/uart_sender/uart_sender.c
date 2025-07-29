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
      break;
  }
  return true;
}

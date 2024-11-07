#include "uart_sender.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "esp_log.h"

#define BLE_ADDRESS_SIZE    6
#define BLE_PDU_INFO_OFFSET 19
#define BLE_ADDRESS_OFFSET  21
#define BLE_PAYLOAD_OFFSET  27

static bool is_uart_sender_initialized = false;

static unsigned char base_packet[] = {
    0x40, 0x53,                          // Start - TI packet
    0xc0,                                // Packet Info - TI Packet
    0x36, 0x00,                          // Packet Length - TI Packet
    0x8c, 0xc9, 0xaf, 0x01, 0x00, 0x00,  // Timestamp - TI Packet
    0x25,                                // Channel - TI Packet
    0x00, 0x00,                          // Connection Event - TI Packet
    0x00,                                // Connection Info - TI Packet
    0xd6, 0xbe, 0x89, 0x8e,              // Access Address - TI Packet
};

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

// unsigned char array[] = {

//     // Header - BLE Packet
//     0x42, // PDU Info - BLE Packet
//     0x21, // AdvLen - BLE Packet
//     0x15, 0xc7, 0xbf, 0x84, 0x50, 0x4b, // AdvA - BLE Packet
//     // Payload - BLE Packet
//     0x02, 0x01, 0x1a, 0x17, 0xff,
//     0x4c, 0x00, 0x09, 0x08, 0x13, 0x03, 0xc0, 0xa8, 0x00, 0xb2, 0x1b, 0x58,
//     0x16, 0x08, 0x00, 0xa6, 0x48, 0x4c, 0x4f, 0x71, 0xdf, 0xb7, 0xf1, 0x9b,
//     0xee, 0xc7, // RSSI - TI Packet 0x80, // Status - TI Packet 0x40, 0x45 //
//     END - TI packet
// };

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

void uart_sender_send_packet_ble(uart_sender_packet_type type,
                                 esp_ble_gap_cb_param_t* packet) {
  uart_sender_init();
  uint8_t packet_length =
      1 + 1 + BLE_ADDRESS_SIZE + packet->scan_rst.adv_data_len;
  for (int i = 0; i < sizeof(base_packet); i++) {
    printf("%c", base_packet[i]);
  }
  unsigned char temp_packet[packet_length];
  uint8_t index = 0;
  temp_packet[index] = packet->scan_rst.ble_evt_type;
  index++;
  temp_packet[index] = packet->scan_rst.adv_data_len;
  index++;
  for (int i = 0; i < BLE_ADDRESS_SIZE; i++) {
    temp_packet[index] = packet->scan_rst.bda[i];
    index++;
  }
  for (int i = 0; i < packet->scan_rst.adv_data_len; i++) {
    temp_packet[index] = packet->scan_rst.ble_adv[i];
    index++;
  }
  // printf("%c", packet->scan_rst.ble_evt_type);
  // printf("%c", packet->scan_rst.adv_data_len);
  // for (int i = 0; i < BLE_ADDRESS_SIZE; i++) {
  //   printf("%c", packet->scan_rst.bda[i]);
  // }
  // for (int i = 0; i < packet->scan_rst.adv_data_len; i++) {
  //   printf("%c", packet->scan_rst.ble_adv[i]);
  // }
  for (int i = 0; i < packet_length; i++) {
    printf("%c", temp_packet[i]);
  }

  uint16_t crc = calculate_ble_crc24(temp_packet, packet_length);
  printf("%02x%02x", (crc >> 8) & 0xFF, crc & 0xFF);
  printf("%c", packet->scan_rst.rssi);
  printf("\x80");  // Status
  printf("@E");    // End of packet
  printf("\n");
}

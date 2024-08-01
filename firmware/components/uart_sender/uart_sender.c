#include "uart_sender.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "esp_log.h"

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

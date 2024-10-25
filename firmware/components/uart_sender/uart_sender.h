#pragma once
#include <stdint.h>
#include "esp_gap_ble_api.h"
#ifndef UART_SENDER
  #define UART_SENDER

typedef enum {
  UART_SENDER_PACKET_TYPE_ZIGBEE = 0,
  UART_SENDER_PACKET_TYPE_BLE,
  UART_SENDER_PACKET_TYPE_WIFI,
  UART_SENDER_PACKET_TYPE_THREAD,
} uart_sender_packet_type;

/**
 * @brief Send a UART packet
 *
 * @param packet packet to send
 * @param len length of the packet
 * @return void
 */
void uart_sender_send_packet(uart_sender_packet_type type,
                             uint8_t* packet,
                             uint8_t len);

void uart_sender_send_packet_ble(uart_sender_packet_type type,
                                 esp_ble_gap_cb_param_t* packet);
/**
 * @brief DeInitialize the UART sender
 *
 * @return void
 */
void uart_sender_deinit();
#endif  // UART_SENDER

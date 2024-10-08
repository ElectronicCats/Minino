#pragma once
#include <stdint.h>
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

/**
 * @brief DeInitialize the UART sender
 *
 * @return void
 */
void uart_sender_deinit();
#endif  // UART_SENDER

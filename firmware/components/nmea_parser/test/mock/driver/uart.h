// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

typedef int uart_port_t;
typedef int uart_word_length_t;
typedef int uart_parity_t;
typedef int uart_stop_bits_t;

#define UART_NUM_0               0
#define UART_NUM_1               1
#define UART_NUM_2               2
#define UART_DATA_8_BITS         3
#define UART_PARITY_DISABLE      0
#define UART_STOP_BITS_1         1
#define UART_HW_FLOWCTRL_DISABLE 0
#define UART_SCLK_DEFAULT        0
#define UART_PIN_NO_CHANGE       -1

typedef struct {
  uint32_t baud_rate;
  uart_word_length_t data_bits;
  uart_parity_t parity;
  uart_stop_bits_t stop_bits;
  int flow_ctrl;
  int source_clk;
} uart_config_t;

static inline esp_err_t uart_param_config(uart_port_t uart_num,
                                          const uart_config_t* uart_config) {
  (void) uart_num;
  (void) uart_config;
  return ESP_OK;
}

static inline esp_err_t uart_set_pin(uart_port_t uart_num,
                                     int tx_io_num,
                                     int rx_io_num,
                                     int rts_io_num,
                                     int cts_io_num) {
  (void) uart_num;
  (void) tx_io_num;
  (void) rx_io_num;
  (void) rts_io_num;
  (void) cts_io_num;
  return ESP_OK;
}

static inline esp_err_t uart_driver_install(uart_port_t uart_num,
                                            int rx_buffer_size,
                                            int tx_buffer_size,
                                            int queue_size,
                                            void* uart_queue,
                                            int intr_alloc_flags) {
  (void) uart_num;
  (void) rx_buffer_size;
  (void) tx_buffer_size;
  (void) queue_size;
  (void) uart_queue;
  (void) intr_alloc_flags;
  return ESP_OK;
}

static inline esp_err_t uart_driver_delete(uart_port_t uart_num) {
  (void) uart_num;
  return ESP_OK;
}

static inline int uart_read_bytes(uart_port_t uart_num,
                                  void* buf,
                                  uint32_t length,
                                  uint32_t ticks_to_wait) {
  (void) uart_num;
  (void) buf;
  (void) length;
  (void) ticks_to_wait;
  return 0;
}

static inline int uart_pattern_pop_pos(uart_port_t uart_num) {
  (void) uart_num;
  return -1;
}

static inline esp_err_t uart_flush_input(uart_port_t uart_num) {
  (void) uart_num;
  return ESP_OK;
}

#pragma once
#include <stdint.h>
#include <stddef.h>
#include "esp_system.h"

#define UART_DATA_5_BITS 0
#define UART_DATA_6_BITS 1
#define UART_DATA_7_BITS 2
#define UART_DATA_8_BITS 3

#define UART_PARITY_DISABLE 0
#define UART_PARITY_ODD 2
#define UART_PARITY_EVEN 3

#define UART_STOP_BITS_1 1
#define UART_STOP_BITS_1_5 2
#define UART_STOP_BITS_2 3

#define UART_HW_FLOWCTRL_DISABLE 0
#define UART_HW_FLOWCTRL_RTS 1
#define UART_HW_FLOWCTRL_CTS 2
#define UART_HW_FLOWCTRL_CTS_RTS 3

typedef struct {
    int baud_rate;
    int data_bits;
    int parity;
    int stop_bits;
    int flow_ctrl;
} uart_config_t;

typedef struct {
    int buffer_size;
    uart_config_t uart_config;
} uart_bridge_config_t;

esp_err_t uart_bridge_begin(uart_config_t uart_config, int buffer_size);
uart_bridge_config_t uart_bridge_get_config(void);
int uart_bridge_write(const char *src, size_t size);
esp_err_t uart_bridge_read(char *buf, int length, int timeout_ms);

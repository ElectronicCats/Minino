/**
 * @file uart_bridge.h
 *
 * @brief UART bridge module.
 *
 * This module provides functions to read and write data from and to the UART0
 * port. The UART0 port is the one that is available on the external pins of the
 * MININO, they are labeled as TXD and RXD.
 */
#pragma once

#include "driver/uart.h"
#include "esp_err.h"

typedef struct {
  int buffer_size;
  uart_config_t uart_config;
} uart_bridge_config_t;

/**
 * @brief Initialize the UART bridge.
 *
 * @param uart_config The UART configuration.
 * @param buffer_size The size of the UART buffer.
 *
 * @return ESP_OK on success, ESP_FAIL otherwise.
 */
esp_err_t uart_bridge_begin(uart_config_t uart_config, int buffer_size);

/**
 * @brief Read data from the UART bridge.
 *
 * @param buffer The buffer to read data into.
 * @param buffer_size The size of the buffer.
 *
 * @return ESP_OK on success, ESP_FAIL otherwise.
 */
esp_err_t uart_bridge_read(char* buffer, int buffer_size, int timeout_ms);

/**
 * @brief Write data to the UART bridge.
 *
 * @param buffer The buffer to write data from.
 * @param buffer_size The size of the buffer.
 *
 * @return ESP_OK on success, ESP_FAIL otherwise.
 */
esp_err_t uart_bridge_write(const char* buffer, int buffer_size);

/**
 * @brief Set the logs to be sent to the UART bridge.
 *
 * @return void
 */
void uart_bridge_set_logs_to_uart();

/**
 * @brief Set the logs to be sent to the USB.
 *
 * @return void
 */
void uart_bridge_set_logs_to_usb();

uart_bridge_config_t uart_bridge_get_config();

/**
 * @brief End the UART bridge.
 *
 * @return ESP_OK on success, ESP_FAIL otherwise.
 */
esp_err_t uart_bridge_end();

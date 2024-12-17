/**
 * @file uart_bridge.h
 *
 * @brief UART bridge module.
 *
 * This module provides functions to read and write data from and to the UART0
 * port. The UART0 port is the one that is available on the external pins of the
 * MININO, they are labeled as TX0 and RX0.
 */
#pragma once

#include "esp_err.h"

/**
 * @brief Initialize the UART bridge.
 *
 * @param baud_rate The baud rate to use.
 * @param buffer_size The size of the UART buffer.
 *
 * @return ESP_OK on success, ESP_FAIL otherwise.
 */
esp_err_t uart_bridge_begin(int baud_rate, int buffer_size);

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
 * @brief End the UART bridge.
 *
 * @return ESP_OK on success, ESP_FAIL otherwise.
 */
esp_err_t uart_bridge_end();

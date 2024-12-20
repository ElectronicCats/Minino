#include "uart_bridge.h"
#include <string.h>
#include "esp_log.h"

static const char* TAG = "uart_bridge";

uart_bridge_config_t uart_bridge_config;

esp_err_t uart_bridge_begin(uart_config_t uart_config, int buffer_size) {
#if !defined(CONFIG_UART_BRIDGE_DEBUG)
  esp_log_level_set(TAG, ESP_LOG_NONE);
#endif

  uart_bridge_config.buffer_size = buffer_size;
  uart_bridge_config.uart_config = uart_config;

  // uart_config_t uart_config = {
  //     .baud_rate = baud_rate,
  //     .data_bits = UART_DATA_8_BITS,
  //     .parity = UART_PARITY_DISABLE,
  //     .stop_bits = UART_STOP_BITS_1,
  //     .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
  // };

  esp_err_t err = uart_param_config(UART_NUM_0, &uart_config);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to configure UART parameters (error code: %d)", err);
    return err;
  }

  err = uart_set_pin(UART_NUM_0, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE,
                     UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set UART pins (error code: %d)", err);
    return err;
  }

  err = uart_driver_install(UART_NUM_0, buffer_size, 0, 0, NULL, 0);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to install UART driver (error code: %d)", err);
    return err;
  }

  return ESP_OK;
}

esp_err_t uart_bridge_read(char* buffer, int buffer_size, int timeout_ms) {
  int bytes_read = uart_read_bytes(UART_NUM_0, (uint8_t*) buffer, buffer_size,
                                   timeout_ms / portTICK_PERIOD_MS);
  if (bytes_read < 0) {
    ESP_LOGE(TAG, "Failed to read data from UART, bytes read: %d", bytes_read);
    return ESP_FAIL;
  } else if (bytes_read == 0) {
    ESP_LOGI(TAG, "No data read from UART");
    return ESP_FAIL;
  }
  return ESP_OK;
}

esp_err_t uart_bridge_write(const char* buffer, int buffer_size) {
  int bytes_written = uart_write_bytes(UART_NUM_0, buffer, buffer_size);
  if (bytes_written < 0) {
    ESP_LOGE(TAG, "Failed to write data to UART (error code: %d)",
             bytes_written);
    return ESP_FAIL;
  }

  return ESP_OK;
}

int custom_log(const char* format, va_list args) {
  char buffer[256];
  vsnprintf(buffer, sizeof(buffer), format, args);
  uart_bridge_write(buffer, strlen(buffer));
  return 0;
}

void uart_bridge_set_logs_to_uart() {
  esp_log_set_vprintf(custom_log);
}

void uart_bridge_set_logs_to_usb() {
  esp_log_set_vprintf(&vprintf);
}

uart_bridge_config_t uart_bridge_get_config() {
  return uart_bridge_config;
}

esp_err_t uart_bridge_end() {
  esp_err_t err = uart_driver_delete(UART_NUM_0);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to delete UART driver (error code: %d)", err);
    return err;
  }

  return ESP_OK;
}

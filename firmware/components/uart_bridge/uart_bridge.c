#include "uart_bridge.h"
#include <string.h>
#include "esp_log.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "uart_bridge";

static uart_bridge_config_t uart_bridge_config;

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

/*
 * esp_log_set_vprintf() sink that mirrors the logs onto UART0.
 *
 * Two things matter here:
 *
 *  - The return value. vsnprintf/vprintf must return the number of characters
 *    that *would* have been written. This used to return 0, which makes every
 *    printf() in the firmware report a length of zero.
 *  - It must not block. uart_write_bytes() is a ring-buffer send with
 *    portMAX_DELAY, and this sink is reached from every ESP_LOGx in the tree -
 *    including the esp_timer task, which runs keyboard_module's button callback
 *    (the log literally marked "DO NOT REMOVE THIS LOG"). If the USB host stops
 *    draining, a blocking sink wedges the TX ring buffer and every task that
 *    logs blocks forever, taking button input down with it.
 *
 * A short timeout bounds that. Log lines are dropped instead of freezing the
 * system, which is the right trade for a diagnostic channel.
 */
int custom_log(const char* format, va_list args) {
  char buffer[256];
  va_list args_copy;
  va_copy(args_copy, args);
  int needed = vsnprintf(buffer, sizeof(buffer), format, args_copy);
  va_end(args_copy);
  if (needed < 0) {
    return 0;
  }
  size_t len = strlen(buffer);
  if (len > 0) {
    /* Bounded write.
     *
     * uart_write_bytes() is xRingbufferSend(..., portMAX_DELAY), so if the host
     * stops draining, this sink blocks every task that logs - including the
     * esp_timer task, which runs keyboard_module's button callback. ESP-IDF 5.5
     * exposes no "tx free space" query, so the budget is enforced with
     * uart_wait_tx_done(): when the TX pipeline is already backed up the line is
     * dropped instead of wedging the system. Dropping diagnostics is the right
     * trade for a log sink; a stalled host must not freeze the buttons.
     *
     * Residual: a burst from another task between the check and the write can
     * still block, but it cannot persist - the next timeout drops the line and
     * the ring buffer drains again.
     *
     * Never ESP_LOG* from inside this sink: esp_log_set_vprintf() routes the
     * call straight back here and the failure would recurse forever. */
    if (uart_wait_tx_done(UART_NUM_0, pdMS_TO_TICKS(5)) != ESP_OK) {
      return needed;
    }
    uart_write_bytes(UART_NUM_0, buffer, len);
  }
  return needed;
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

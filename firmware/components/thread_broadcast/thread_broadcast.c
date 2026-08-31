#include "thread_broadcast.h"
#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "thread_broadcast";

static otUdpSocket mSocket;
static TaskHandle_t sender_task_handler = NULL;
static on_msg_recieve_cb_t on_msg_recieve_cb = NULL;

static void on_udp_recieve(void* aContext,
                           otMessage* aMessage,
                           const otMessageInfo* aMessageInfo) {
  (void) aContext;
  (void) aMessageInfo;
  uint16_t msg_len = otMessageGetLength(aMessage);
  uint16_t msg_offset = otMessageGetOffset(aMessage);
  if (msg_len <= msg_offset) {
    return;
  }
  size_t payload_size = msg_len - msg_offset;
  if (payload_size > 1024) {
    payload_size = 1024;
  }
  char* str = (char*) malloc(payload_size + 1);
  if (!str) {
    ESP_LOGE(TAG, "OOM in udp receive");
    return;
  }
  otMessageRead(aMessage, msg_offset, str, payload_size);
  str[payload_size] = '\0';
  ESP_LOGI(TAG, "MSG: %s", str);
  if (on_msg_recieve_cb != NULL) {
    on_msg_recieve_cb(str);
  }
  free(str);
}

static void sender_task(void* pvParameters) {
  (void) pvParameters;
  uint16_t counter = 0;
  while (1) {
    counter++;
    char str[32];
    snprintf(str, sizeof(str), "Counter: %u", counter);
    vTaskDelay(pdMS_TO_TICKS(500));
    openthread_udp_send(&mSocket, "ff02::1", PORT, str, strlen(str));
  }
}

void thread_broadcast_init() {
  openthread_init();
  vTaskDelay(pdMS_TO_TICKS(200));
  openthread_udp_open(&mSocket, on_udp_recieve);
  openthread_udp_bind(&mSocket, PORT);
  xTaskCreate(sender_task, "sender", 2048, NULL, 10, &sender_task_handler);
}

void thread_broadcast_deinit() {
  if (sender_task_handler != NULL) {
    vTaskDelete(sender_task_handler);
    sender_task_handler = NULL;
  }
  openthread_udp_close(&mSocket);
  openthread_deinit();
}

void thread_broadcast_set_on_msg_recieve_cb(on_msg_recieve_cb_t cb) {
  on_msg_recieve_cb = cb;
}

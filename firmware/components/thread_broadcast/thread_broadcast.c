#include "thread_broadcast.h"
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

otUdpSocket mSocket;
TaskHandle_t sender_task_handler = NULL;

void (*on_msg_recieve_cb)(char*) = NULL;

void on_udp_recieve(void* aContext,
                    otMessage* aMessage,
                    const otMessageInfo* aMessageInfo) {
  otError error = OT_ERROR_NONE;

  int payload_size =
      (otMessageGetLength(aMessage) - otMessageGetOffset(aMessage));
  void* data = malloc(payload_size);
  otMessageRead(aMessage, otMessageGetOffset(aMessage), data, payload_size);
  char* str = (char*) malloc(payload_size + 1);
  sprintf(str, "%s", (char*) data);
  printf("MSG\n");
  printf("%s\n", str);
  if (on_msg_recieve_cb != NULL) {
    on_msg_recieve_cb(str);
  }
  free(str);
  free(data);
}

void sender() {
  uint16_t counter = 0;
  while (1) {
    counter++;
    char* str = (char*) malloc(20);
    sprintf(str, "Counter: %d", counter);
    vTaskDelay(pdMS_TO_TICKS(500));
    openthread_udp_send(&mSocket, "ff02::1", PORT, str, strlen(str));
    free(str);
  }
}

void thread_broadcast_init() {
  openthread_init();
  vTaskDelay(pdMS_TO_TICKS(200));
  openthread_udp_open(&mSocket, on_udp_recieve);
  openthread_udp_bind(&mSocket, PORT);
  xTaskCreate(sender, "sender", 2048, NULL, 10, &sender_task_handler);
}

void thread_broadcast_deinit() {
  openthread_udp_close(&mSocket);
  openthread_deinit();
}

void thread_broadcast_set_on_msg_recieve_cb(on_msg_recieve_cb_t cb) {
  on_msg_recieve_cb = cb;
}

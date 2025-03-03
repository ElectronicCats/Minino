#include "odrone_id.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "location.h"
#include "spoofer.h"

static int num_spoofers = 1;
static Spoofer spoofers[16];

static void spoofing_task(void* pvParameters) {
  while (1) {
    for (int i = 0; i < num_spoofers; i++) {
      spoofers[i].update();
      vTaskDelay(pdMS_TO_TICKS(200 / num_spoofers));
    }
  }
}

extern "C" {
void odrone_id_begin() {
  printf("ODRONE ID BEGIN\n");
  for (int i = 0; i < num_spoofers; i++) {
    spoofers[i].init();
    spoofers[i].updateLocation(LATITUDE, LONGITUDE);
  }
  xTaskCreate(&spoofing_task, "spoofing_task", 4096, NULL, 5, NULL);
}
}

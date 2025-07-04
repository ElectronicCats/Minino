#include <dirent.h>
#include <sys/param.h>
#include "esp_event.h"
#include "esp_log.h"
#include "files_ops.h"
#include "lwip/inet.h"
#include "nvs_flash.h"
#include "sd_card.h"

#include "gattcmd_module.h"
#include "services/gattcmd_service.h"

#define TAG "GATT_READ"

static uint16_t idx = 0;
static bool configured = false;
static char values_array[250][128];
static char uuid_handler[64];
static char addr_handler[64];

void gatt_read_file() {
  esp_err_t err = sd_card_mount();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to mount file for reading");
    return;
  }

  FILE* file = fopen("/sdcard/gatt.csv", "r");
  if (file == NULL) {
    ESP_LOGE(TAG, "Failed to open file for reading");
    return;
  }

  char line[256];

  while (fgets(line, sizeof(line), file)) {
    line[strcspn(line, "\r\n")] = '\0';

    char* addr = strtok(line, ",");
    char* handle_str = strtok(NULL, ",");
    char* value = strtok(NULL, ",");

    if (addr && handle_str && value) {
      if (idx > 0) {
        strncpy(values_array[idx], value, sizeof(values_array[idx]) - 1);
        values_array[idx][sizeof(values_array[idx]) - 1] = '\0';

        if (!configured) {
          configured = true;
          strncpy(uuid_handler, handle_str, sizeof(uuid_handler) - 1);
          uuid_handler[sizeof(uuid_handler) - 1] = '\0';

          strncpy(addr_handler, addr, sizeof(addr_handler) - 1);
          addr_handler[sizeof(addr_handler) - 1] = '\0';
        }
      }
      idx++;
    } else {
      printf("Invalid format.\n");
    }
  }

  fclose(file);
  sd_card_unmount();

  for (int i = 0; i < idx; i++) {
    gattcmd_module_gatt_write(addr_handler, uuid_handler, values_array[i]);
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}
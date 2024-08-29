#include "wifi_screens_module.h"
#include <string.h>
#include "animations_task.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "modules/wifi/wifi_bitmaps.h"
#include "oled_screen.h"

TaskHandle_t wifi_sniffer_animation_task_handle = NULL;

void wifi_screens_module_display_sniffer_cb(sniffer_runtime_t* sniffer) {
  if (sniffer->is_running) {
    const char* packets_str = malloc(16);
    const char* channel_str = malloc(16);

    sprintf(packets_str, "%ld", sniffer->sniffed_packets);
    sprintf(channel_str, "%ld", sniffer->channel);

    uint8_t x_offset = 66;
    oled_screen_display_text("Packets", x_offset, 0, OLED_DISPLAY_INVERT);
    oled_screen_display_text(packets_str, x_offset, 1, OLED_DISPLAY_INVERT);
    oled_screen_display_text("Channel", x_offset, 3, OLED_DISPLAY_INVERT);
    oled_screen_display_text(channel_str, x_offset, 4, OLED_DISPLAY_INVERT);
  } else {
    ESP_LOGI(TAG_WIFI_SCREENS_MODULE, "sniffer task stopped");
  }
}

void wifi_screens_display_sniffer_animation_task() {
  static uint8_t idx = 0;
  oled_screen_display_bitmap(epd_bitmap_wifi_loading[idx], 0, 0, 64, 64,
                             OLED_DISPLAY_NORMAL);
  idx = ++idx > 3 ? 0 : idx;
}

void wifi_screens_sniffer_animation_start() {
  animations_task_run(&wifi_screens_display_sniffer_animation_task, 100, NULL);
}

void wifi_screens_sniffer_animation_stop() {
  animations_task_stop();
}

void wifi_screeens_show_sd_not_supported() {
  oled_screen_display_text_center("SD card not", 0, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("supported", 1, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("Switching to", 3, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("internal storage", 4, OLED_DISPLAY_NORMAL);
  vTaskDelay(2000 / portTICK_PERIOD_MS);
  oled_screen_clear();
}

void wifi_screeens_show_sd_not_found() {
  oled_screen_display_text_center("SD card ", 0, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("not found", 1, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("Switching to", 3, OLED_DISPLAY_NORMAL);
  oled_screen_display_text_center("internal storage", 4, OLED_DISPLAY_NORMAL);
  vTaskDelay(2000 / portTICK_PERIOD_MS);
  oled_screen_clear();
}

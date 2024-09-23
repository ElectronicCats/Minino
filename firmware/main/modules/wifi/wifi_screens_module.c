#include "wifi_screens_module.h"
#include <string.h>
#include "animations_task.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "modules/wifi/wifi_bitmaps.h"
#include "oled_screen.h"

TaskHandle_t wifi_sniffer_animation_task_handle = NULL;

#ifdef CONFIG_RESOLUTION_128X64
uint8_t pkts_offset = 3;
#else
uint8_t pkts_offset = 2;
#endif

void wifi_screens_module_display_sniffer_cb(sniffer_runtime_t* sniffer) {
  if (sniffer->is_running) {
    const char* packets_str = malloc(16);
    const char* channel_str = malloc(16);

    sprintf(packets_str, "%ld", sniffer->sniffed_packets);
    sprintf(channel_str, "%ld", sniffer->channel);

    uint8_t x_offset = 66;
    oled_screen_display_text("Packets", x_offset, 0, OLED_DISPLAY_INVERT);
    oled_screen_display_text(packets_str, x_offset, 1, OLED_DISPLAY_INVERT);
    oled_screen_display_text("Channel", x_offset, pkts_offset,
                             OLED_DISPLAY_INVERT);
    oled_screen_display_text(channel_str, x_offset, pkts_offset + 1,
                             OLED_DISPLAY_INVERT);
  } else {
    ESP_LOGI(TAG_WIFI_SCREENS_MODULE, "sniffer task stopped");
  }
}

void wifi_screens_display_sniffer_animation_task() {
#ifdef CONFIG_RESOLUTION_128X64
  uint8_t width = 56;
  uint8_t height = 56;
  uint8_t x = 0;
#else
  uint8_t width = 32;
  uint8_t height = 32;
  uint8_t x = (64 - width) / 2;
  // uint8_t x = 0;
#endif

  static uint8_t idx = 0;
  oled_screen_display_bitmap(epd_bitmap_wifi_loading[idx], x, 1, width, height,
                             OLED_DISPLAY_NORMAL);
  idx = ++idx > 3 ? 0 : idx;
}

void wifi_screens_sniffer_animation_start() {
  oled_screen_clear();
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

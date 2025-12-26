#include "thread_sniffer_screens.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "animations_task.h"
#include "oled_screen.h"
#include "open_thread_module.h"
#include "thread_sniffer_bitmaps.h"

#ifdef CONFIG_RESOLUTION_128X64
#else  // CONFIG_RESOLUTION_128X32
#endif

static void thread_sniffer_scanning_animation() {
  static uint8_t frame = 0;
#ifdef CONFIG_RESOLUTION_128X64
  uint8_t x = 48;
  uint8_t y = 8;
#else  // CONFIG_RESOLUTION_128X32
  uint8_t x = 0;
  uint8_t y = 0;
#endif
  oled_screen_display_bitmap(thread_sniffer_bitmap_arr[frame], x, y, 32, 32,
                             OLED_DISPLAY_NORMAL);
  frame = ++frame > 3 ? 0 : frame;
}

#ifdef CONFIG_RESOLUTION_128X64
static void thread_sniffer_show_destination(bool* save_in_sd) {
  char* str = (char*) malloc(17);
  sprintf(str, "Dest: %s", *save_in_sd ? "SD card" : "Internal");
  oled_screen_display_text_center(str, 7, OLED_DISPLAY_NORMAL);
  free(str);
}
#else  // CONFIG_RESOLUTION_128X32
static void thread_sniffer_show_destination(bool* save_in_sd) {
  char* str = (char*) malloc(17);
  oled_screen_display_text("Dest:", 40, 0, OLED_DISPLAY_INVERT);
  oled_screen_display_text(*save_in_sd ? "SD card" : "Internal", 40, 1,
                           OLED_DISPLAY_NORMAL);
  free(str);
}
#endif

#ifdef CONFIG_RESOLUTION_128X64
static void thread_sniffer_show_new_packet(uint32_t packets_count) {
  char* str = (char*) malloc(17);
  sprintf(str, "Packets: %lu", packets_count);
  oled_screen_display_text_center(str, 6, OLED_DISPLAY_INVERT);
  free(str);
}
#else  // CONFIG_RESOLUTION_128X32
static void thread_sniffer_show_new_packet(uint32_t packets_count) {
  char* str = (char*) malloc(10);
  sprintf(str, "%lu", packets_count);
  oled_screen_display_text("Packets:", 40, 2, OLED_DISPLAY_INVERT);
  oled_screen_display_text(str, 40, 3, OLED_DISPLAY_INVERT);
  free(str);
}

#endif

static void thread_sniffer_show_fatal_error(const char* error) {
  int page = 2;
  oled_screen_clear();
  oled_screen_display_text_center("Fatal Error", 0, OLED_DISPLAY_INVERT);
  if (error == NULL) {
    oled_screen_display_text_splited("Error pointer is NULL", &page,
                                     OLED_DISPLAY_NORMAL);
    goto exit;
  }
  oled_screen_display_text_splited(error, &page, OLED_DISPLAY_NORMAL);
exit:
  vTaskDelay(pdMS_TO_TICKS(4000));
  open_thread_module_exit();
}

void thread_sniffer_show_event_handler(thread_sniffer_events_t event,
                                       void* context) {
  switch (event) {
    case THREAD_SNIFFER_START_EV:
      animations_task_run(thread_sniffer_scanning_animation, 100, NULL);
      break;
    case THREAD_SNIFFER_STOP_EV:
      animations_task_stop();
      break;
    case THREAD_SNIFFER_FATAL_ERROR_EV:
      thread_sniffer_show_fatal_error((const char*) context);
      break;
    case THREAD_SNIFFER_DESTINATION_EV:
      oled_screen_clear();
      thread_sniffer_show_destination((bool*) context);
      break;
    case THREAD_SNIFFER_NEW_PACKET_EV:
      thread_sniffer_show_new_packet((uint32_t) context);
      break;
    default:
      break;
  }
}

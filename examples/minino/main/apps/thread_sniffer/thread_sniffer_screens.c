#include "thread_sniffer_screens.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "animations_task.h"
#include "oled_screen.h"
#include "open_thread_module.h"
#include "thread_sniffer_bitmaps.h"

static void thread_sniffer_scanning_animation() {
  static uint8_t frame = 0;
  oled_screen_display_bitmap(thread_sniffer_bitmap_arr[frame], 40, 8, 32, 32,
                             OLED_DISPLAY_NORMAL);
  frame = ++frame > 3 ? 0 : frame;
}

static void thread_sniffer_show_destination(bool* save_in_sd) {
  char* str = (char*) malloc(17);
  sprintf(str, "Dest: %s", *save_in_sd ? "SD card" : "Buffer");
  oled_screen_display_text_center(str, 7, OLED_DISPLAY_INVERT);
  free(str);
}

static void thread_sniffer_show_new_packet(uint32_t packets_count) {
  char* str = (char*) malloc(17);
  sprintf(str, "Packets: %lu", packets_count);
  oled_screen_display_text_center(str, 6, OLED_DISPLAY_INVERT);
  free(str);
}

static void thread_sniffer_show_fatal_error(const char* error) {
  int page = 2;
  oled_screen_clear();
  oled_screen_display_text_center("Fatal Error", 0, OLED_DISPLAY_INVERT);
  if (error == NULL) {
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
      thread_sniffer_show_fatal_error(context);
      break;
    case THREAD_SNIFFER_DESTINATION_EV:
      thread_sniffer_show_destination(context);
      break;
    case THREAD_SNIFFER_NEW_PACKET_EV:
      thread_sniffer_show_new_packet(context);
      break;
    default:
      break;
  }
}

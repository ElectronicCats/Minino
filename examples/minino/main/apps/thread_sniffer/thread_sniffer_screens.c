#include "thread_sniffer_screens.h"

#include "animations_task.h"
#include "oled_screen.h"
#include "thread_sniffer_bitmaps.h"

static void thread_sniffer_scanning_animation() {
  static uint8_t frame = 0;
  oled_screen_display_bitmap(thread_sniffer_bitmap_arr[frame], 0, 0, 32, 32,
                             OLED_DISPLAY_NORMAL);
  frame = ++frame > 3 ? 0 : frame;
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
    case THREAD_SNIFFER_ERROR_EV:
      // thread_sniffer_show_error(context);
      break;
    case THREAD_SNIFFER_DESTINATION_EV:
      //   thread_sniffer_show_destination();
      break;
    case THREAD_SNIFFER_NEW_PACKET_EV:
      //   thread_sniffer_show_new_packet(context);
      break;
    default:
      break;
  }
}

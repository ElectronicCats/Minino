#include "thread_sniffer_screens.h"

#include "oled_screen.h"

static void thread_sniffer_scanning_animation() {
  // animation
}

static void thread_sniffer_start_scanning_animation() {
  // start animation
}

static void thread_sniffer_stop_scanning_animation() {
  // stop animation
}

void thread_sniffer_show_event_handler(thread_sniffer_events_t event,
                                       void* context) {
  switch (event) {
    case THREAD_SNIFFER_START_EV:
      thread_sniffer_start_scanning_animation();
      break;
    case THREAD_SNIFFER_STOP_EV:
      thread_sniffer_stop_scanning_animation();
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

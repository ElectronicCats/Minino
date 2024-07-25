#pragma once
#include "open_thread.h"

typedef enum {
  THREAD_SNIFFER_START_EV,
  THREAD_SNIFFER_STOP_EV,
  THREAD_SNIFFER_ERROR_EV,
  THREAD_SNIFFER_DESTINATION_EV,
  THREAD_SNIFFER_NEW_PACKET_EV,
} thread_sniffer_events_t;

typedef void (*thread_sniffer_show_event_cb_t)(thread_sniffer_events_t, void*);

void thread_sniffer_init();
void thread_sniffer_run();
void thread_sniffer_stop();
void thread_sniffer_set_show_event_cb(thread_sniffer_show_event_cb_t cb);

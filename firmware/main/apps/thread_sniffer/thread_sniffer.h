#pragma once
#include "open_thread.h"

typedef enum {
  THREAD_SNIFFER_START_EV,
  THREAD_SNIFFER_STOP_EV,
  THREAD_SNIFFER_FATAL_ERROR_EV,
  THREAD_SNIFFER_DESTINATION_EV,
  THREAD_SNIFFER_NEW_PACKET_EV,
} thread_sniffer_events_t;

#define THREAD_SNIFFER_TOPIC (EV_APP_START + 2)

typedef void (*thread_sniffer_show_event_cb_t)(thread_sniffer_events_t, void*);

/* Handler de eventos por defecto, definido en thread_sniffer_screens.c. */
extern void thread_sniffer_show_event_handler(thread_sniffer_events_t event,
                                              void* context);

void thread_sniffer_init();
void thread_sniffer_run();
void thread_sniffer_stop();
void thread_sniffer_set_channel(uint8_t channel);
void thread_sniffer_subscribe_show_event(thread_sniffer_show_event_cb_t cb);
void thread_sniffer_unsubscribe_show_event(thread_sniffer_show_event_cb_t cb);
void thread_sniffer_subscribe_screen_events(void);

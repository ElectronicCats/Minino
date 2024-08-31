#pragma once
#include <stdio.h>

typedef enum {
  WEB_FILE_BROWSER_READY_EV,
  WEB_FILE_BROWSER_ALREADY_EV,
  WEB_FILE_BROWSER_ERROR_EV,
  WEB_FILE_BROWSER_STOP_EV,
  WEB_FILE_BROWSER_TRANSFER_INIT_EV,
  WEB_FILE_BROWSER_TRANSFER_STATE_EV,
  WEB_FILE_BROWSER_TRANSFERING_FILE_RESULT_EV
} web_file_browser_events_t;

typedef void (*web_file_browser_show_event_cb_t)(uint8_t, void*);
web_file_browser_show_event_cb_t wfb_show_event_cb = NULL;

void web_file_browser_begin();
void web_file_browser_stop();
void web_file_browser_set_show_event_cb(web_file_browser_show_event_cb_t cb);
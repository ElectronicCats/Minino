#pragma once

#include <stdio.h>
#include "open_thread.h"

#define PORT 12345

typedef void (*on_msg_recieve_cb_t)(otMessage*, const otMessageInfo*);

void thread_broadcast_init();
void thread_broadcast_deinit();
void thread_broadcast_set_on_msg_recieve_cb(on_msg_recieve_cb_t cb);

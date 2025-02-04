#pragma once

#include <stdio.h>

enum {
  SPAM_LIST_SSID,
  SPAM_START,
  SPAM_COUNT
} spam_menu_count = SPAM_LIST_SSID;

void ssid_spam_begin();
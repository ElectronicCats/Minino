#pragma once

#include <stdio.h>

typedef enum {
  GENERAL_SCROLLING_TEXT_WINDOW,
  GENERAL_SCROLLING_TEXT_FULL_SCREEN
} general_scrolling_text_window_type;

typedef enum {
  GENERAL_SCROLLING_TEXT_INFINITE,
  GENERAL_SCROLLING_TEXT_CLAMPED
} general_scrolling_text_scroll_type;

typedef struct {
  char* banner;
  char* text;
  char** text_arr;
  uint16_t text_len;
  uint16_t current_idx;
  uint8_t window_type;
  uint8_t scroll_type;
  void (*select_cb)();
  void (*exit_cb)();
  void (*finish_cb)();
} general_scrolling_text_ctx;

void general_scrolling_text(general_scrolling_text_ctx ctx);
void general_scrolling_text_array(general_scrolling_text_ctx ctx);
uint16_t general_scrolling_text_get_current_idx();
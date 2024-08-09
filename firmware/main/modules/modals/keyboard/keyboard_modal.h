#pragma once

#include <stdbool.h>
#include <stdio.h>

typedef struct {
  char* old_text;
  char* new_text;
  char* banner;
  size_t text_length;
  uint8_t current_char;
  volatile int8_t consumed;
  bool caps;
  bool shift;
} keyboard_modal_ctx_t;

char* keyboard_modal_write(char* text, char* banner);
#pragma once

#include <stdbool.h>
#include <stdio.h>

typedef struct {
  char* old_text;
  char* banner;
  size_t text_length;
  uint8_t current_char;
  bool shift;
  bool caps;
  int8_t consumed;
  char* new_text;
} keyboard_modal_ctx_t;

char* keyboard_module_write(char* text, char* banner);
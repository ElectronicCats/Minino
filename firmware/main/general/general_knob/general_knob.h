#pragma once

#include <stdio.h>

typedef struct {
  uint16_t min;
  uint16_t max;
  uint16_t step;
  int16_t value;
  char* var_lbl;
  char* help_lbl;
  void (*value_handler)(int16_t);
} general_knob_ctx_t;

void general_knob(general_knob_ctx_t ctx);

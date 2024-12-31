#pragma once

// Same order as logs_output_options
typedef enum {
  USB,
  UART,
} logs_output_option_t;

void logs_output_set_output(logs_output_option_t selected_option);
void logs_output();
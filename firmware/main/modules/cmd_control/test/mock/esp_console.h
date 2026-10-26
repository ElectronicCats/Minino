#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef int (*esp_console_cmd_func_t)(int argc, char** argv);

typedef struct {
  const char* command;
  const char* help;
  const char* hint;
  esp_console_cmd_func_t func;
  void* argtable;
  const char* category;
} esp_console_cmd_t;

int esp_console_cmd_register(const esp_console_cmd_t* cmd);

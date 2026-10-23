#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef int BaseType_t;
#define pdPASS             1
#define pdFAIL             0
#define pdMS_TO_TICKS(ms)  (ms)
#define portTICK_PERIOD_MS 1

typedef void* TaskHandle_t;
typedef void (*TaskFunction_t)(void*);

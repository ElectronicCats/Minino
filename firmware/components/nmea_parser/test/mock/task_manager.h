// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TASK_STACK_LARGE 8192
#define TASK_PRIORITY_HIGH 10

typedef void (*task_fn_t)(void*);

static inline esp_err_t task_manager_create(task_fn_t task_func,
                                            const char* name,
                                            uint32_t stack_size,
                                            void* param,
                                            int priority,
                                            TaskHandle_t* handle) {
  (void) task_func;
  (void) name;
  (void) stack_size;
  (void) param;
  (void) priority;
  if (handle) {
    *handle = (TaskHandle_t) 0x1234;
  }
  return ESP_OK;
}

static inline esp_err_t task_manager_delete(TaskHandle_t handle) {
  (void) handle;
  return ESP_OK;
}

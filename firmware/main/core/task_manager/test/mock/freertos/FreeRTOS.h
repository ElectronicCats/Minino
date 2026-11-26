/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Just enough FreeRTOS for task_manager.c on the host. Handles are fake
 * pointers; the test drives which task is "current" and records deletes. */
#pragma once

#include <stddef.h>
#include <stdint.h>

typedef int BaseType_t;
typedef unsigned int UBaseType_t;
typedef uint32_t TickType_t;
typedef uint32_t StackType_t;
typedef void* TaskHandle_t;
typedef void* SemaphoreHandle_t;
typedef void (*TaskFunction_t)(void*);
typedef enum { eRunning, eReady, eBlocked, eSuspended, eDeleted } eTaskState;

#define pdTRUE             1
#define pdPASS             1
#define portMAX_DELAY      0xFFFFFFFFu
#define portTICK_PERIOD_MS 1

extern TaskHandle_t mock_current_task;
extern TaskHandle_t mock_last_deleted;
extern int mock_delete_calls;

static inline BaseType_t xTaskCreate(TaskFunction_t fn,
                                     const char* name,
                                     uint32_t stack,
                                     void* params,
                                     UBaseType_t prio,
                                     TaskHandle_t* out) {
  static uintptr_t next = 0x1000;
  (void) fn, (void) name, (void) stack, (void) params, (void) prio;
  *out = (TaskHandle_t) (next += 0x10);
  return pdPASS;
}
/* vTaskDelete(NULL) deletes the current task. */
static inline void vTaskDelete(TaskHandle_t h) {
  mock_last_deleted = h != NULL ? h : mock_current_task;
  mock_delete_calls++;
}
static inline TaskHandle_t xTaskGetCurrentTaskHandle(void) {
  return mock_current_task;
}
static inline TickType_t xTaskGetTickCount(void) {
  return 0;
}
static inline UBaseType_t uxTaskGetStackHighWaterMark(TaskHandle_t h) {
  (void) h;
  return 512;
}
static inline eTaskState eTaskGetState(TaskHandle_t h) {
  (void) h;
  return eRunning;
}
static inline SemaphoreHandle_t xSemaphoreCreateMutex(void) {
  return (SemaphoreHandle_t) 1;
}
static inline BaseType_t xSemaphoreTake(SemaphoreHandle_t s, TickType_t t) {
  (void) s, (void) t;
  return pdTRUE;
}
static inline BaseType_t xSemaphoreGive(SemaphoreHandle_t s) {
  (void) s;
  return pdTRUE;
}
static inline void vSemaphoreDelete(SemaphoreHandle_t s) {
  (void) s;
}

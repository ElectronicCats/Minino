#pragma once
#include "freertos/FreeRTOS.h"

BaseType_t xTaskCreate(TaskFunction_t pxTaskCode,
                       const char * const pcName,
                       const uint32_t usStackDepth,
                       void * const pvParameters,
                       uint32_t uxPriority,
                       TaskHandle_t * const pxCreatedTask);

void vTaskDelete(TaskHandle_t xTaskToDelete);
void vTaskDelay(uint32_t xTicksToDelay);

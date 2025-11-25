#include "task_manager.h"
#include "esp_log.h"
#include "string.h"

static const char* TAG = "task_manager";

#define MAX_TASKS 50  // Máximo de tareas a trackear

// Array de tareas registradas
static task_info_t task_registry[MAX_TASKS];
static uint32_t task_count = 0;
static bool manager_initialized = false;

esp_err_t task_manager_init(void) {
  if (manager_initialized) {
    ESP_LOGW(TAG, "Task Manager ya inicializado");
    return ESP_OK;
  }

  // Limpiar registry
  memset(task_registry, 0, sizeof(task_registry));
  task_count = 0;

  ESP_LOGI(TAG, "Task Manager inicializado (capacidad: %d tareas)", MAX_TASKS);
  manager_initialized = true;

  return ESP_OK;
}

esp_err_t task_manager_create(TaskFunction_t task_func,
                              const char* name,
                              task_stack_size_t stack_size,
                              void* params,
                              task_priority_t priority,
                              TaskHandle_t* handle) {
  if (!manager_initialized) {
    ESP_LOGE(TAG, "Task Manager no inicializado");
    return ESP_ERR_INVALID_STATE;
  }

  if (task_count >= MAX_TASKS) {
    ESP_LOGE(TAG, "Límite de tareas alcanzado (%d)", MAX_TASKS);
    return ESP_ERR_NO_MEM;
  }

  // Crear la tarea usando FreeRTOS estándar
  TaskHandle_t task_handle = NULL;
  BaseType_t result =
      xTaskCreate(task_func, name, stack_size, params, priority, &task_handle);

  if (result != pdPASS || task_handle == NULL) {
    ESP_LOGE(TAG, "Error creando tarea '%s'", name);
    return ESP_FAIL;
  }

  // Registrar en el manager
  task_info_t* info = &task_registry[task_count];
  info->name = name;
  info->handle = task_handle;
  info->priority = priority;
  info->stack_size = stack_size;
  info->is_running = true;
  info->created_at_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
  info->stack_watermark = uxTaskGetStackHighWaterMark(task_handle);

  task_count++;

  // Retornar handle si se solicitó
  if (handle != NULL) {
    *handle = task_handle;
  }

  ESP_LOGI(TAG, "✅ Tarea creada: '%s' (stack=%d, priority=%d, #%lu)", name,
           stack_size, priority, task_count);

  return ESP_OK;
}

esp_err_t task_manager_delete(TaskHandle_t handle) {
  if (handle == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  // Buscar tarea en el registry
  for (uint32_t i = 0; i < task_count; i++) {
    if (task_registry[i].handle == handle) {
      task_registry[i].is_running = false;
      vTaskDelete(handle);
      ESP_LOGI(TAG, "Tarea eliminada: '%s'", task_registry[i].name);
      return ESP_OK;
    }
  }

  ESP_LOGW(TAG, "Tarea no encontrada en registry");
  return ESP_ERR_NOT_FOUND;
}

void task_manager_list_all(void) {
  ESP_LOGI(TAG, "Total de tareas: %lu / %d", task_count, MAX_TASKS);
  ESP_LOGI(TAG, "");

  for (uint32_t i = 0; i < task_count; i++) {
    task_info_t* info = &task_registry[i];
    const char* status = info->is_running ? "RUN" : "STOP";
    uint32_t uptime_sec =
        (xTaskGetTickCount() * portTICK_PERIOD_MS - info->created_at_ms) / 1000;

    ESP_LOGI(TAG, "[%2lu] %s %-20s | Pri:%2d | Stack:%5d | Uptime:%lus", i + 1,
             status, info->name, info->priority, info->stack_size, uptime_sec);
  }
}

uint32_t task_manager_get_count(void) {
  return task_count;
}

task_info_t* task_manager_get_info(TaskHandle_t handle) {
  for (uint32_t i = 0; i < task_count; i++) {
    if (task_registry[i].handle == handle) {
      return &task_registry[i];
    }
  }
  return NULL;
}

void task_manager_update_watermarks(void) {
  for (uint32_t i = 0; i < task_count; i++) {
    if (task_registry[i].is_running) {
      task_registry[i].stack_watermark =
          uxTaskGetStackHighWaterMark(task_registry[i].handle);
    }
  }
}

void task_manager_print_stack_usage(void) {
  task_manager_update_watermarks();
  for (uint32_t i = 0; i < task_count; i++) {
    if (!task_registry[i].is_running)
      continue;

    task_info_t* info = &task_registry[i];
    UBaseType_t watermark = info->stack_watermark;
    size_t used = info->stack_size - (watermark * sizeof(StackType_t));
    float usage_percent = ((float) used / (float) info->stack_size) * 100.0f;

    const char* status;
    if (watermark < 128) {
      status = "DANGER";  // Menos de 512 bytes libres
    } else if (watermark < 256) {
      status = "WARNING";  // Menos de 1KB libre
    } else {
      status = "OK";
    }

    ESP_LOGI(TAG, "[%2lu] %-20s | %5zu/%5d bytes (%.1f%%) | %s", i + 1,
             info->name, used, info->stack_size, usage_percent, status);
  }
}

bool task_manager_check_stack_overflow_risk(void) {
  task_manager_update_watermarks();

  for (uint32_t i = 0; i < task_count; i++) {
    if (task_registry[i].is_running) {
      if (task_registry[i].stack_watermark < 128) {  // < 512 bytes
        ESP_LOGE(TAG, "Stack overflow risk: '%s' (solo %lu bytes libres)",
                 task_registry[i].name,
                 task_registry[i].stack_watermark * sizeof(StackType_t));
        return true;
      }
    }
  }

  return false;
}

#include "task_manager.h"
#include "esp_log.h"
#include "freertos/semphr.h"
#include "string.h"

static const char* TAG = "task_manager";

#define MAX_TASKS 50  // Máximo de tareas a trackear

// Array de tareas registradas
static task_info_t task_registry[MAX_TASKS];
static uint32_t task_count = 0;
static bool manager_initialized = false;
static SemaphoreHandle_t task_manager_mutex = NULL;

esp_err_t task_manager_init(void) {
#if !defined(CONFIG_TASK_MANAGER_DEBUG)
  esp_log_level_set(TAG, ESP_LOG_NONE);
#endif

  if (manager_initialized) {
    ESP_LOGW(TAG, "Task Manager ya inicializado");
    return ESP_OK;
  }

  if (task_manager_mutex == NULL) {
    task_manager_mutex = xSemaphoreCreateMutex();
    if (task_manager_mutex == NULL) {
      ESP_LOGE(TAG, "Error creando mutex para Task Manager");
      return ESP_ERR_NO_MEM;
    }
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

  if (task_manager_mutex == NULL) {
    return ESP_ERR_INVALID_STATE;
  }

  if (xSemaphoreTake(task_manager_mutex, portMAX_DELAY) != pdTRUE) {
    return ESP_ERR_TIMEOUT;
  }

  if (task_count >= MAX_TASKS) {
    xSemaphoreGive(task_manager_mutex);
    ESP_LOGE(TAG, "Límite de tareas alcanzado (%d)", MAX_TASKS);
    return ESP_ERR_NO_MEM;
  }

  // Crear la tarea usando FreeRTOS estándar
  TaskHandle_t task_handle = NULL;
  BaseType_t result =
      xTaskCreate(task_func, name, stack_size, params, priority, &task_handle);

  if (result != pdPASS || task_handle == NULL) {
    xSemaphoreGive(task_manager_mutex);
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

  ESP_LOGI(TAG, "Tarea creada: '%s' (stack=%d, priority=%d, #%lu)", name,
           stack_size, priority, task_count);

  xSemaphoreGive(task_manager_mutex);
  return ESP_OK;
}

esp_err_t task_manager_delete(TaskHandle_t handle) {
  if (handle == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  if (task_manager_mutex == NULL) {
    return ESP_ERR_INVALID_STATE;
  }

  if (xSemaphoreTake(task_manager_mutex, portMAX_DELAY) != pdTRUE) {
    return ESP_ERR_TIMEOUT;
  }

  // Buscar tarea en el registry
  for (uint32_t i = 0; i < task_count; i++) {
    if (task_registry[i].handle == handle) {
      ESP_LOGI(TAG, "Tarea eliminada: '%s'", task_registry[i].name ? task_registry[i].name : "unknown");
      
      // Compactar el registry PRIMERO antes de llamar a vTaskDelete
      // Esto previene que si una tarea se elimina a sí misma, la compactación se omita
      for (uint32_t j = i; j < task_count - 1; j++) {
        task_registry[j] = task_registry[j + 1];
      }
      memset(&task_registry[task_count - 1], 0, sizeof(task_info_t));
      task_count--;

      xSemaphoreGive(task_manager_mutex);
      vTaskDelete(handle);
      return ESP_OK;
    }
  }

  xSemaphoreGive(task_manager_mutex);
  ESP_LOGW(TAG, "Tarea no encontrada en registry");
  return ESP_ERR_NOT_FOUND;
}

void task_manager_list_all(void) {
  if (task_manager_mutex && xSemaphoreTake(task_manager_mutex, portMAX_DELAY) == pdTRUE) {
    ESP_LOGI(TAG, "Total de tareas: %lu / %d", task_count, MAX_TASKS);
    ESP_LOGI(TAG, "");

    for (uint32_t i = 0; i < task_count; i++) {
      task_info_t* info = &task_registry[i];
      const char* status = info->is_running ? "RUN" : "STOP";
      uint32_t uptime_sec =
          (xTaskGetTickCount() * portTICK_PERIOD_MS - info->created_at_ms) / 1000;

      ESP_LOGI(TAG, "[%2lu] %s %-20s | Pri:%2d | Stack:%5d | Uptime:%lus", i + 1,
               status, info->name ? info->name : "unknown", info->priority, info->stack_size, uptime_sec);
    }
    xSemaphoreGive(task_manager_mutex);
  }
}

uint32_t task_manager_get_count(void) {
  uint32_t count = 0;
  if (task_manager_mutex && xSemaphoreTake(task_manager_mutex, portMAX_DELAY) == pdTRUE) {
    count = task_count;
    xSemaphoreGive(task_manager_mutex);
  } else {
    count = task_count;
  }
  return count;
}

task_info_t* task_manager_get_info(TaskHandle_t handle) {
  if (task_manager_mutex && xSemaphoreTake(task_manager_mutex, portMAX_DELAY) == pdTRUE) {
    for (uint32_t i = 0; i < task_count; i++) {
      if (task_registry[i].handle == handle) {
        task_info_t* info = &task_registry[i];
        xSemaphoreGive(task_manager_mutex);
        return info;
      }
    }
    xSemaphoreGive(task_manager_mutex);
  }
  return NULL;
}

bool task_manager_get_info_copy(TaskHandle_t handle, task_info_t* out_info) {
  if (out_info == NULL || task_manager_mutex == NULL) {
    return false;
  }
  if (xSemaphoreTake(task_manager_mutex, portMAX_DELAY) == pdTRUE) {
    for (uint32_t i = 0; i < task_count; i++) {
      if (task_registry[i].handle == handle) {
        *out_info = task_registry[i];
        xSemaphoreGive(task_manager_mutex);
        return true;
      }
    }
    xSemaphoreGive(task_manager_mutex);
  }
  return false;
}

void task_manager_update_watermarks(void) {
  if (task_manager_mutex && xSemaphoreTake(task_manager_mutex, portMAX_DELAY) == pdTRUE) {
    for (uint32_t i = 0; i < task_count; i++) {
      if (task_registry[i].is_running && task_registry[i].handle != NULL) {
        eTaskState state = eTaskGetState(task_registry[i].handle);
        if (state == eDeleted) {
          task_registry[i].is_running = false;
          continue;
        }
        task_registry[i].stack_watermark =
            uxTaskGetStackHighWaterMark(task_registry[i].handle);
      }
    }
    xSemaphoreGive(task_manager_mutex);
  }
}

void task_manager_print_stack_usage(void) {
  task_manager_update_watermarks();
  if (task_manager_mutex && xSemaphoreTake(task_manager_mutex, portMAX_DELAY) == pdTRUE) {
    for (uint32_t i = 0; i < task_count; i++) {
      if (!task_registry[i].is_running)
        continue;

      task_info_t* info = &task_registry[i];
      UBaseType_t watermark = info->stack_watermark;
      size_t watermark_bytes = watermark * sizeof(StackType_t);
      size_t used = (info->stack_size > watermark_bytes)
                        ? (info->stack_size - watermark_bytes)
                        : 0;
      float usage_percent = info->stack_size > 0 ? (((float) used / (float) info->stack_size) * 100.0f) : 0.0f;

      const char* status;
      if (watermark < 128) {
        status = "DANGER";  // Menos de 512 bytes libres
      } else if (watermark < 256) {
        status = "WARNING";  // Menos de 1KB libre
      } else {
        status = "OK";
      }

      ESP_LOGI(TAG, "[%2lu] %-20s | %5zu/%5d bytes (%.1f%%) | %s", i + 1,
               info->name ? info->name : "unknown", used, info->stack_size, usage_percent, status);
    }
    xSemaphoreGive(task_manager_mutex);
  }
}

bool task_manager_check_stack_overflow_risk(void) {
  task_manager_update_watermarks();
  bool risk = false;

  if (task_manager_mutex && xSemaphoreTake(task_manager_mutex, portMAX_DELAY) == pdTRUE) {
    for (uint32_t i = 0; i < task_count; i++) {
      if (task_registry[i].is_running) {
        if (task_registry[i].stack_watermark < 128) {  // < 512 bytes
          ESP_LOGE(TAG, "Stack overflow risk: '%s' (solo %lu bytes libres)",
                   task_registry[i].name ? task_registry[i].name : "unknown",
                   task_registry[i].stack_watermark * sizeof(StackType_t));
          risk = true;
          break;
        }
      }
    }
    xSemaphoreGive(task_manager_mutex);
  }

  return risk;
}

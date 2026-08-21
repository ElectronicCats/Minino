#include "error_handler.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static const char* TAG = "error_handler";

// Estado del handler
static bool handler_initialized = false;
static bool auto_restart_enabled = true;
static error_callback_t error_callback = NULL;
static void (*restart_callback)(void) = NULL;
static SemaphoreHandle_t error_handler_mutex = NULL;

// Estadísticas
static error_stats_t stats = {0};

// Nombres de componentes para logging
static const char* component_names[] = {"WiFi",   "BLE",     "GPS",   "Zigbee",
                                        "Thread", "SD Card", "Flash", "UI",
                                        "System", "Other"};
#define COMPONENT_NAMES_COUNT (sizeof(component_names) / sizeof(component_names[0]))

esp_err_t error_handler_init(void) {
  if (handler_initialized) {
    ESP_LOGW(TAG, "Error Handler ya inicializado");
    return ESP_OK;
  }

  if (error_handler_mutex == NULL) {
    error_handler_mutex = xSemaphoreCreateMutex();
    if (error_handler_mutex == NULL) {
      ESP_LOGE(TAG, "Error creando mutex para Error Handler");
      return ESP_ERR_NO_MEM;
    }
  }

  // Resetear estadísticas
  stats = (error_stats_t) {0};

  ESP_LOGI(TAG, "Error Handler inicializado");
  handler_initialized = true;

  return ESP_OK;
}

void error_handler_report(const error_info_t* error) {
  if (!handler_initialized) {
    ESP_LOGE(TAG, "Error Handler no inicializado!");
    return;
  }

  if (error == NULL) {
    ESP_LOGE(TAG, "Error info es NULL");
    return;
  }

  if (error_handler_mutex && xSemaphoreTake(error_handler_mutex, portMAX_DELAY) == pdTRUE) {
    // Actualizar estadísticas
    stats.total_errors++;
    switch (error->severity) {
      case ERROR_SEVERITY_INFO:
        stats.info_count++;
        break;
      case ERROR_SEVERITY_WARNING:
        stats.warning_count++;
        break;
      case ERROR_SEVERITY_ERROR:
        stats.error_count++;
        break;
      case ERROR_SEVERITY_CRITICAL:
        stats.critical_count++;
        break;
    }
    xSemaphoreGive(error_handler_mutex);
  }

  // Determinar emoji y nivel de log con bounds check
  const char* severity_emoji[] = {"ℹ️ ", "⚠️ ", "❌", "🚨"};
  const char* emoji = (error->severity < 4) ? severity_emoji[error->severity] : "❓";

  // Log estructurado con bounds check
  const char* component_name = (error->component < COMPONENT_NAMES_COUNT) 
                                   ? component_names[error->component] 
                                   : "Unknown";
  const char* error_name = esp_err_to_name(error->error_code);
  const char* message = error->message ? error->message : "No message";

  // Log según severidad
  switch (error->severity) {
    case ERROR_SEVERITY_INFO:
      ESP_LOGI(TAG, "%s [%s] %s (%s) - %s", emoji,
               component_name, message, error_name,
               error->file ? error->file : "");
      break;

    case ERROR_SEVERITY_WARNING:
      ESP_LOGW(TAG, "%s [%s] %s (%s) at %s:%d", emoji,
               component_name, message, error_name,
               error->file ? error->file : "unknown", error->line);
      break;

    case ERROR_SEVERITY_ERROR:
    case ERROR_SEVERITY_CRITICAL:
    default:
      ESP_LOGE(TAG, "%s [%s] %s (%s) at %s:%d", emoji,
               component_name, message, error_name,
               error->file ? error->file : "unknown", error->line);
      break;
  }

  // Intentar recuperación si existe función
  if (error->recovery_func != NULL) {
    ESP_LOGI(TAG, "🔧 Intentando recuperación automática...");
    if (error_handler_mutex && xSemaphoreTake(error_handler_mutex, portMAX_DELAY) == pdTRUE) {
      stats.recoveries_attempted++;
      xSemaphoreGive(error_handler_mutex);
    }

    error->recovery_func();

    if (error_handler_mutex && xSemaphoreTake(error_handler_mutex, portMAX_DELAY) == pdTRUE) {
      stats.recoveries_successful++;
      xSemaphoreGive(error_handler_mutex);
    }
    ESP_LOGI(TAG, "✅ Recuperación exitosa");
  }

  // Llamar callback personalizado si existe
  if (error_callback != NULL) {
    error_callback(error);
  }

  // Manejar restart si es crítico
  if (error->severity == ERROR_SEVERITY_CRITICAL && error->requires_restart) {
    ESP_LOGE(TAG, "💥 Error crítico requiere reinicio del sistema");

    if (restart_callback != NULL) {
      ESP_LOGI(TAG, "Ejecutando callback pre-restart...");
      restart_callback();
    }

    if (auto_restart_enabled) {
      if (error_handler_mutex && xSemaphoreTake(error_handler_mutex, portMAX_DELAY) == pdTRUE) {
        stats.restarts_triggered++;
        xSemaphoreGive(error_handler_mutex);
      }
      ESP_LOGE(TAG, "🔄 Reiniciando en 3 segundos...");
      vTaskDelay(pdMS_TO_TICKS(3000));
      esp_restart();
    } else {
      ESP_LOGW(TAG, "Auto-restart deshabilitado, requiere intervención manual");
    }
  }
}

void error_handler_set_callback(error_callback_t callback) {
  error_callback = callback;
}

void error_handler_set_restart_callback(void (*callback)(void)) {
  restart_callback = callback;
}

error_stats_t error_handler_get_stats(void) {
  error_stats_t s = {0};
  if (error_handler_mutex && xSemaphoreTake(error_handler_mutex, portMAX_DELAY) == pdTRUE) {
    s = stats;
    xSemaphoreGive(error_handler_mutex);
  } else {
    s = stats;
  }
  return s;
}

void error_handler_print_stats(void) {
  error_stats_t s = error_handler_get_stats();
  ESP_LOGI(TAG, "════════════════════════════════════════");
  ESP_LOGI(TAG, "  ERROR HANDLER - Estadísticas");
  ESP_LOGI(TAG, "════════════════════════════════════════");
  ESP_LOGI(TAG, "  Total errores:        %lu", s.total_errors);
  ESP_LOGI(TAG, "  ├─ Info:              %lu", s.info_count);
  ESP_LOGI(TAG, "  ├─ Warnings:          %lu", s.warning_count);
  ESP_LOGI(TAG, "  ├─ Errors:            %lu", s.error_count);
  ESP_LOGI(TAG, "  └─ Critical:          %lu", s.critical_count);
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "  Recuperaciones:       %lu / %lu intentadas",
           s.recoveries_successful, s.recoveries_attempted);
  ESP_LOGI(TAG, "  Reinicios provocados: %lu", s.restarts_triggered);
  ESP_LOGI(TAG, "════════════════════════════════════════");
}

void error_handler_reset_stats(void) {
  if (error_handler_mutex && xSemaphoreTake(error_handler_mutex, portMAX_DELAY) == pdTRUE) {
    stats = (error_stats_t) {0};
    xSemaphoreGive(error_handler_mutex);
  } else {
    stats = (error_stats_t) {0};
  }
  ESP_LOGI(TAG, "Estadísticas reseteadas");
}

void error_handler_set_auto_restart(bool enabled) {
  auto_restart_enabled = enabled;
  ESP_LOGI(TAG, "Auto-restart %s", enabled ? "habilitado" : "deshabilitado");
}

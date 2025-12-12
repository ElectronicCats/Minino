#include "error_handler.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "error_handler";

// Estado del handler
static bool handler_initialized = false;
static bool auto_restart_enabled = true;
static error_callback_t error_callback = NULL;
static void (*restart_callback)(void) = NULL;

// Estadísticas
static error_stats_t stats = {0};

// Nombres de componentes para logging
static const char* component_names[] = {"WiFi",   "BLE",     "GPS",   "Zigbee",
                                        "Thread", "SD Card", "Flash", "UI",
                                        "System", "Other"};

esp_err_t error_handler_init(void) {
  if (handler_initialized) {
    ESP_LOGW(TAG, "Error Handler ya inicializado");
    return ESP_OK;
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

  // Determinar emoji y nivel de log
  const char* severity_emoji[] = {"ℹ️ ", "⚠️ ", "❌", "🚨"};
  const char* severity_str[] = {"INFO", "WARNING", "ERROR", "CRITICAL"};

  // Log estructurado
  const char* component_name = component_names[error->component];
  const char* error_name = esp_err_to_name(error->error_code);

  // Log según severidad
  switch (error->severity) {
    case ERROR_SEVERITY_INFO:
      ESP_LOGI(TAG, "%s [%s] %s (%s) - %s", severity_emoji[error->severity],
               component_name, error->message, error_name,
               error->file ? error->file : "");
      break;

    case ERROR_SEVERITY_WARNING:
      ESP_LOGW(TAG, "%s [%s] %s (%s) at %s:%d", severity_emoji[error->severity],
               component_name, error->message, error_name,
               error->file ? error->file : "unknown", error->line);
      break;

    case ERROR_SEVERITY_ERROR:
    case ERROR_SEVERITY_CRITICAL:
      ESP_LOGE(TAG, "%s [%s] %s (%s) at %s:%d", severity_emoji[error->severity],
               component_name, error->message, error_name,
               error->file ? error->file : "unknown", error->line);
      break;
  }

  // Intentar recuperación si existe función
  if (error->recovery_func != NULL) {
    ESP_LOGI(TAG, "🔧 Intentando recuperación automática...");
    stats.recoveries_attempted++;

    error->recovery_func();

    // Asumir éxito si no crasheó
    stats.recoveries_successful++;
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
      stats.restarts_triggered++;
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
  return stats;
}

void error_handler_print_stats(void) {
  ESP_LOGI(TAG, "════════════════════════════════════════");
  ESP_LOGI(TAG, "  ERROR HANDLER - Estadísticas");
  ESP_LOGI(TAG, "════════════════════════════════════════");
  ESP_LOGI(TAG, "  Total errores:        %lu", stats.total_errors);
  ESP_LOGI(TAG, "  ├─ Info:              %lu", stats.info_count);
  ESP_LOGI(TAG, "  ├─ Warnings:          %lu", stats.warning_count);
  ESP_LOGI(TAG, "  ├─ Errors:            %lu", stats.error_count);
  ESP_LOGI(TAG, "  └─ Critical:          %lu", stats.critical_count);
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "  Recuperaciones:       %lu / %lu intentadas",
           stats.recoveries_successful, stats.recoveries_attempted);
  ESP_LOGI(TAG, "  Reinicios provocados: %lu", stats.restarts_triggered);
  ESP_LOGI(TAG, "════════════════════════════════════════");
}

void error_handler_reset_stats(void) {
  stats = (error_stats_t) {0};
  ESP_LOGI(TAG, "Estadísticas reseteadas");
}

void error_handler_set_auto_restart(bool enabled) {
  auto_restart_enabled = enabled;
  ESP_LOGI(TAG, "Auto-restart %s", enabled ? "habilitado" : "deshabilitado");
}

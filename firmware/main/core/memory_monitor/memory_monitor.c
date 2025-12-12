#include "memory_monitor.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "heap_monitor";

// Estado del monitor
static bool monitor_initialized = false;
static bool monitor_running = false;
static esp_timer_handle_t monitor_timer = NULL;
static heap_alert_callback_t alert_callback = NULL;

// Umbrales de alerta (en bytes)
static size_t threshold_warning = 50 * 1024;    // 50 KB
static size_t threshold_critical = 30 * 1024;   // 30 KB
static size_t threshold_emergency = 20 * 1024;  // 20 KB

// Última alerta para evitar spam
static heap_alert_level_t last_alert_level = HEAP_ALERT_NONE;

/**
 * @brief Calcula el porcentaje de fragmentación del heap
 */
static float calculate_fragmentation(size_t total_free, size_t largest_block) {
  if (total_free == 0)
    return 100.0f;
  return ((float) (total_free - largest_block) / (float) total_free) * 100.0f;
}

/**
 * @brief Determina el nivel de alerta basado en memoria libre
 */
static heap_alert_level_t get_alert_level_from_free(size_t free_bytes) {
  if (free_bytes < threshold_emergency) {
    return HEAP_ALERT_EMERGENCY;
  } else if (free_bytes < threshold_critical) {
    return HEAP_ALERT_CRITICAL;
  } else if (free_bytes < threshold_warning) {
    return HEAP_ALERT_WARNING;
  }
  return HEAP_ALERT_NONE;
}

/**
 * @brief Callback del timer de monitoreo
 */
static void monitor_timer_callback(void* arg) {
  heap_stats_t stats = heap_monitor_get_stats();
  heap_alert_level_t current_level =
      get_alert_level_from_free(stats.total_free);

  // Solo alertar si el nivel cambió
  if (current_level != last_alert_level && current_level != HEAP_ALERT_NONE) {
    switch (current_level) {
      case HEAP_ALERT_WARNING:
        ESP_LOGW(TAG, "⚠️  Memoria baja: %zu KB libre (fragmentación: %.1f%%)",
                 stats.total_free / 1024, stats.fragmentation_percent);
        break;
      case HEAP_ALERT_CRITICAL:
        ESP_LOGE(TAG,
                 "🔴 Memoria crítica: %zu KB libre! Considera liberar recursos",
                 stats.total_free / 1024);
        break;
      case HEAP_ALERT_EMERGENCY:
        ESP_LOGE(TAG, "🚨 EMERGENCIA: Solo %zu KB libres! Sistema inestable",
                 stats.total_free / 1024);
        break;
      default:
        break;
    }

    // Llamar callback personalizado si existe
    if (alert_callback != NULL) {
      alert_callback(current_level, &stats);
    }
  }

  last_alert_level = current_level;
}

esp_err_t heap_monitor_init(void) {
  if (monitor_initialized) {
    ESP_LOGW(TAG, "Monitor ya inicializado");
    return ESP_OK;
  }

  ESP_LOGI(TAG, "Inicializando Memory Monitor");

  // Obtener estadísticas iniciales
  heap_stats_t initial_stats = heap_monitor_get_stats();
  ESP_LOGI(TAG, "Memoria inicial: %zu KB libre, bloque más grande: %zu KB",
           initial_stats.total_free / 1024,
           initial_stats.largest_free_block / 1024);

  monitor_initialized = true;
  return ESP_OK;
}

esp_err_t heap_monitor_start(uint32_t interval_ms) {
  if (!monitor_initialized) {
    ESP_LOGE(TAG,
             "Monitor no inicializado. Llamar heap_monitor_init() primero");
    return ESP_ERR_INVALID_STATE;
  }

  if (monitor_running) {
    ESP_LOGW(TAG, "Monitor ya está corriendo");
    return ESP_OK;
  }

  // Crear timer periódico
  const esp_timer_create_args_t timer_args = {
      .callback = monitor_timer_callback, .arg = NULL, .name = "heap_monitor"};

  esp_err_t err = esp_timer_create(&timer_args, &monitor_timer);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error creando timer: %s", esp_err_to_name(err));
    return err;
  }

  err = esp_timer_start_periodic(monitor_timer,
                                 interval_ms * 1000);  // ms a microsegundos
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error iniciando timer: %s", esp_err_to_name(err));
    esp_timer_delete(monitor_timer);
    monitor_timer = NULL;
    return err;
  }

  monitor_running = true;
  ESP_LOGI(TAG, "Monitor iniciado (intervalo: %lu ms)", interval_ms);

  return ESP_OK;
}

void heap_monitor_stop(void) {
  if (!monitor_running) {
    return;
  }

  if (monitor_timer != NULL) {
    esp_timer_stop(monitor_timer);
    esp_timer_delete(monitor_timer);
    monitor_timer = NULL;
  }

  monitor_running = false;
  ESP_LOGI(TAG, "Monitor detenido");
}

heap_stats_t heap_monitor_get_stats(void) {
  heap_stats_t stats = {0};

  // Obtener estadísticas del heap principal (MALLOC_CAP_8BIT)
  stats.total_free = heap_caps_get_free_size(MALLOC_CAP_8BIT);
  stats.min_free_ever = heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT);
  stats.largest_free_block = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);

  // Calcular memoria total alocada
  multi_heap_info_t heap_info;
  heap_caps_get_info(&heap_info, MALLOC_CAP_8BIT);
  stats.total_allocated = heap_info.total_allocated_bytes;

  // Calcular fragmentación
  stats.fragmentation_percent =
      calculate_fragmentation(stats.total_free, stats.largest_free_block);

  return stats;
}

void heap_monitor_set_alert_callback(heap_alert_callback_t callback) {
  alert_callback = callback;
}

void heap_monitor_set_thresholds(size_t warning_kb,
                                 size_t critical_kb,
                                 size_t emergency_kb) {
  threshold_warning = warning_kb * 1024;
  threshold_critical = critical_kb * 1024;
  threshold_emergency = emergency_kb * 1024;

  ESP_LOGI(
      TAG,
      "Umbrales actualizados: Warning=%zuKB, Critical=%zuKB, Emergency=%zuKB",
      warning_kb, critical_kb, emergency_kb);
}

void heap_monitor_print_stats(void) {
  heap_stats_t stats = heap_monitor_get_stats();
  heap_alert_level_t level = get_alert_level_from_free(stats.total_free);

  const char* level_str[] = {"NORMAL", "WARNING", "CRITICAL", "EMERGENCY"};
  const char* level_emoji[] = {"✅", "⚠️ ", "🔴", "🚨"};

  ESP_LOGI(TAG, "════════════════════════════════════════");
  ESP_LOGI(TAG, "  MEMORY MONITOR - Estado del Heap");
  ESP_LOGI(TAG, "════════════════════════════════════════");
  ESP_LOGI(TAG, "  Estado:           %s %s", level_emoji[level],
           level_str[level]);
  ESP_LOGI(TAG, "  Libre actual:     %zu KB", stats.total_free / 1024);
  ESP_LOGI(TAG, "  Mínimo histórico: %zu KB", stats.min_free_ever / 1024);
  ESP_LOGI(TAG, "  Bloque más grande:%zu KB", stats.largest_free_block / 1024);
  ESP_LOGI(TAG, "  Total alocado:    %zu KB", stats.total_allocated / 1024);
  ESP_LOGI(TAG, "  Fragmentación:    %.1f%%", stats.fragmentation_percent);
  ESP_LOGI(TAG, "════════════════════════════════════════");
}

heap_alert_level_t heap_monitor_get_alert_level(void) {
  heap_stats_t stats = heap_monitor_get_stats();
  return get_alert_level_from_free(stats.total_free);
}

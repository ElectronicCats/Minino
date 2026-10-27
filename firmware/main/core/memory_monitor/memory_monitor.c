#include "memory_monitor.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static const char* TAG = "heap_monitor";

// Monitor state
static bool monitor_initialized = false;
static bool monitor_running = false;
static esp_timer_handle_t monitor_timer = NULL;
static heap_alert_callback_t alert_callback = NULL;
static SemaphoreHandle_t monitor_mutex = NULL;

// Alert thresholds (in bytes)
static size_t threshold_warning = 50 * 1024;    // 50 KB
static size_t threshold_critical = 30 * 1024;   // 30 KB
static size_t threshold_emergency = 20 * 1024;  // 20 KB

// Last alert level to avoid spam
static heap_alert_level_t last_alert_level = HEAP_ALERT_NONE;

/**
 * @brief Calculate heap fragmentation percentage
 */
static float calculate_fragmentation(size_t total_free, size_t largest_block) {
  if (total_free == 0 || largest_block >= total_free) {
    return (total_free == 0) ? 100.0f : 0.0f;
  }
  return ((float) (total_free - largest_block) / (float) total_free) * 100.0f;
}

/**
 * @brief Determine alert level based on free memory
 */
static heap_alert_level_t get_alert_level_from_free(size_t free_bytes) {
  size_t w, c, e;
  if (monitor_mutex && xSemaphoreTake(monitor_mutex, portMAX_DELAY) == pdTRUE) {
    w = threshold_warning;
    c = threshold_critical;
    e = threshold_emergency;
    xSemaphoreGive(monitor_mutex);
  } else {
    w = threshold_warning;
    c = threshold_critical;
    e = threshold_emergency;
  }

  if (free_bytes < e) {
    return HEAP_ALERT_EMERGENCY;
  } else if (free_bytes < c) {
    return HEAP_ALERT_CRITICAL;
  } else if (free_bytes < w) {
    return HEAP_ALERT_WARNING;
  }
  return HEAP_ALERT_NONE;
}

/**
 * @brief Monitoring timer callback
 */
static void monitor_timer_callback(void* arg) {
  heap_stats_t stats = heap_monitor_get_stats();
  heap_alert_level_t current_level =
      get_alert_level_from_free(stats.total_free);

  // Only alert if level changed
  if (current_level != last_alert_level && current_level != HEAP_ALERT_NONE) {
    switch (current_level) {
      case HEAP_ALERT_WARNING:
        ESP_LOGW(TAG, "⚠️  Low memory: %zu KB free (fragmentation: %.1f%%)",
                 stats.total_free / 1024, stats.fragmentation_percent);
        break;
      case HEAP_ALERT_CRITICAL:
        ESP_LOGE(TAG,
                 "🔴 Critical memory: %zu KB free! Consider freeing resources",
                 stats.total_free / 1024);
        break;
      case HEAP_ALERT_EMERGENCY:
        ESP_LOGE(TAG, "🚨 EMERGENCY: Only %zu KB free! System unstable",
                 stats.total_free / 1024);
        break;
      default:
        break;
    }

    // Call custom callback if registered
    heap_alert_callback_t saved_cb = NULL;
    if (monitor_mutex &&
        xSemaphoreTake(monitor_mutex, portMAX_DELAY) == pdTRUE) {
      saved_cb = alert_callback;
      xSemaphoreGive(monitor_mutex);
    }
    if (saved_cb != NULL) {
      saved_cb(current_level, &stats);
    }
  }

  last_alert_level = current_level;
}

esp_err_t heap_monitor_init(void) {
  if (monitor_initialized) {
    ESP_LOGW(TAG, "Monitor already initialized");
    return ESP_OK;
  }

  ESP_LOGI(TAG, "Initializing Memory Monitor");

  if (monitor_mutex == NULL) {
    monitor_mutex = xSemaphoreCreateMutex();
    if (monitor_mutex == NULL) {
      ESP_LOGE(TAG, "Error creating mutex for Memory Monitor");
      return ESP_ERR_NO_MEM;
    }
  }

  // Get initial statistics
  heap_stats_t initial_stats = heap_monitor_get_stats();
  ESP_LOGI(TAG, "Initial memory: %zu KB free, largest block: %zu KB",
           initial_stats.total_free / 1024,
           initial_stats.largest_free_block / 1024);

  monitor_initialized = true;
  return ESP_OK;
}

esp_err_t heap_monitor_start(uint32_t interval_ms) {
  if (!monitor_initialized) {
    ESP_LOGE(TAG, "Monitor not initialized. Call heap_monitor_init() first");
    return ESP_ERR_INVALID_STATE;
  }

  if (monitor_mutex && xSemaphoreTake(monitor_mutex, portMAX_DELAY) != pdTRUE) {
    return ESP_ERR_TIMEOUT;
  }

  if (monitor_running) {
    if (monitor_mutex) {
      xSemaphoreGive(monitor_mutex);
    }
    ESP_LOGW(TAG, "Monitor is already running");
    return ESP_OK;
  }

  // Create periodic timer
  const esp_timer_create_args_t timer_args = {
      .callback = monitor_timer_callback, .arg = NULL, .name = "heap_monitor"};

  esp_err_t err = esp_timer_create(&timer_args, &monitor_timer);
  if (err != ESP_OK) {
    if (monitor_mutex) {
      xSemaphoreGive(monitor_mutex);
    }
    ESP_LOGE(TAG, "Error creating timer: %s", esp_err_to_name(err));
    return err;
  }

  err = esp_timer_start_periodic(monitor_timer,
                                 interval_ms * 1000);  // ms to microseconds
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error starting timer: %s", esp_err_to_name(err));
    esp_timer_delete(monitor_timer);
    monitor_timer = NULL;
    if (monitor_mutex) {
      xSemaphoreGive(monitor_mutex);
    }
    return err;
  }

  monitor_running = true;
  ESP_LOGI(TAG, "Monitor started (interval: %lu ms)", interval_ms);

  if (monitor_mutex) {
    xSemaphoreGive(monitor_mutex);
  }
  return ESP_OK;
}

void heap_monitor_stop(void) {
  if (monitor_mutex && xSemaphoreTake(monitor_mutex, portMAX_DELAY) != pdTRUE) {
    return;
  }

  if (!monitor_running) {
    if (monitor_mutex) {
      xSemaphoreGive(monitor_mutex);
    }
    return;
  }

  if (monitor_timer != NULL) {
    esp_timer_stop(monitor_timer);
    esp_timer_delete(monitor_timer);
    monitor_timer = NULL;
  }

  monitor_running = false;
  ESP_LOGI(TAG, "Monitor stopped");

  if (monitor_mutex) {
    xSemaphoreGive(monitor_mutex);
  }
}

heap_stats_t heap_monitor_get_stats(void) {
  heap_stats_t stats = {0};

  // Get main heap statistics (MALLOC_CAP_8BIT)
  stats.total_free = heap_caps_get_free_size(MALLOC_CAP_8BIT);
  stats.min_free_ever = heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT);
  stats.largest_free_block = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);

  // Calculate total allocated memory
  multi_heap_info_t heap_info;
  heap_caps_get_info(&heap_info, MALLOC_CAP_8BIT);
  stats.total_allocated = heap_info.total_allocated_bytes;

  // Calculate fragmentation
  stats.fragmentation_percent =
      calculate_fragmentation(stats.total_free, stats.largest_free_block);

  return stats;
}

void heap_monitor_set_alert_callback(heap_alert_callback_t callback) {
  if (monitor_mutex && xSemaphoreTake(monitor_mutex, portMAX_DELAY) == pdTRUE) {
    alert_callback = callback;
    xSemaphoreGive(monitor_mutex);
  }
}

void heap_monitor_set_thresholds(size_t warning_kb,
                                 size_t critical_kb,
                                 size_t emergency_kb) {
  if (monitor_mutex && xSemaphoreTake(monitor_mutex, portMAX_DELAY) == pdTRUE) {
    threshold_warning = warning_kb * 1024;
    threshold_critical = critical_kb * 1024;
    threshold_emergency = emergency_kb * 1024;
    xSemaphoreGive(monitor_mutex);
  }

  ESP_LOGI(TAG,
           "Thresholds updated: Warning=%zuKB, Critical=%zuKB, Emergency=%zuKB",
           warning_kb, critical_kb, emergency_kb);
}

void heap_monitor_print_stats(void) {
  heap_stats_t stats = heap_monitor_get_stats();
  heap_alert_level_t level = get_alert_level_from_free(stats.total_free);

  const char* level_str[] = {"NORMAL", "WARNING", "CRITICAL", "EMERGENCY"};
  const char* level_emoji[] = {"✅", "⚠️ ", "🔴", "🚨"};

  ESP_LOGI(TAG, "════════════════════════════════════════");
  ESP_LOGI(TAG, "  MEMORY MONITOR - Heap Status");
  ESP_LOGI(TAG, "════════════════════════════════════════");
  ESP_LOGI(TAG, "  Status:           %s %s", level_emoji[level],
           level_str[level]);
  ESP_LOGI(TAG, "  Current free:     %zu KB", stats.total_free / 1024);
  ESP_LOGI(TAG, "  Historical min:   %zu KB", stats.min_free_ever / 1024);
  ESP_LOGI(TAG, "  Largest block:    %zu KB", stats.largest_free_block / 1024);
  ESP_LOGI(TAG, "  Total allocated:  %zu KB", stats.total_allocated / 1024);
  ESP_LOGI(TAG, "  Fragmentation:    %.1f%%", stats.fragmentation_percent);
  ESP_LOGI(TAG, "════════════════════════════════════════");
}

heap_alert_level_t heap_monitor_get_alert_level(void) {
  heap_stats_t stats = heap_monitor_get_stats();
  return get_alert_level_from_free(stats.total_free);
}

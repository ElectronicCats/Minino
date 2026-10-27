#pragma once

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

/**
 * @file memory_monitor.h
 * @brief Real-time heap memory monitoring system
 *
 * Provides passive monitoring and automatic alerts when available memory
 * drops below critical levels.
 */

/**
 * @brief Heap statistics
 */
typedef struct {
  size_t total_free;            ///< Current free memory (bytes)
  size_t min_free_ever;         ///< Historical minimum free memory
  size_t largest_free_block;    ///< Largest available contiguous block
  size_t total_allocated;       ///< Total allocated memory
  float fragmentation_percent;  ///< Fragmentation percentage (0-100)
} heap_stats_t;

/**
 * @brief Memory alert levels
 */
typedef enum {
  HEAP_ALERT_NONE = 0,  ///< Normal memory (>50KB)
  HEAP_ALERT_WARNING,   ///< Warning (30-50KB)
  HEAP_ALERT_CRITICAL,  ///< Critical (20-30KB)
  HEAP_ALERT_EMERGENCY  ///< Emergency (<20KB)
} heap_alert_level_t;

/**
 * @brief Callback for memory alerts
 *
 * @param level Alert level
 * @param stats Current heap statistics
 */
typedef void (*heap_alert_callback_t)(heap_alert_level_t level,
                                      const heap_stats_t* stats);

/**
 * @brief Initialize the memory monitoring system
 *
 * @return ESP_OK on success
 */
esp_err_t heap_monitor_init(void);

/**
 * @brief Start periodic memory monitoring
 *
 * @param interval_ms Sampling interval in milliseconds (recommended: 5000)
 * @return ESP_OK on success
 */
esp_err_t heap_monitor_start(uint32_t interval_ms);

/**
 * @brief Stop periodic monitoring
 */
void heap_monitor_stop(void);

/**
 * @brief Get current heap statistics
 *
 * @return Structure with current statistics
 */
heap_stats_t heap_monitor_get_stats(void);

/**
 * @brief Register a callback for low memory alerts
 *
 * @param callback Function to call when an alert occurs
 */
void heap_monitor_set_alert_callback(heap_alert_callback_t callback);

/**
 * @brief Configure custom alert thresholds
 *
 * @param warning_kb Warning threshold in KB (default: 50)
 * @param critical_kb Critical threshold in KB (default: 30)
 * @param emergency_kb Emergency threshold in KB (default: 20)
 */
void heap_monitor_set_thresholds(size_t warning_kb,
                                 size_t critical_kb,
                                 size_t emergency_kb);

/**
 * @brief Print memory statistics to console
 */
void heap_monitor_print_stats(void);

/**
 * @brief Get the current alert level
 *
 * @return Alert level based on free memory
 */
heap_alert_level_t heap_monitor_get_alert_level(void);

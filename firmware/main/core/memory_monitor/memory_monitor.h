#pragma once

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

/**
 * @file memory_monitor.h
 * @brief Sistema de monitoreo de memoria heap en tiempo real
 *
 * Proporciona monitoreo pasivo y alertas automáticas cuando la memoria
 * disponible baja de niveles críticos.
 */

/**
 * @brief Estadísticas de heap
 */
typedef struct {
  size_t total_free;            ///< Memoria libre actual (bytes)
  size_t min_free_ever;         ///< Mínimo histórico de memoria libre
  size_t largest_free_block;    ///< Bloque contiguo más grande disponible
  size_t total_allocated;       ///< Total de memoria alocada
  float fragmentation_percent;  ///< Porcentaje de fragmentación (0-100)
} heap_stats_t;

/**
 * @brief Niveles de alerta de memoria
 */
typedef enum {
  HEAP_ALERT_NONE = 0,  ///< Memoria normal (>50KB)
  HEAP_ALERT_WARNING,   ///< Advertencia (30-50KB)
  HEAP_ALERT_CRITICAL,  ///< Crítico (20-30KB)
  HEAP_ALERT_EMERGENCY  ///< Emergencia (<20KB)
} heap_alert_level_t;

/**
 * @brief Callback para alertas de memoria
 *
 * @param level Nivel de alerta
 * @param stats Estadísticas actuales del heap
 */
typedef void (*heap_alert_callback_t)(heap_alert_level_t level,
                                      const heap_stats_t* stats);

/**
 * @brief Inicializa el sistema de monitoreo de memoria
 *
 * @return ESP_OK en caso de éxito
 */
esp_err_t heap_monitor_init(void);

/**
 * @brief Inicia el monitoreo periódico de memoria
 *
 * @param interval_ms Intervalo de muestreo en milisegundos (recomendado: 5000)
 * @return ESP_OK en caso de éxito
 */
esp_err_t heap_monitor_start(uint32_t interval_ms);

/**
 * @brief Detiene el monitoreo periódico
 */
void heap_monitor_stop(void);

/**
 * @brief Obtiene las estadísticas actuales del heap
 *
 * @return Estructura con estadísticas actuales
 */
heap_stats_t heap_monitor_get_stats(void);

/**
 * @brief Registra un callback para alertas de memoria baja
 *
 * @param callback Función a llamar cuando hay alerta
 */
void heap_monitor_set_alert_callback(heap_alert_callback_t callback);

/**
 * @brief Configura los umbrales de alerta personalizados
 *
 * @param warning_kb Umbral de warning en KB (default: 50)
 * @param critical_kb Umbral crítico en KB (default: 30)
 * @param emergency_kb Umbral de emergencia en KB (default: 20)
 */
void heap_monitor_set_thresholds(size_t warning_kb,
                                 size_t critical_kb,
                                 size_t emergency_kb);

/**
 * @brief Imprime estadísticas de memoria en consola
 */
void heap_monitor_print_stats(void);

/**
 * @brief Obtiene el nivel de alerta actual
 *
 * @return Nivel de alerta basado en memoria libre
 */
heap_alert_level_t heap_monitor_get_alert_level(void);

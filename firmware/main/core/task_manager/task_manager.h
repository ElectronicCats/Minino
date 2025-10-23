#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/**
 * @file task_manager.h
 * @brief Sistema centralizado de gestión de tareas FreeRTOS
 *
 * Proporciona una API unificada para crear y monitorear tareas,
 * con prioridades y stack sizes estandarizados.
 */

/**
 * @brief Prioridades estandarizadas de tareas
 *
 * Basado en análisis del proyecto Minino:
 * - CRITICAL: Protocolos time-sensitive (IEEE 802.15.4, Zigbee)
 * - HIGH: GPS, WiFi crítico, BLE GATT
 * - NORMAL: Aplicaciones, scanners, ataques
 * - LOW: UI, animaciones, logging
 * - IDLE: Background, estadísticas
 */
typedef enum {
  TASK_PRIORITY_CRITICAL = 24,  ///< Máxima prioridad (protocolos críticos)
  TASK_PRIORITY_HIGH = 20,      ///< Alta (GPS, WiFi, BLE core)
  TASK_PRIORITY_NORMAL = 15,    ///< Normal (apps, scanners)
  TASK_PRIORITY_LOW = 10,       ///< Baja (UI, LEDs, keyboard)
  TASK_PRIORITY_IDLE = 5        ///< Mínima (background tasks)
} task_priority_t;

/**
 * @brief Tamaños estandarizados de stack
 *
 * Recomendaciones basadas en uso típico:
 * - TINY: Tareas simples sin allocations
 * - SMALL: UI básica, LEDs
 * - MEDIUM: Mayoría de aplicaciones
 * - LARGE: Protocolos complejos (WiFi, BLE, GPS)
 * - HUGE: Zigbee, Thread, OpenThread stack
 */
typedef enum {
  TASK_STACK_TINY = 1024,    ///< 1 KB - Tareas muy simples
  TASK_STACK_SMALL = 2048,   ///< 2 KB - UI, LEDs, animaciones
  TASK_STACK_MEDIUM = 4096,  ///< 4 KB - Apps estándar
  TASK_STACK_LARGE = 8192,   ///< 8 KB - Protocolos complejos
  TASK_STACK_HUGE = 16384    ///< 16 KB - Zigbee, Thread
} task_stack_size_t;

/**
 * @brief Metadata de una tarea registrada
 */
typedef struct {
  const char* name;              ///< Nombre de la tarea
  TaskHandle_t handle;           ///< Handle de FreeRTOS
  task_priority_t priority;      ///< Prioridad asignada
  task_stack_size_t stack_size;  ///< Tamaño de stack
  bool is_running;               ///< Estado actual
  uint32_t created_at_ms;        ///< Timestamp de creación
  UBaseType_t stack_watermark;   ///< Stack libre mínimo (watermark)
} task_info_t;

/**
 * @brief Inicializa el Task Manager
 *
 * Debe llamarse en app_main() antes de crear cualquier tarea
 *
 * @return ESP_OK en caso de éxito
 */
esp_err_t task_manager_init(void);

/**
 * @brief Crea una nueva tarea y la registra en el manager
 *
 * Wrapper sobre xTaskCreate() que estandariza prioridades y stack sizes
 *
 * @param task_func Función de la tarea
 * @param name Nombre descriptivo (usar patrón: module_function)
 * @param stack_size Tamaño de stack (usar enums TASK_STACK_*)
 * @param params Parámetros para la tarea
 * @param priority Prioridad (usar enums TASK_PRIORITY_*)
 * @param handle Puntero para guardar el handle (puede ser NULL)
 * @return ESP_OK en caso de éxito
 */
esp_err_t task_manager_create(TaskFunction_t task_func,
                              const char* name,
                              task_stack_size_t stack_size,
                              void* params,
                              task_priority_t priority,
                              TaskHandle_t* handle);

/**
 * @brief Elimina una tarea y actualiza el registro
 *
 * @param handle Handle de la tarea a eliminar
 * @return ESP_OK en caso de éxito
 */
esp_err_t task_manager_delete(TaskHandle_t handle);

/**
 * @brief Lista todas las tareas registradas en el sistema
 *
 * Imprime información detallada de cada tarea en el log
 */
void task_manager_list_all(void);

/**
 * @brief Obtiene el número total de tareas registradas
 *
 * @return Número de tareas
 */
uint32_t task_manager_get_count(void);

/**
 * @brief Obtiene información de una tarea específica
 *
 * @param handle Handle de la tarea
 * @return Puntero a task_info_t o NULL si no se encuentra
 */
task_info_t* task_manager_get_info(TaskHandle_t handle);

/**
 * @brief Actualiza stack watermarks de todas las tareas
 *
 * Útil para detectar tareas que se están quedando sin stack
 */
void task_manager_update_watermarks(void);

/**
 * @brief Imprime estadísticas de uso de stack de todas las tareas
 *
 * Ayuda a identificar tareas con stack oversized o undersized
 */
void task_manager_print_stack_usage(void);

/**
 * @brief Verifica si alguna tarea está cerca de stack overflow
 *
 * @return true si alguna tarea tiene < 512 bytes libres en stack
 */
bool task_manager_check_stack_overflow_risk(void);

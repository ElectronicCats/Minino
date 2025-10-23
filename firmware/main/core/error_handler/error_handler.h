#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

/**
 * @file error_handler.h
 * @brief Sistema centralizado de manejo de errores
 *
 * Proporciona un sistema unificado para reportar, loggear y recuperarse
 * de errores en todo el proyecto.
 */

/**
 * @brief Severidad del error
 */
typedef enum {
  ERROR_SEVERITY_INFO,     ///< Informativo (solo log)
  ERROR_SEVERITY_WARNING,  ///< Advertencia (log + alerta)
  ERROR_SEVERITY_ERROR,    ///< Error (log + detener feature)
  ERROR_SEVERITY_CRITICAL  ///< Crítico (log + restart)
} error_severity_t;

/**
 * @brief Categorías de componentes
 */
typedef enum {
  ERROR_COMPONENT_WIFI,
  ERROR_COMPONENT_BLE,
  ERROR_COMPONENT_GPS,
  ERROR_COMPONENT_ZIGBEE,
  ERROR_COMPONENT_THREAD,
  ERROR_COMPONENT_SD_CARD,
  ERROR_COMPONENT_FLASH,
  ERROR_COMPONENT_UI,
  ERROR_COMPONENT_SYSTEM,
  ERROR_COMPONENT_OTHER
} error_component_t;

/**
 * @brief Información completa de un error
 */
typedef struct {
  error_severity_t severity;    ///< Severidad del error
  error_component_t component;  ///< Componente afectado
  esp_err_t error_code;         ///< Código de error ESP-IDF
  const char* message;          ///< Mensaje descriptivo
  bool requires_restart;        ///< Si requiere reinicio
  void (*recovery_func)(void);  ///< Función de recuperación (opcional)
  const char* file;             ///< Archivo donde ocurrió (para debug)
  int line;                     ///< Línea donde ocurrió (para debug)
} error_info_t;

/**
 * @brief Estadísticas de errores
 */
typedef struct {
  uint32_t total_errors;           ///< Total de errores reportados
  uint32_t info_count;             ///< Errores informativos
  uint32_t warning_count;          ///< Advertencias
  uint32_t error_count;            ///< Errores
  uint32_t critical_count;         ///< Errores críticos
  uint32_t restarts_triggered;     ///< Reinicios provocados
  uint32_t recoveries_attempted;   ///< Intentos de recuperación
  uint32_t recoveries_successful;  ///< Recuperaciones exitosas
} error_stats_t;

/**
 * @brief Callback para notificaciones de errores
 *
 * @param error Información del error
 */
typedef void (*error_callback_t)(const error_info_t* error);

/**
 * @brief Inicializa el sistema de manejo de errores
 *
 * @return ESP_OK en caso de éxito
 */
esp_err_t error_handler_init(void);

/**
 * @brief Reporta un error al sistema
 *
 * Procesa el error según su severidad:
 * - INFO: Solo log
 * - WARNING: Log + estadísticas
 * - ERROR: Log + intento de recovery
 * - CRITICAL: Log + recovery + posible restart
 *
 * @param error Información del error
 */
void error_handler_report(const error_info_t* error);

/**
 * @brief Macro helper para reportar errores con info de archivo/línea
 */
#define ERROR_REPORT(severity, component, code, msg, restart, recovery) \
  do {                                                                  \
    error_info_t _err = {.severity = severity,                          \
                         .component = component,                        \
                         .error_code = code,                            \
                         .message = msg,                                \
                         .requires_restart = restart,                   \
                         .recovery_func = recovery,                     \
                         .file = __FILE__,                              \
                         .line = __LINE__};                             \
    error_handler_report(&_err);                                        \
  } while (0)

/**
 * @brief Macro simple para errores WiFi
 */
#define ERROR_WIFI(code, msg)                                                \
  ERROR_REPORT(ERROR_SEVERITY_ERROR, ERROR_COMPONENT_WIFI, code, msg, false, \
               NULL)

/**
 * @brief Macro simple para errores críticos
 */
#define ERROR_CRITICAL(component, code, msg) \
  ERROR_REPORT(ERROR_SEVERITY_CRITICAL, component, code, msg, true, NULL)

/**
 * @brief Registra callback para notificaciones de errores
 *
 * @param callback Función a llamar cuando hay un error
 */
void error_handler_set_callback(error_callback_t callback);

/**
 * @brief Registra callback para ejecutar antes de restart
 *
 * Útil para guardar datos antes de reiniciar
 *
 * @param callback Función a llamar antes de esp_restart()
 */
void error_handler_set_restart_callback(void (*callback)(void));

/**
 * @brief Obtiene estadísticas de errores
 *
 * @return Estructura con estadísticas acumuladas
 */
error_stats_t error_handler_get_stats(void);

/**
 * @brief Imprime estadísticas de errores en consola
 */
void error_handler_print_stats(void);

/**
 * @brief Resetea las estadísticas
 */
void error_handler_reset_stats(void);

/**
 * @brief Habilita/deshabilita auto-restart en errores críticos
 *
 * @param enabled true para habilitar auto-restart
 */
void error_handler_set_auto_restart(bool enabled);

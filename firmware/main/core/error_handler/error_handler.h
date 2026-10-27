#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

/**
 * @file error_handler.h
 * @brief Centralized error handling system
 *
 * Provides a unified system to report, log, and recover from errors
 * across the entire project.
 */

/**
 * @brief Error severity
 */
typedef enum {
  ERROR_SEVERITY_INFO,     ///< Informational (log only)
  ERROR_SEVERITY_WARNING,  ///< Warning (log + alert)
  ERROR_SEVERITY_ERROR,    ///< Error (log + stop feature)
  ERROR_SEVERITY_CRITICAL  ///< Critical (log + restart)
} error_severity_t;

/**
 * @brief Component categories
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
 * @brief Complete error information
 */
typedef struct {
  error_severity_t severity;    ///< Error severity
  error_component_t component;  ///< Affected component
  esp_err_t error_code;         ///< ESP-IDF error code
  const char* message;          ///< Descriptive message
  bool requires_restart;        ///< Whether a restart is required
  void (*recovery_func)(void);  ///< Recovery function (optional)
  const char* file;             ///< Source file where it occurred (for debug)
  int line;                     ///< Line number where it occurred (for debug)
} error_info_t;

/**
 * @brief Error statistics
 */
typedef struct {
  uint32_t total_errors;           ///< Total reported errors
  uint32_t info_count;             ///< Informational errors
  uint32_t warning_count;          ///< Warnings
  uint32_t error_count;            ///< Errors
  uint32_t critical_count;         ///< Critical errors
  uint32_t restarts_triggered;     ///< Triggered restarts
  uint32_t recoveries_attempted;   ///< Recovery attempts
  uint32_t recoveries_successful;  ///< Successful recoveries
} error_stats_t;

/**
 * @brief Callback for error notifications
 *
 * @param error Error information
 */
typedef void (*error_callback_t)(const error_info_t* error);

/**
 * @brief Initialize the error handling system
 *
 * @return ESP_OK on success
 */
esp_err_t error_handler_init(void);

/**
 * @brief Report an error to the system
 *
 * Processes the error according to its severity:
 * - INFO: Log only
 * - WARNING: Log + statistics
 * - ERROR: Log + recovery attempt
 * - CRITICAL: Log + recovery + potential restart
 *
 * @param error Error information
 */
void error_handler_report(const error_info_t* error);

/**
 * @brief Helper macro to report errors with file/line info
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
 * @brief Simple macro for WiFi errors
 */
#define ERROR_WIFI(code, msg)                                                \
  ERROR_REPORT(ERROR_SEVERITY_ERROR, ERROR_COMPONENT_WIFI, code, msg, false, \
               NULL)

/**
 * @brief Simple macro for critical errors
 */
#define ERROR_CRITICAL(component, code, msg) \
  ERROR_REPORT(ERROR_SEVERITY_CRITICAL, component, code, msg, true, NULL)

/**
 * @brief Register callback for error notifications
 *
 * @param callback Function to call when an error occurs
 */
void error_handler_set_callback(error_callback_t callback);

/**
 * @brief Register callback to execute before restart
 *
 * Useful for saving data before rebooting
 *
 * @param callback Function to call before esp_restart()
 */
void error_handler_set_restart_callback(void (*callback)(void));

/**
 * @brief Get error statistics
 *
 * @return Structure with accumulated statistics
 */
error_stats_t error_handler_get_stats(void);

/**
 * @brief Print error statistics to console
 */
void error_handler_print_stats(void);

/**
 * @brief Reset error statistics
 */
void error_handler_reset_stats(void);

/**
 * @brief Enable/disable auto-restart on critical errors
 *
 * @param enabled true to enable auto-restart
 */
void error_handler_set_auto_restart(bool enabled);

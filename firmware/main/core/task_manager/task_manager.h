#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/**
 * @file task_manager.h
 * @brief Centralized FreeRTOS task management system
 *
 * Provides a unified API for creating and monitoring tasks,
 * with standardized priorities and stack sizes.
 */

/**
 * @brief Standardized task priorities
 *
 * Based on Minino project analysis:
 * - CRITICAL: Time-sensitive protocols (IEEE 802.15.4, Zigbee)
 * - HIGH: GPS, critical WiFi, BLE GATT
 * - NORMAL: Applications, scanners, attacks
 * - LOW: UI, animations, logging
 * - IDLE: Background, statistics
 */
typedef enum {
  TASK_PRIORITY_CRITICAL = 24,  ///< Maximum priority (critical protocols)
  TASK_PRIORITY_HIGH = 20,      ///< High (GPS, WiFi, BLE core)
  TASK_PRIORITY_NORMAL = 15,    ///< Normal (apps, scanners)
  TASK_PRIORITY_LOW = 10,       ///< Low (UI, LEDs, keyboard)
  TASK_PRIORITY_IDLE = 5        ///< Minimum (background tasks)
} task_priority_t;

/**
 * @brief Standardized stack sizes
 *
 * Recommendations based on typical usage:
 * - TINY: Simple tasks without allocations
 * - SMALL: Basic UI, LEDs
 * - MEDIUM: Most applications
 * - LARGE: Complex protocols (WiFi, BLE, GPS)
 * - HUGE: Zigbee, Thread, OpenThread stack
 */
typedef enum {
  TASK_STACK_TINY = 1024,    ///< 1 KB - Very simple tasks
  TASK_STACK_SMALL = 2048,   ///< 2 KB - UI, LEDs, animations
  TASK_STACK_MEDIUM = 4096,  ///< 4 KB - Standard apps
  TASK_STACK_LARGE = 8192,   ///< 8 KB - Complex protocols
  TASK_STACK_HUGE = 16384    ///< 16 KB - Zigbee, Thread
} task_stack_size_t;

/**
 * @brief Metadata for a registered task
 */
typedef struct {
  const char* name;              ///< Task name
  TaskHandle_t handle;           ///< FreeRTOS handle
  task_priority_t priority;      ///< Assigned priority
  task_stack_size_t stack_size;  ///< Stack size
  bool is_running;               ///< Current state
  uint32_t created_at_ms;        ///< Creation timestamp
  UBaseType_t stack_watermark;   ///< Minimum free stack (watermark)
} task_info_t;

/**
 * @brief Initialize the Task Manager
 *
 * Must be called in app_main() before creating any task
 *
 * @return ESP_OK on success
 */
esp_err_t task_manager_init(void);

/**
 * @brief Deinitialize the Task Manager and free resources
 */
void task_manager_deinit(void);

/**
 * @brief Create a new task and register it in the manager
 *
 * Wrapper around xTaskCreate() that standardizes priorities and stack sizes
 *
 * @param task_func Task function
 * @param name Descriptive name (use pattern: module_function)
 * @param stack_size Stack size (use TASK_STACK_* enums)
 * @param params Parameters for the task
 * @param priority Priority (use TASK_PRIORITY_* enums)
 * @param handle Pointer to store the handle (can be NULL)
 * @return ESP_OK on success
 */
esp_err_t task_manager_create(TaskFunction_t task_func,
                              const char* name,
                              task_stack_size_t stack_size,
                              void* params,
                              task_priority_t priority,
                              TaskHandle_t* handle);

/**
 * @brief Delete a task and update the registry
 *
 * @param handle Handle of the task to delete
 * @return ESP_OK on success
 */
esp_err_t task_manager_delete(TaskHandle_t handle);

/**
 * @brief List all registered tasks in the system
 *
 * Prints detailed information for each task to the log
 */
void task_manager_list_all(void);

/**
 * @brief Get the total number of registered tasks
 *
 * @return Number of tasks
 */
uint32_t task_manager_get_count(void);

/**
 * @brief Get information for a specific task
 *
 * @param handle Task handle
 * @return Pointer to task_info_t or NULL if not found
 */
task_info_t* task_manager_get_info(TaskHandle_t handle);
bool task_manager_get_info_copy(TaskHandle_t handle, task_info_t* out_info);

/**
 * @brief Update stack watermarks for all tasks
 *
 * Useful for detecting tasks running low on stack
 */
void task_manager_update_watermarks(void);

/**
 * @brief Print stack usage statistics for all tasks
 *
 * Helps identify tasks with oversized or undersized stacks
 */
void task_manager_print_stack_usage(void);

/**
 * @brief Check if any task is near stack overflow
 *
 * @return true if any task has < 512 bytes of free stack
 */
bool task_manager_check_stack_overflow_risk(void);

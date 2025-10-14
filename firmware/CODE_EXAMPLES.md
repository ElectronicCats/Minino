# 💻 Ejemplos de Código - Mejoras Arquitecturales

Este documento contiene ejemplos de código concretos para implementar las mejoras arquitecturales propuestas en el plan de migración.

---

## 📑 Tabla de Contenidos

1. [Task Manager Implementation](#task-manager-implementation)
2. [Memory Manager Implementation](#memory-manager-implementation)
3. [Error Handler Implementation](#error-handler-implementation)
4. [Queue & Event Manager](#queue--event-manager)
5. [Power Manager](#power-manager)
6. [Migration Examples](#migration-examples)

---

## 1. Task Manager Implementation

### Header File: `main/core/task_manager/task_manager.h`

```c
#ifndef TASK_MANAGER_H
#define TASK_MANAGER_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Standardized task priorities
 * 
 * These priorities are designed to work with ESP32-C6 and ensure proper
 * task scheduling for the multi-protocol Minino device.
 */
typedef enum {
    TASK_PRIORITY_CRITICAL = 24,    /**< Protocol critical (IEEE 802.15.4, Zigbee) */
    TASK_PRIORITY_HIGH = 20,        /**< High priority (GPS, WiFi events, BLE) */
    TASK_PRIORITY_NORMAL = 15,      /**< Normal applications */
    TASK_PRIORITY_LOW = 10,         /**< UI, logging, non-critical */
    TASK_PRIORITY_IDLE = 5          /**< Background tasks */
} task_priority_t;

/**
 * @brief Standardized stack sizes
 * 
 * Based on empirical testing and ESP-IDF recommendations.
 */
typedef enum {
    TASK_STACK_TINY = 1024,         /**< Minimal tasks (watchdog monitor) */
    TASK_STACK_SMALL = 2048,        /**< Simple tasks (LED control, UI) */
    TASK_STACK_MEDIUM = 4096,       /**< Standard tasks (protocol handlers) */
    TASK_STACK_LARGE = 8192,        /**< Complex tasks (rarely needed) */
    TASK_STACK_HUGE = 16384         /**< Very complex tasks (avoid if possible) */
} task_stack_size_t;

/**
 * @brief Task metadata
 * 
 * Stores information about each managed task for monitoring and debugging.
 */
typedef struct {
    const char* name;               /**< Task name */
    TaskHandle_t handle;            /**< FreeRTOS task handle */
    task_priority_t priority;       /**< Task priority */
    task_stack_size_t stack_size;   /**< Stack size in bytes */
    bool is_running;                /**< Running status */
    uint32_t created_at;            /**< Creation timestamp (ms since boot) */
    uint32_t last_runtime;          /**< Last known runtime (for monitoring) */
} task_info_t;

/**
 * @brief Initialize the task manager
 * 
 * Must be called before any task_manager_create() calls.
 * 
 * @return 
 *     - ESP_OK: Success
 *     - ESP_ERR_NO_MEM: Insufficient memory
 *     - ESP_ERR_INVALID_STATE: Already initialized
 */
esp_err_t task_manager_init(void);

/**
 * @brief Create a managed task
 * 
 * Creates a FreeRTOS task and registers it with the task manager for
 * monitoring and lifecycle management.
 * 
 * @param task_func Task function pointer
 * @param name Task name (must follow convention: <module>_<function>)
 * @param stack_size Stack size (use TASK_STACK_* enums)
 * @param params Task parameters (passed to task function)
 * @param priority Task priority (use TASK_PRIORITY_* enums)
 * @param handle Pointer to store task handle (can be NULL)
 * 
 * @return 
 *     - ESP_OK: Success
 *     - ESP_ERR_NO_MEM: Insufficient memory
 *     - ESP_ERR_INVALID_ARG: Invalid arguments
 *     - ESP_FAIL: Task creation failed
 */
esp_err_t task_manager_create(
    TaskFunction_t task_func,
    const char* name,
    task_stack_size_t stack_size,
    void* params,
    task_priority_t priority,
    TaskHandle_t* handle
);

/**
 * @brief Delete a managed task
 * 
 * @param handle Task handle to delete
 * 
 * @return 
 *     - ESP_OK: Success
 *     - ESP_ERR_INVALID_ARG: Invalid handle
 *     - ESP_ERR_NOT_FOUND: Task not found
 */
esp_err_t task_manager_delete(TaskHandle_t handle);

/**
 * @brief List all managed tasks (debug)
 * 
 * Prints task information to console.
 */
void task_manager_list_all(void);

/**
 * @brief Get total managed task count
 * 
 * @return Number of currently managed tasks
 */
uint32_t task_manager_get_count(void);

/**
 * @brief Get task information
 * 
 * @param handle Task handle
 * @return Pointer to task_info_t or NULL if not found
 */
task_info_t* task_manager_get_info(TaskHandle_t handle);

/**
 * @brief Get task by name
 * 
 * @param name Task name
 * @return Task handle or NULL if not found
 */
TaskHandle_t task_manager_get_by_name(const char* name);

#ifdef __cplusplus
}
#endif

#endif // TASK_MANAGER_H
```

### Implementation: `main/core/task_manager/task_manager.c`

```c
#include "task_manager.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>

#define TASK_MANAGER_MAX_TASKS 64
#define TAG "task_manager"

static task_info_t tasks[TASK_MANAGER_MAX_TASKS];
static uint32_t task_count = 0;
static bool initialized = false;
static portMUX_TYPE task_manager_spinlock = portMUX_INITIALIZER_UNLOCKED;

esp_err_t task_manager_init(void) {
    if (initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_ERR_INVALID_STATE;
    }

    memset(tasks, 0, sizeof(tasks));
    task_count = 0;
    initialized = true;

    ESP_LOGI(TAG, "Task Manager initialized (max tasks: %d)", TASK_MANAGER_MAX_TASKS);
    return ESP_OK;
}

esp_err_t task_manager_create(
    TaskFunction_t task_func,
    const char* name,
    task_stack_size_t stack_size,
    void* params,
    task_priority_t priority,
    TaskHandle_t* handle
) {
    if (!initialized) {
        ESP_LOGE(TAG, "Task manager not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (task_func == NULL || name == NULL) {
        ESP_LOGE(TAG, "Invalid arguments");
        return ESP_ERR_INVALID_ARG;
    }

    portENTER_CRITICAL(&task_manager_spinlock);
    
    if (task_count >= TASK_MANAGER_MAX_TASKS) {
        portEXIT_CRITICAL(&task_manager_spinlock);
        ESP_LOGE(TAG, "Maximum number of tasks reached");
        return ESP_ERR_NO_MEM;
    }

    // Find free slot
    int slot = -1;
    for (int i = 0; i < TASK_MANAGER_MAX_TASKS; i++) {
        if (!tasks[i].is_running && tasks[i].handle == NULL) {
            slot = i;
            break;
        }
    }

    if (slot == -1) {
        portEXIT_CRITICAL(&task_manager_spinlock);
        ESP_LOGE(TAG, "No free slot found (internal error)");
        return ESP_ERR_NO_MEM;
    }

    // Create FreeRTOS task
    TaskHandle_t task_handle = NULL;
    BaseType_t ret = xTaskCreate(
        task_func,
        name,
        (uint32_t)stack_size,
        params,
        (UBaseType_t)priority,
        &task_handle
    );

    if (ret != pdPASS || task_handle == NULL) {
        portEXIT_CRITICAL(&task_manager_spinlock);
        ESP_LOGE(TAG, "Failed to create task '%s'", name);
        return ESP_FAIL;
    }

    // Store task info
    tasks[slot].name = name;
    tasks[slot].handle = task_handle;
    tasks[slot].priority = priority;
    tasks[slot].stack_size = stack_size;
    tasks[slot].is_running = true;
    tasks[slot].created_at = (uint32_t)(esp_timer_get_time() / 1000);
    tasks[slot].last_runtime = 0;

    task_count++;

    if (handle != NULL) {
        *handle = task_handle;
    }

    portEXIT_CRITICAL(&task_manager_spinlock);

    ESP_LOGI(TAG, "Created task '%s' [priority:%d, stack:%d bytes, handle:%p]",
             name, priority, stack_size, (void*)task_handle);

    return ESP_OK;
}

esp_err_t task_manager_delete(TaskHandle_t handle) {
    if (!initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    portENTER_CRITICAL(&task_manager_spinlock);

    // Find task
    int slot = -1;
    for (int i = 0; i < TASK_MANAGER_MAX_TASKS; i++) {
        if (tasks[i].handle == handle && tasks[i].is_running) {
            slot = i;
            break;
        }
    }

    if (slot == -1) {
        portEXIT_CRITICAL(&task_manager_spinlock);
        ESP_LOGW(TAG, "Task not found");
        return ESP_ERR_NOT_FOUND;
    }

    // Mark as not running
    tasks[slot].is_running = false;
    tasks[slot].handle = NULL;
    task_count--;

    ESP_LOGI(TAG, "Deleted task '%s'", tasks[slot].name);

    portEXIT_CRITICAL(&task_manager_spinlock);

    // Delete FreeRTOS task
    vTaskDelete(handle);

    return ESP_OK;
}

void task_manager_list_all(void) {
    if (!initialized) {
        ESP_LOGW(TAG, "Task manager not initialized");
        return;
    }

    ESP_LOGI(TAG, "=== Managed Tasks (%lu) ===", task_count);
    
    for (int i = 0; i < TASK_MANAGER_MAX_TASKS; i++) {
        if (tasks[i].is_running && tasks[i].handle != NULL) {
            ESP_LOGI(TAG, "  [%d] %s - Priority:%d Stack:%d Handle:%p Age:%lums",
                     i,
                     tasks[i].name,
                     tasks[i].priority,
                     tasks[i].stack_size,
                     (void*)tasks[i].handle,
                     (uint32_t)(esp_timer_get_time() / 1000) - tasks[i].created_at);
        }
    }
}

uint32_t task_manager_get_count(void) {
    return task_count;
}

task_info_t* task_manager_get_info(TaskHandle_t handle) {
    if (!initialized || handle == NULL) {
        return NULL;
    }

    for (int i = 0; i < TASK_MANAGER_MAX_TASKS; i++) {
        if (tasks[i].handle == handle && tasks[i].is_running) {
            return &tasks[i];
        }
    }

    return NULL;
}

TaskHandle_t task_manager_get_by_name(const char* name) {
    if (!initialized || name == NULL) {
        return NULL;
    }

    for (int i = 0; i < TASK_MANAGER_MAX_TASKS; i++) {
        if (tasks[i].is_running && strcmp(tasks[i].name, name) == 0) {
            return tasks[i].handle;
        }
    }

    return NULL;
}
```

### CMakeLists.txt

```cmake
idf_component_register(
    SRCS "task_manager.c"
    INCLUDE_DIRS "."
    REQUIRES esp_timer
)
```

---

## 2. Memory Manager Implementation

### Header: `main/core/memory_manager/mem_pool.h`

```c
#ifndef MEM_POOL_H
#define MEM_POOL_H

#include "esp_err.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Memory pool identifiers
 * 
 * Add new pools as needed for frequently allocated objects.
 */
typedef enum {
    POOL_WIFI_SCAN_RESULT,      /**< wifi_ap_record_t objects */
    POOL_GPS_COORDINATE,        /**< GPS coordinate objects */
    POOL_BLE_ADV_DATA,          /**< BLE advertisement data */
    POOL_ZIGBEE_PACKET,         /**< Zigbee packet buffers */
    POOL_MAX                    /**< Must be last */
} pool_id_t;

/**
 * @brief Initialize all memory pools
 * 
 * @return 
 *     - ESP_OK: Success
 *     - ESP_ERR_NO_MEM: Insufficient memory
 */
esp_err_t mem_pool_init(void);

/**
 * @brief Allocate object from pool
 * 
 * @param pool Pool ID
 * @return Pointer to allocated object or NULL if pool is empty
 */
void* mem_pool_alloc(pool_id_t pool);

/**
 * @brief Free object back to pool
 * 
 * @param pool Pool ID
 * @param ptr Pointer to object to free
 */
void mem_pool_free(pool_id_t pool, void* ptr);

/**
 * @brief Get free object count in pool
 * 
 * @param pool Pool ID
 * @return Number of free objects
 */
size_t mem_pool_get_free_count(pool_id_t pool);

/**
 * @brief Get pool statistics
 * 
 * @param pool Pool ID
 * @param total_count Output: total objects in pool
 * @param free_count Output: free objects
 * @param alloc_count Output: total allocations since init
 */
void mem_pool_get_stats(pool_id_t pool, size_t* total_count, 
                        size_t* free_count, size_t* alloc_count);

#ifdef __cplusplus
}
#endif

#endif // MEM_POOL_H
```

### Implementation: `main/core/memory_manager/mem_pool.c`

```c
#include "mem_pool.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include <string.h>

#define TAG "mem_pool"

// Pool configuration
typedef struct {
    size_t object_size;
    size_t pool_size;
    const char* name;
} pool_config_t;

static const pool_config_t pool_configs[POOL_MAX] = {
    [POOL_WIFI_SCAN_RESULT] = {
        .object_size = sizeof(wifi_ap_record_t),
        .pool_size = 50,
        .name = "WiFi_Scan"
    },
    [POOL_GPS_COORDINATE] = {
        .object_size = 32,  // sizeof(gps_coordinate_t) - adjust as needed
        .pool_size = 100,
        .name = "GPS_Coord"
    },
    [POOL_BLE_ADV_DATA] = {
        .object_size = 256,  // BLE adv data max size
        .pool_size = 30,
        .name = "BLE_Adv"
    },
    [POOL_ZIGBEE_PACKET] = {
        .object_size = 128,  // Typical Zigbee packet
        .pool_size = 20,
        .name = "Zigbee_Pkt"
    }
};

// Pool runtime data
typedef struct {
    void* pool_memory;          // Allocated pool memory
    uint8_t* free_bitmap;       // Bitmap of free objects
    size_t free_count;          // Current free count
    size_t total_allocs;        // Total allocations
    bool initialized;
} pool_data_t;

static pool_data_t pools[POOL_MAX];

esp_err_t mem_pool_init(void) {
    ESP_LOGI(TAG, "Initializing memory pools...");

    for (int i = 0; i < POOL_MAX; i++) {
        const pool_config_t* config = &pool_configs[i];
        pool_data_t* data = &pools[i];

        // Allocate pool memory
        size_t total_size = config->object_size * config->pool_size;
        data->pool_memory = malloc(total_size);
        if (data->pool_memory == NULL) {
            ESP_LOGE(TAG, "Failed to allocate pool %s", config->name);
            return ESP_ERR_NO_MEM;
        }

        // Allocate bitmap (1 bit per object)
        size_t bitmap_size = (config->pool_size + 7) / 8;
        data->free_bitmap = malloc(bitmap_size);
        if (data->free_bitmap == NULL) {
            free(data->pool_memory);
            ESP_LOGE(TAG, "Failed to allocate bitmap for pool %s", config->name);
            return ESP_ERR_NO_MEM;
        }

        // Initialize all as free (set all bits to 1)
        memset(data->free_bitmap, 0xFF, bitmap_size);
        data->free_count = config->pool_size;
        data->total_allocs = 0;
        data->initialized = true;

        ESP_LOGI(TAG, "Pool %s: %zu objects x %zu bytes = %zu bytes total",
                 config->name, config->pool_size, config->object_size, total_size);
    }

    ESP_LOGI(TAG, "Memory pools initialized");
    return ESP_OK;
}

void* mem_pool_alloc(pool_id_t pool) {
    if (pool >= POOL_MAX || !pools[pool].initialized) {
        return NULL;
    }

    pool_data_t* data = &pools[pool];
    const pool_config_t* config = &pool_configs[pool];

    if (data->free_count == 0) {
        ESP_LOGW(TAG, "Pool %s exhausted", config->name);
        return NULL;
    }

    // Find first free object
    for (size_t i = 0; i < config->pool_size; i++) {
        size_t byte_idx = i / 8;
        size_t bit_idx = i % 8;
        
        if (data->free_bitmap[byte_idx] & (1 << bit_idx)) {
            // Found free object
            data->free_bitmap[byte_idx] &= ~(1 << bit_idx);  // Mark as used
            data->free_count--;
            data->total_allocs++;
            
            void* ptr = (uint8_t*)data->pool_memory + (i * config->object_size);
            return ptr;
        }
    }

    // Should never reach here
    ESP_LOGE(TAG, "Pool %s: free_count=%zu but no free object found (bug!)",
             config->name, data->free_count);
    return NULL;
}

void mem_pool_free(pool_id_t pool, void* ptr) {
    if (pool >= POOL_MAX || !pools[pool].initialized || ptr == NULL) {
        return;
    }

    pool_data_t* data = &pools[pool];
    const pool_config_t* config = &pool_configs[pool];

    // Calculate object index
    ptrdiff_t offset = (uint8_t*)ptr - (uint8_t*)data->pool_memory;
    if (offset < 0 || (size_t)offset >= (config->object_size * config->pool_size)) {
        ESP_LOGE(TAG, "Invalid pointer for pool %s", config->name);
        return;
    }

    size_t index = offset / config->object_size;
    size_t byte_idx = index / 8;
    size_t bit_idx = index % 8;

    // Check if already free
    if (data->free_bitmap[byte_idx] & (1 << bit_idx)) {
        ESP_LOGW(TAG, "Double free detected in pool %s", config->name);
        return;
    }

    // Mark as free
    data->free_bitmap[byte_idx] |= (1 << bit_idx);
    data->free_count++;
}

size_t mem_pool_get_free_count(pool_id_t pool) {
    if (pool >= POOL_MAX || !pools[pool].initialized) {
        return 0;
    }
    return pools[pool].free_count;
}

void mem_pool_get_stats(pool_id_t pool, size_t* total_count, 
                        size_t* free_count, size_t* alloc_count) {
    if (pool >= POOL_MAX || !pools[pool].initialized) {
        if (total_count) *total_count = 0;
        if (free_count) *free_count = 0;
        if (alloc_count) *alloc_count = 0;
        return;
    }

    if (total_count) *total_count = pool_configs[pool].pool_size;
    if (free_count) *free_count = pools[pool].free_count;
    if (alloc_count) *alloc_count = pools[pool].total_allocs;
}
```

---

## 3. Error Handler Implementation

### Header: `main/core/error_handler/error_handler.h`

```c
#ifndef ERROR_HANDLER_H
#define ERROR_HANDLER_H

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Error severity levels
 */
typedef enum {
    ERROR_SEVERITY_INFO,        /**< Informational */
    ERROR_SEVERITY_WARNING,     /**< Warning - degraded functionality */
    ERROR_SEVERITY_ERROR,       /**< Error - feature not working */
    ERROR_SEVERITY_CRITICAL     /**< Critical - system instability */
} error_severity_t;

/**
 * @brief Error information structure
 */
typedef struct {
    error_severity_t severity;      /**< Error severity */
    esp_err_t error_code;           /**< ESP-IDF error code */
    const char* component;          /**< Component name */
    const char* message;            /**< Error message */
    bool requires_restart;          /**< Should system restart? */
    void (*recovery_func)(void);    /**< Optional recovery function */
} error_info_t;

/**
 * @brief Error callback function type
 */
typedef void (*error_callback_t)(const error_info_t* error);

/**
 * @brief Initialize error handler
 * 
 * @return ESP_OK on success
 */
esp_err_t error_handler_init(void);

/**
 * @brief Report an error
 * 
 * @param error Error information
 */
void error_handler_report(const error_info_t* error);

/**
 * @brief Set restart callback
 * 
 * @param callback Function to call before restart
 */
void error_handler_set_restart_callback(void (*callback)(void));

/**
 * @brief Set custom error callback
 * 
 * @param callback Function to call on errors
 */
void error_handler_set_callback(error_callback_t callback);

/**
 * @brief Get error statistics
 * 
 * @param total Total errors
 * @param critical Critical errors
 * @param warnings Warnings
 */
void error_handler_get_stats(uint32_t* total, uint32_t* critical, uint32_t* warnings);

#ifdef __cplusplus
}
#endif

#endif // ERROR_HANDLER_H
```

### Implementation: `main/core/error_handler/error_handler.c`

```c
#include "error_handler.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"

#define TAG "error_handler"
#define RESTART_DELAY_MS 5000

static bool initialized = false;
static void (*restart_callback)(void) = NULL;
static error_callback_t custom_callback = NULL;

// Statistics
static uint32_t total_errors = 0;
static uint32_t critical_errors = 0;
static uint32_t warnings = 0;

static const char* severity_to_string(error_severity_t severity) {
    switch (severity) {
        case ERROR_SEVERITY_INFO: return "INFO";
        case ERROR_SEVERITY_WARNING: return "WARNING";
        case ERROR_SEVERITY_ERROR: return "ERROR";
        case ERROR_SEVERITY_CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

esp_err_t error_handler_init(void) {
    if (initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    total_errors = 0;
    critical_errors = 0;
    warnings = 0;
    restart_callback = NULL;
    custom_callback = NULL;
    initialized = true;

    ESP_LOGI(TAG, "Error handler initialized");
    return ESP_OK;
}

void error_handler_report(const error_info_t* error) {
    if (!initialized || error == NULL) {
        return;
    }

    // Update statistics
    total_errors++;
    if (error->severity == ERROR_SEVERITY_CRITICAL) {
        critical_errors++;
    } else if (error->severity == ERROR_SEVERITY_WARNING) {
        warnings++;
    }

    // Log error
    const char* severity_str = severity_to_string(error->severity);
    
    switch (error->severity) {
        case ERROR_SEVERITY_INFO:
            ESP_LOGI(TAG, "[%s] %s: %s (code: 0x%x)",
                     severity_str, error->component, error->message, error->error_code);
            break;
        case ERROR_SEVERITY_WARNING:
            ESP_LOGW(TAG, "[%s] %s: %s (code: 0x%x)",
                     severity_str, error->component, error->message, error->error_code);
            break;
        case ERROR_SEVERITY_ERROR:
            ESP_LOGE(TAG, "[%s] %s: %s (code: 0x%x)",
                     severity_str, error->component, error->message, error->error_code);
            break;
        case ERROR_SEVERITY_CRITICAL:
            ESP_LOGE(TAG, "[%s] %s: %s (code: 0x%x) - CRITICAL!",
                     severity_str, error->component, error->message, error->error_code);
            break;
    }

    // Call custom callback if set
    if (custom_callback != NULL) {
        custom_callback(error);
    }

    // Attempt recovery
    if (error->recovery_func != NULL) {
        ESP_LOGI(TAG, "Attempting recovery for %s...", error->component);
        error->recovery_func();
    }

    // Restart if required
    if (error->requires_restart) {
        ESP_LOGE(TAG, "System restart required. Restarting in %d ms...", RESTART_DELAY_MS);
        
        // Call restart callback if set
        if (restart_callback != NULL) {
            restart_callback();
        }

        // Delay to allow logs to flush
        vTaskDelay(pdMS_TO_TICKS(RESTART_DELAY_MS));
        
        // Restart
        esp_restart();
    }
}

void error_handler_set_restart_callback(void (*callback)(void)) {
    restart_callback = callback;
}

void error_handler_set_callback(error_callback_t callback) {
    custom_callback = callback;
}

void error_handler_get_stats(uint32_t* total, uint32_t* critical, uint32_t* warnings_out) {
    if (total) *total = total_errors;
    if (critical) *critical = critical_errors;
    if (warnings_out) *warnings_out = warnings;
}
```

---

## 4. Queue & Event Manager

### Header: `main/core/sync_manager/queue_manager.h`

```c
#ifndef QUEUE_MANAGER_H
#define QUEUE_MANAGER_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief System queue identifiers
 */
typedef enum {
    QUEUE_GPS_DATA,         /**< GPS data updates */
    QUEUE_WIFI_EVENTS,      /**< WiFi events */
    QUEUE_BT_EVENTS,        /**< Bluetooth events */
    QUEUE_ZIGBEE_EVENTS,    /**< Zigbee events */
    QUEUE_UI_EVENTS,        /**< UI/input events */
    QUEUE_LOG_EVENTS,       /**< Logging events */
    QUEUE_MAX               /**< Must be last */
} queue_id_t;

/**
 * @brief Initialize queue manager
 * 
 * @return ESP_OK on success
 */
esp_err_t queue_manager_init(void);

/**
 * @brief Get queue handle
 * 
 * @param id Queue ID
 * @return Queue handle or NULL if invalid
 */
QueueHandle_t queue_manager_get(queue_id_t id);

/**
 * @brief Send data to queue
 * 
 * @param id Queue ID
 * @param data Pointer to data to send
 * @param timeout Timeout in ticks
 * @return ESP_OK on success
 */
esp_err_t queue_manager_send(queue_id_t id, void* data, TickType_t timeout);

/**
 * @brief Receive data from queue
 * 
 * @param id Queue ID
 * @param buffer Buffer to receive data
 * @param timeout Timeout in ticks
 * @return ESP_OK on success
 */
esp_err_t queue_manager_receive(queue_id_t id, void* buffer, TickType_t timeout);

#ifdef __cplusplus
}
#endif

#endif // QUEUE_MANAGER_H
```

---

## 5. Migration Examples

### Example 1: Migrating Zigbee Switch Tasks

**BEFORE** (zigbee_switch.c):
```c
void zigbee_switch_init() {
    // ... setup code ...
    
    xTaskCreate(esp_zb_task, "Zigbee_main", 4096, NULL, 5, NULL);
    xTaskCreate(network_failed_task, "Network_failed", 4096, NULL, 5,
                &network_failed_task_handle);
    xTaskCreate(wait_for_devices_task, "Wait_for_devices", 4096, NULL, 5,
                &wait_for_devices_task_handle);
    xTaskCreate(switch_state_machine_task, "Switch_state_machine", 4096, NULL, 5,
                &switch_state_machine_task_handle);
    xTaskCreate(network_open_task, "Network_open", 4096, NULL, 5,
                &network_open_task_handle);
}
```

**AFTER** (zigbee_switch.c):
```c
#include "task_manager.h"

void zigbee_switch_init() {
    // ... setup code ...
    
    // Create tasks with task manager
    task_manager_create(esp_zb_task, "zigbee_main", 
                       TASK_STACK_MEDIUM, NULL, 
                       TASK_PRIORITY_CRITICAL, NULL);
                       
    task_manager_create(network_failed_task, "zigbee_net_failed", 
                       TASK_STACK_MEDIUM, NULL, 
                       TASK_PRIORITY_NORMAL, &network_failed_task_handle);
                       
    task_manager_create(wait_for_devices_task, "zigbee_wait_dev", 
                       TASK_STACK_MEDIUM, NULL, 
                       TASK_PRIORITY_NORMAL, &wait_for_devices_task_handle);
                       
    task_manager_create(switch_state_machine_task, "zigbee_state_machine", 
                       TASK_STACK_MEDIUM, NULL, 
                       TASK_PRIORITY_NORMAL, &switch_state_machine_task_handle);
                       
    task_manager_create(network_open_task, "zigbee_net_open", 
                       TASK_STACK_MEDIUM, NULL, 
                       TASK_PRIORITY_NORMAL, &network_open_task_handle);
}
```

### Example 2: Using Memory Pools

**BEFORE** (wardriving_module.c):
```c
// Dynamic allocation every scan
wifi_ap_record_t* ap = malloc(sizeof(wifi_ap_record_t));
if (ap == NULL) {
    ESP_LOGE(TAG, "No memory");
    return;
}
// ... use ap ...
free(ap);
```

**AFTER** (wardriving_module.c):
```c
#include "mem_pool.h"

// Use memory pool
wifi_ap_record_t* ap = mem_pool_alloc(POOL_WIFI_SCAN_RESULT);
if (ap == NULL) {
    ESP_LOGE(TAG, "Pool exhausted");
    return;
}
// ... use ap ...
mem_pool_free(POOL_WIFI_SCAN_RESULT, ap);
```

### Example 3: Using Error Handler

**BEFORE** (wifi_controller.c):
```c
esp_err_t err = esp_netif_init();
if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error initializing netif: %s", esp_err_to_name(err));
    esp_restart();  // Abrupt restart
}
```

**AFTER** (wifi_controller.c):
```c
#include "error_handler.h"

static void wifi_cleanup(void) {
    // Cleanup WiFi resources
    esp_wifi_stop();
    esp_wifi_deinit();
}

esp_err_t err = esp_netif_init();
if (err != ESP_OK) {
    error_info_t error = {
        .severity = ERROR_SEVERITY_CRITICAL,
        .error_code = err,
        .component = "wifi_controller",
        .message = "Failed to initialize netif",
        .requires_restart = true,
        .recovery_func = wifi_cleanup
    };
    error_handler_report(&error);
    // error_handler will handle restart with cleanup
}
```

---

## 6. Integration in main.c

```c
#include <stdio.h>

// Core managers
#include "task_manager.h"
#include "mem_pool.h"
#include "error_handler.h"
#include "queue_manager.h"

// Existing includes
#include "buzzer.h"
#include "cat_console.h"
#include "esp_log.h"
// ... rest of includes ...

static const char* TAG = "main";

void app_main() {
    // Initialize core managers FIRST
    ESP_ERROR_CHECK(error_handler_init());
    ESP_ERROR_CHECK(task_manager_init());
    ESP_ERROR_CHECK(mem_pool_init());
    ESP_ERROR_CHECK(queue_manager_init());

    ESP_LOGI(TAG, "Core managers initialized");

    // Existing initialization
    preferences_begin();
    gps_hw_init();
    sleep_mode_set_mode(resistor_detector(CONFIG_GPIO_RIGHT_BUTTON));

    #if !defined(CONFIG_MAIN_DEBUG)
    esp_log_level_set(TAG, ESP_LOG_NONE);
    #endif

    uart_config_t uart_config = {
        .baud_rate = 921600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };

    uart_bridge_begin(uart_config, 1024);
    logs_output_set_output(preferences_get_uchar("logs_output", USB));

    bool stealth_mode = preferences_get_bool("stealth_mode", false);
    if (!stealth_mode) {
        buzzer_enable();
        leds_begin();
    }
    
    buzzer_begin(CONFIG_BUZZER_PIN);
    sd_card_begin();
    flash_fs_begin(flash_fs_screens_handler);
    keyboard_module_begin();
    menus_module_begin();
    leds_off();
    preferences_put_bool("wifi_connected", false);
    flash_storage_begin();

    // Log initial stats
    ESP_LOGI(TAG, "Tasks: %lu", task_manager_get_count());
    
    uint32_t total_errors, critical_errors, warnings;
    error_handler_get_stats(&total_errors, &critical_errors, &warnings);
    ESP_LOGI(TAG, "Errors: Total=%lu Critical=%lu Warnings=%lu", 
             total_errors, critical_errors, warnings);

    cat_console_begin();  // Contains while(true) loop
}
```

---

**Última Actualización**: Octubre 2025  
**Versión**: 1.0


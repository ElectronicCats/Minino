# 💡 Ejemplos de Código - Core Components

Este documento proporciona ejemplos prácticos de cómo usar los nuevos componentes core del firmware Minino.

---

## 📦 Memory Monitor

### Uso Básico

```c
#include "memory_monitor.h"

void app_main() {
    // Inicializar el monitor
    heap_monitor_init();
    
    // Iniciar monitoreo cada 5 segundos
    heap_monitor_start(5000);
    
    // El monitor ahora corre en background
    // y loggea automáticamente alertas si la memoria es baja
}
```

### Obtener Estadísticas Manualmente

```c
#include "memory_monitor.h"

void check_memory_usage() {
    heap_stats_t stats = heap_monitor_get_stats();
    
    ESP_LOGI(TAG, "Memoria libre: %zu KB", stats.total_free / 1024);
    ESP_LOGI(TAG, "Bloque más grande: %zu KB", stats.largest_free_block / 1024);
    ESP_LOGI(TAG, "Fragmentación: %.1f%%", stats.fragmentation_percent);
    ESP_LOGI(TAG, "Mínimo histórico: %zu KB", stats.min_free_ever / 1024);
}
```

### Configurar Callback para Alertas

```c
#include "memory_monitor.h"

void my_memory_alert_handler(heap_alert_level_t level, const heap_stats_t* stats) {
    if (level == HEAP_ALERT_CRITICAL) {
        // Liberar caches, cerrar features no esenciales, etc.
        ESP_LOGW(TAG, "¡Memoria crítica! Liberando recursos...");
        
        // Ejemplo: limpiar buffers
        clear_image_cache();
        stop_non_essential_tasks();
    }
}

void app_main() {
    heap_monitor_init();
    heap_monitor_set_alert_callback(my_memory_alert_handler);
    heap_monitor_start(5000);
}
```

### Configurar Umbrales Personalizados

```c
#include "memory_monitor.h"

void setup_memory_monitor() {
    heap_monitor_init();
    
    // Umbrales personalizados: Warning=60KB, Critical=40KB, Emergency=25KB
    heap_monitor_set_thresholds(60, 40, 25);
    
    heap_monitor_start(3000);  // Monitorear cada 3 segundos
}
```

### Imprimir Reporte Completo

```c
#include "memory_monitor.h"

// Comando de consola
int cmd_memory_report(int argc, char **argv) {
    heap_monitor_print_stats();
    return 0;
}
```

---

## 🎯 Task Manager

### Crear una Tarea Simple

```c
#include "task_manager.h"

void my_task(void *pvParameters) {
    while (1) {
        ESP_LOGI(TAG, "Task running");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void start_my_module() {
    TaskHandle_t task_handle = NULL;
    
    esp_err_t err = task_manager_create(
        my_task,                // Función de la tarea
        "my_module_task",       // Nombre descriptivo
        TASK_STACK_SMALL,       // 2KB stack
        NULL,                   // Sin parámetros
        TASK_PRIORITY_LOW,      // Baja prioridad (UI, background)
        &task_handle            // Guardar handle
    );
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error creando tarea");
    }
}
```

### Crear Tarea con Prioridad Alta (GPS, WiFi)

```c
#include "task_manager.h"

void gps_processing_task(void *pvParameters) {
    while (1) {
        process_gps_data();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void start_gps_module() {
    task_manager_create(
        gps_processing_task,
        "gps_processor",
        TASK_STACK_LARGE,       // 8KB para procesamiento complejo
        NULL,
        TASK_PRIORITY_HIGH,     // Alta prioridad - time-sensitive
        &gps_task_handle
    );
}
```

### Crear Tarea Crítica (Protocolos)

```c
#include "task_manager.h"

void zigbee_protocol_task(void *pvParameters) {
    // Procesamiento de protocolo crítico
    while (1) {
        handle_zigbee_events();
    }
}

void start_zigbee_stack() {
    task_manager_create(
        zigbee_protocol_task,
        "zigbee_stack",
        TASK_STACK_HUGE,        // 16KB para stacks de protocolo
        NULL,
        TASK_PRIORITY_CRITICAL, // Máxima prioridad
        &zigbee_task_handle
    );
}
```

### Eliminar una Tarea

```c
#include "task_manager.h"

void stop_my_module() {
    if (my_task_handle != NULL) {
        task_manager_delete(my_task_handle);
        my_task_handle = NULL;
    }
}
```

### Listar Todas las Tareas

```c
#include "task_manager.h"

// Comando de consola
int cmd_task_list(int argc, char **argv) {
    task_manager_list_all();
    return 0;
}
```

### Verificar Stack Usage

```c
#include "task_manager.h"

// Comando de consola
int cmd_stack_usage(int argc, char **argv) {
    task_manager_print_stack_usage();
    
    if (task_manager_check_stack_overflow_risk()) {
        ESP_LOGW(TAG, "⚠️  Hay tareas en riesgo de stack overflow!");
    }
    
    return 0;
}
```

### Patrón Completo: Módulo con Múltiples Tareas

```c
#include "task_manager.h"

static TaskHandle_t scan_task_handle = NULL;
static TaskHandle_t display_task_handle = NULL;

void wifi_scan_task(void *pvParameters) {
    while (1) {
        perform_wifi_scan();
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

void wifi_display_task(void *pvParameters) {
    while (1) {
        update_display();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void wifi_module_start() {
    // Tarea de scanning - prioridad normal
    task_manager_create(
        wifi_scan_task,
        "wifi_scanner",
        TASK_STACK_MEDIUM,
        NULL,
        TASK_PRIORITY_NORMAL,
        &scan_task_handle
    );
    
    // Tarea de display - baja prioridad
    task_manager_create(
        wifi_display_task,
        "wifi_display",
        TASK_STACK_SMALL,
        NULL,
        TASK_PRIORITY_LOW,
        &display_task_handle
    );
}

void wifi_module_stop() {
    if (scan_task_handle != NULL) {
        task_manager_delete(scan_task_handle);
        scan_task_handle = NULL;
    }
    
    if (display_task_handle != NULL) {
        task_manager_delete(display_task_handle);
        display_task_handle = NULL;
    }
}
```

---

## 🚨 Error Handler

### Reportar Error Simple

```c
#include "error_handler.h"

void connect_to_wifi() {
    esp_err_t err = esp_wifi_connect();
    
    if (err != ESP_OK) {
        error_info_t error = {
            .severity = ERROR_SEVERITY_WARNING,
            .component = ERROR_COMPONENT_WIFI,
            .error_code = err,
            .message = "WiFi connection failed",
            .requires_restart = false,
            .recovery_func = NULL,
            .file = __FILE__,
            .line = __LINE__
        };
        
        error_handler_report(&error);
    }
}
```

### Usar Macro Helper

```c
#include "error_handler.h"

void connect_to_wifi() {
    esp_err_t err = esp_wifi_connect();
    
    if (err != ESP_OK) {
        ERROR_REPORT(
            ERROR_SEVERITY_WARNING,
            ERROR_COMPONENT_WIFI,
            err,
            "WiFi connection failed",
            false,  // No requiere restart
            NULL    // Sin recovery function
        );
    }
}
```

### Usar Macros Shortcuts

```c
#include "error_handler.h"

void connect_to_wifi() {
    esp_err_t err = esp_wifi_connect();
    
    if (err != ESP_OK) {
        ERROR_WIFI(err, "WiFi connection failed");
    }
}

void critical_sd_card_failure() {
    ERROR_CRITICAL(
        ERROR_COMPONENT_SD_CARD,
        ESP_FAIL,
        "SD card mount failed - data loss imminent"
    );
    // Sistema se reiniciará automáticamente
}
```

### Error con Recuperación Automática

```c
#include "error_handler.h"

void wifi_reconnect_recovery() {
    ESP_LOGI(TAG, "Intentando reconectar WiFi...");
    esp_wifi_connect();
}

void handle_wifi_disconnect() {
    error_info_t error = {
        .severity = ERROR_SEVERITY_ERROR,
        .component = ERROR_COMPONENT_WIFI,
        .error_code = ESP_ERR_WIFI_NOT_CONNECT,
        .message = "WiFi disconnected",
        .requires_restart = false,
        .recovery_func = wifi_reconnect_recovery,  // ← Recovery automática
        .file = __FILE__,
        .line = __LINE__
    };
    
    error_handler_report(&error);
    // El error handler llamará automáticamente a wifi_reconnect_recovery()
}
```

### Configurar Callback Pre-Restart

```c
#include "error_handler.h"
#include "preferences.h"

void save_data_before_restart() {
    ESP_LOGW(TAG, "Guardando datos antes de reiniciar...");
    
    // Guardar configuración crítica
    preferences_commit();
    
    // Flush logs a SD si está disponible
    flush_logs_to_sd();
    
    // Dar tiempo para que termine
    vTaskDelay(pdMS_TO_TICKS(500));
    
    ESP_LOGW(TAG, "Datos guardados, reiniciando...");
}

void app_main() {
    error_handler_init();
    error_handler_set_restart_callback(save_data_before_restart);
}
```

### Configurar Callback de Errores

```c
#include "error_handler.h"

void my_error_callback(const error_info_t* error) {
    // Ejemplo: enviar error a servidor remoto
    if (wifi_is_connected()) {
        send_error_to_telemetry(error);
    }
    
    // Ejemplo: guardar en flash para debugging
    if (error->severity >= ERROR_SEVERITY_ERROR) {
        save_error_to_flash(error);
    }
    
    // Ejemplo: mostrar en pantalla
    if (error->severity == ERROR_SEVERITY_CRITICAL) {
        show_error_on_display(error->message);
    }
}

void app_main() {
    error_handler_init();
    error_handler_set_callback(my_error_callback);
}
```

### Ver Estadísticas de Errores

```c
#include "error_handler.h"

// Comando de consola
int cmd_error_stats(int argc, char **argv) {
    error_handler_print_stats();
    
    // O acceder programáticamente
    error_stats_t stats = error_handler_get_stats();
    
    ESP_LOGI(TAG, "Total de errores: %lu", stats.total_errors);
    ESP_LOGI(TAG, "Errores críticos: %lu", stats.critical_count);
    ESP_LOGI(TAG, "Reinicios: %lu", stats.restarts_triggered);
    
    return 0;
}
```

### Deshabilitar Auto-Restart (Para Debugging)

```c
#include "error_handler.h"

void app_main() {
    error_handler_init();
    
    #ifdef CONFIG_DEBUG_MODE
    // En modo debug, no reiniciar automáticamente
    error_handler_set_auto_restart(false);
    #endif
}
```

---

## 🔄 Patrón Completo: Módulo Robusto

Ejemplo de cómo integrar los 3 componentes en un módulo real:

```c
#include "task_manager.h"
#include "memory_monitor.h"
#include "error_handler.h"

static const char* TAG = "my_module";
static TaskHandle_t processing_task_handle = NULL;

// Recovery function
void my_module_recovery() {
    ESP_LOGW(TAG, "Recuperando módulo...");
    my_module_stop();
    vTaskDelay(pdMS_TO_TICKS(1000));
    my_module_start();
}

// Task function
void my_module_processing_task(void *pvParameters) {
    while (1) {
        // Verificar memoria antes de operación pesada
        heap_stats_t stats = heap_monitor_get_stats();
        
        if (stats.total_free < 30 * 1024) {  // < 30KB
            ESP_LOGW(TAG, "Memoria baja, saltando procesamiento");
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }
        
        // Operación normal
        esp_err_t err = do_heavy_processing();
        
        if (err != ESP_OK) {
            ERROR_REPORT(
                ERROR_SEVERITY_ERROR,
                ERROR_COMPONENT_SYSTEM,
                err,
                "Processing failed",
                false,
                my_module_recovery  // ← Auto-recovery
            );
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

esp_err_t my_module_start() {
    ESP_LOGI(TAG, "Iniciando módulo");
    
    // Crear tarea con Task Manager
    esp_err_t err = task_manager_create(
        my_module_processing_task,
        "my_module_proc",
        TASK_STACK_MEDIUM,
        NULL,
        TASK_PRIORITY_NORMAL,
        &processing_task_handle
    );
    
    if (err != ESP_OK) {
        ERROR_REPORT(
            ERROR_SEVERITY_CRITICAL,
            ERROR_COMPONENT_SYSTEM,
            err,
            "Failed to create processing task",
            false,
            NULL
        );
        return err;
    }
    
    ESP_LOGI(TAG, "Módulo iniciado correctamente");
    return ESP_OK;
}

void my_module_stop() {
    ESP_LOGI(TAG, "Deteniendo módulo");
    
    if (processing_task_handle != NULL) {
        task_manager_delete(processing_task_handle);
        processing_task_handle = NULL;
    }
    
    ESP_LOGI(TAG, "Módulo detenido");
}

// Comando de diagnóstico
int cmd_my_module_status(int argc, char **argv) {
    ESP_LOGI(TAG, "═══════════════════════════════");
    ESP_LOGI(TAG, "  MY MODULE - Status");
    ESP_LOGI(TAG, "═══════════════════════════════");
    
    // Estado de la tarea
    if (processing_task_handle != NULL) {
        task_info_t* info = task_manager_get_info(processing_task_handle);
        if (info != NULL) {
            ESP_LOGI(TAG, "Task: %s", info->is_running ? "RUNNING" : "STOPPED");
            ESP_LOGI(TAG, "Stack: %lu bytes", info->stack_watermark);
        }
    }
    
    // Memoria
    heap_stats_t stats = heap_monitor_get_stats();
    ESP_LOGI(TAG, "Free heap: %zu KB", stats.total_free / 1024);
    
    ESP_LOGI(TAG, "═══════════════════════════════");
    
    return 0;
}
```

---

## 📋 Guía Rápida de Migración

### Checklist para Migrar un Módulo

1. **Incluir headers:**
   ```c
   #include "task_manager.h"
   #include "memory_monitor.h"
   #include "error_handler.h"
   ```

2. **Reemplazar `xTaskCreate` con `task_manager_create`:**
   ```c
   // ANTES
   xTaskCreate(my_task, "my_task", 4096, NULL, 5, &handle);
   
   // DESPUÉS
   task_manager_create(my_task, "my_task", TASK_STACK_MEDIUM, NULL, 
                      TASK_PRIORITY_NORMAL, &handle);
   ```

3. **Reemplazar `vTaskDelete` con `task_manager_delete`:**
   ```c
   // ANTES
   vTaskDelete(handle);
   
   // DESPUÉS
   task_manager_delete(handle);
   ```

4. **Agregar error reporting en puntos críticos:**
   ```c
   if (err != ESP_OK) {
       ERROR_WIFI(err, "Operation failed");
   }
   ```

5. **Opcional: Verificar memoria antes de operaciones pesadas:**
   ```c
   heap_stats_t stats = heap_monitor_get_stats();
   if (stats.total_free < 20 * 1024) {
       ESP_LOGW(TAG, "Low memory, skipping operation");
       return ESP_ERR_NO_MEM;
   }
   ```

---

**¡Listo!** Ahora tienes ejemplos completos para usar los core components en tus módulos.

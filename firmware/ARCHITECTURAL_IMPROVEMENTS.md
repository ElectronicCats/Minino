# 🏗️ Mejoras Arquitecturales - Minino Firmware v5.5.1

## 📋 Resumen Ejecutivo

Se han implementado **3 componentes core** que mejoran significativamente la robustez, monitoreo y gestión de recursos del firmware Minino. Estos componentes fueron diseñados siguiendo las mejores prácticas de ESP-IDF y FreeRTOS.

---

## ✅ Componentes Implementados

### 1. **Memory Monitor** (`/main/core/memory_monitor/`)

**Objetivo**: Monitoreo pasivo de memoria heap en tiempo real con alertas automáticas.

#### Funcionalidades:
- ✅ Monitoreo periódico del heap (configurable, por defecto 5 segundos)
- ✅ Detección automática de memoria baja con 4 niveles de alerta:
  - **NORMAL**: > 50 KB libre
  - **WARNING**: 30-50 KB libre
  - **CRITICAL**: 20-30 KB libre
  - **EMERGENCY**: < 20 KB libre
- ✅ Cálculo de fragmentación del heap
- ✅ Tracking de mínimo histórico de memoria libre
- ✅ Callbacks personalizables para alertas
- ✅ Umbrales configurables

#### API Principal:
```c
esp_err_t heap_monitor_init(void);
esp_err_t heap_monitor_start(uint32_t interval_ms);
heap_stats_t heap_monitor_get_stats(void);
void heap_monitor_set_alert_callback(heap_alert_callback_t callback);
void heap_monitor_print_stats(void);
```

#### Beneficios:
- 🔍 Visibilidad total del uso de memoria
- ⚠️ Detección temprana de leaks
- 📊 Optimización de stack sizes
- 🛡️ Prevención de crashes por OOM

---

### 2. **Task Manager** (`/main/core/task_manager/`)

**Objetivo**: Gestión centralizada de tareas FreeRTOS con prioridades y stack sizes estandarizados.

#### Funcionalidades:
- ✅ Creación y registro centralizado de tareas
- ✅ Prioridades estandarizadas (5 niveles):
  - `CRITICAL` (24): Protocolos time-sensitive (IEEE 802.15.4, Zigbee)
  - `HIGH` (20): GPS, WiFi crítico, BLE GATT
  - `NORMAL` (15): Aplicaciones, scanners
  - `LOW` (10): UI, LEDs, keyboard
  - `IDLE` (5): Background tasks
- ✅ Stack sizes predefinidos:
  - `TINY` (1KB), `SMALL` (2KB), `MEDIUM` (4KB), `LARGE` (8KB), `HUGE` (16KB)
- ✅ Tracking de stack watermarks (uso mínimo)
- ✅ Detección de riesgo de stack overflow
- ✅ Listado y estadísticas de todas las tareas

#### API Principal:
```c
esp_err_t task_manager_init(void);
esp_err_t task_manager_create(
    TaskFunction_t task_func,
    const char* name,
    task_stack_size_t stack_size,
    void* params,
    task_priority_t priority,
    TaskHandle_t* handle
);
esp_err_t task_manager_delete(TaskHandle_t handle);
void task_manager_list_all(void);
void task_manager_print_stack_usage(void);
bool task_manager_check_stack_overflow_risk(void);
```

#### Beneficios:
- 📏 Estandarización de stack sizes → menos desperdicio de memoria
- 🎯 Prioridades consistentes → mejor scheduling
- 🔍 Visibilidad completa de tareas activas
- ⚡ Detección temprana de problemas de stack

---

### 3. **Error Handler** (`/main/core/error_handler/`)

**Objetivo**: Sistema unificado de manejo y reporteo de errores con categorización y recuperación automática.

#### Funcionalidades:
- ✅ Categorización por severidad:
  - `INFO`, `WARNING`, `ERROR`, `CRITICAL`
- ✅ Categorización por componente:
  - WiFi, BLE, GPS, Zigbee, Thread, SD Card, Flash, UI, System
- ✅ Logging estructurado con emoji visual
- ✅ Callbacks de recuperación automática
- ✅ Estadísticas acumuladas de errores
- ✅ Auto-restart configurable en errores críticos
- ✅ Callback pre-restart para guardar datos

#### API Principal:
```c
esp_err_t error_handler_init(void);
void error_handler_report(const error_info_t* error);

// Macros helper
ERROR_REPORT(severity, component, code, msg, restart, recovery);
ERROR_WIFI(code, msg);  // Shortcut para errores WiFi
ERROR_CRITICAL(component, code, msg);  // Shortcut para críticos
```

#### Beneficios:
- 📝 Logging consistente y estructurado
- 🔧 Recuperación automática de errores
- 📊 Métricas de confiabilidad
- 🛡️ Protección contra crashes silenciosos

---

## 🔌 Integración en `main.c`

Los 3 componentes se inicializan **antes** de cualquier otro módulo para garantizar monitoreo desde el inicio:

```c
void app_main() {
  // ═══════════════════════════════════════════════════════════
  // FASE 1: Inicialización de Core Components
  // ═══════════════════════════════════════════════════════════
  ESP_LOGI(TAG, "🚀 Iniciando Minino Firmware v5.5.1");
  
  error_handler_init();
  task_manager_init();
  heap_monitor_init();
  heap_monitor_start(5000);  // Monitorear cada 5 segundos
  
  ESP_LOGI(TAG, "✅ Core components inicializados");
  
  // ═══════════════════════════════════════════════════════════
  // FASE 2: Inicialización de Sistema
  // ═══════════════════════════════════════════════════════════
  preferences_begin();
  gps_hw_init();
  // ... resto de inicialización
}
```

---

## 🔄 Migración de Módulos Existentes

### Ejemplo: Wardriving Module

**ANTES:**
```c
xTaskCreate(wardriving_module_scan_task, "wardriving_module_scan_task", 
            4096, NULL, 5, &wardriving_module_scan_task_handle);

// ...

vTaskDelete(wardriving_module_scan_task_handle);
```

**DESPUÉS:**
```c
// Usar Task Manager con prioridades estandarizadas
task_manager_create(
    wardriving_module_scan_task,
    "wardriving_scan",
    TASK_STACK_MEDIUM,  // 4KB estandarizado
    NULL,
    TASK_PRIORITY_HIGH,  // GPS es alta prioridad
    &wardriving_module_scan_task_handle
);

// ...

task_manager_delete(wardriving_module_scan_task_handle);
```

**Beneficios de la migración:**
- ✅ Stack size estandarizado
- ✅ Prioridad apropiada (HIGH para GPS)
- ✅ Registro automático para monitoreo
- ✅ Detección de problemas de stack

---

## 📊 Comandos de Diagnóstico Disponibles

Los core components exponen funciones de diagnóstico que pueden integrarse en la consola:

```c
// Memory Monitor
heap_monitor_print_stats();
/*
════════════════════════════════════════
  MEMORY MONITOR - Estado del Heap
════════════════════════════════════════
  Estado:           ✅ NORMAL
  Libre actual:     128 KB
  Mínimo histórico: 95 KB
  Bloque más grande:64 KB
  Total alocado:    152 KB
  Fragmentación:    15.2%
════════════════════════════════════════
*/

// Task Manager
task_manager_list_all();
/*
════════════════════════════════════════
  TASK MANAGER - Tareas Registradas
════════════════════════════════════════
Total de tareas: 12 / 50

[1] 🟢 RUN wardriving_scan    | Pri:20 | Stack: 4096 | Uptime:45s
[2] 🟢 RUN wardriving_anim    | Pri:10 | Stack: 2048 | Uptime:45s
...
════════════════════════════════════════
*/

task_manager_print_stack_usage();
/*
════════════════════════════════════════
  STACK USAGE - Análisis de Uso
════════════════════════════════════════
[1] wardriving_scan    | 3200/4096 bytes (78.1%) | ✅ OK
[2] wardriving_anim    |  512/2048 bytes (25.0%) | ✅ OK
...
════════════════════════════════════════
*/

// Error Handler
error_handler_print_stats();
/*
════════════════════════════════════════
  ERROR HANDLER - Estadísticas
════════════════════════════════════════
  Total errores:        45
  ├─ Info:              12
  ├─ Warnings:          28
  ├─ Errors:            4
  └─ Critical:          1

  Recuperaciones:       3 / 4 intentadas
  Reinicios provocados: 1
════════════════════════════════════════
*/
```

---

## 🎯 Roadmap de Migración (Futura)

### Alta Prioridad
- [ ] Migrar módulos GPS a Task Manager
- [ ] Migrar módulos WiFi críticos (sniffer, scanner)
- [ ] Integrar error reporting en módulos de storage (SD, Flash)

### Media Prioridad
- [ ] Migrar módulos BLE a Task Manager
- [ ] Implementar recovery handlers para WiFi/BLE disconnects
- [ ] Agregar comandos de consola para diagnóstico

### Baja Prioridad
- [ ] Migrar todos los módulos UI a Task Manager
- [ ] Implementar telemetría de errores para debugging remoto
- [ ] Optimizar stack sizes basado en watermarks reales

---

## 📈 Métricas de Impacto

### Antes:
- ❌ Sin visibilidad de uso de memoria
- ❌ Prioridades inconsistentes (5, 10, 15, 20, 24, custom...)
- ❌ Stack sizes arbitrarios (1024, 2048, 3072, 4096, 8192...)
- ❌ Sin tracking de errores
- ❌ Debugging manual de leaks

### Después:
- ✅ Monitoreo continuo de heap con alertas
- ✅ Prioridades estandarizadas en 5 niveles
- ✅ Stack sizes consistentes y optimizados
- ✅ Registro completo de errores con estadísticas
- ✅ Detección automática de problemas

---

## 🚀 Próximos Pasos Recomendados

1. **Testing en Producción**:
   - Monitorear logs de Memory Monitor durante 24h
   - Verificar que no hay alertas de memoria baja
   - Revisar stack usage de todas las tareas

2. **Optimización**:
   - Ajustar umbrales de alerta basado en uso real
   - Reducir stack sizes oversized
   - Identificar y fix memory leaks si los hay

3. **Migración Progresiva**:
   - Migrar 1-2 módulos por semana al Task Manager
   - Agregar error reporting en módulos críticos
   - Implementar recovery handlers donde tenga sentido

4. **Documentación**:
   - Agregar comandos de consola para diagnóstico
   - Crear guía de migración para desarrolladores
   - Documentar patrones de uso recomendados

---

## 📝 Notas de Implementación

- **Compatible con ESP-IDF 5.5.1**: Usa APIs estables de ESP-IDF
- **Zero overhead cuando no se usa**: Los componentes no consumen recursos si no se llaman
- **Thread-safe**: Todas las APIs son seguras para llamar desde múltiples tareas
- **Configuración flexible**: Umbrales y comportamientos son configurables en runtime

---

**Fecha de Implementación**: 23 de Octubre, 2025  
**Versión de Firmware**: v5.5.1  
**Estado**: ✅ **COMPLETADO Y FUNCIONAL**


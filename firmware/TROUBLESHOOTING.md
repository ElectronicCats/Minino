# 🔧 Troubleshooting Guide - Migración ESP-IDF v5.5.1

## 📋 Tabla de Contenidos

1. [Core Components (NUEVO)](#core-components-nuevo)
2. [Errores de Compilación](#errores-de-compilación)
3. [Errores de Ejecución](#errores-de-ejecución)
4. [Problemas de Protocolos](#problemas-de-protocolos)
5. [Problemas de Memoria](#problemas-de-memoria)
6. [Comandos Útiles](#comandos-útiles)
7. [Debugging Avanzado](#debugging-avanzado)

---

## 1. Core Components (NUEVO)

### 🚀 Uso de los Nuevos Core Components

El firmware ahora incluye 3 componentes core que mejoran robustez y monitoreo:

#### Memory Monitor

**¿Qué hace?**  
Monitorea automáticamente el heap y alerta cuando la memoria es baja.

**Comandos de diagnóstico:**
```c
// Ver estadísticas de memoria
heap_monitor_print_stats();

// Obtener estado actual
heap_alert_level_t level = heap_monitor_get_alert_level();
```

**Alertas automáticas en logs:**
```
W (12345) heap_monitor: ⚠️  Memoria baja: 45 KB libre (fragmentación: 12.5%)
E (12350) heap_monitor: 🔴 Memoria crítica: 25 KB libre! Considera liberar recursos
```

#### Task Manager

**¿Qué hace?**  
Gestiona todas las tareas con prioridades y stack sizes estandarizados.

**Comandos de diagnóstico:**
```c
// Listar todas las tareas
task_manager_list_all();

// Ver uso de stack
task_manager_print_stack_usage();

// Verificar riesgo de overflow
if (task_manager_check_stack_overflow_risk()) {
    ESP_LOGW(TAG, "Hay tareas con stack muy lleno!");
}
```

**Prioridades estandarizadas:**
- `TASK_PRIORITY_CRITICAL` (24) - Protocolos time-sensitive
- `TASK_PRIORITY_HIGH` (20) - GPS, WiFi, BLE crítico
- `TASK_PRIORITY_NORMAL` (15) - Apps, scanners
- `TASK_PRIORITY_LOW` (10) - UI, LEDs
- `TASK_PRIORITY_IDLE` (5) - Background tasks

#### Error Handler

**¿Qué hace?**  
Sistema centralizado de reporteo y recuperación de errores.

**Comandos de diagnóstico:**
```c
// Ver estadísticas de errores
error_handler_print_stats();
```

**Macros para reportar errores:**
```c
// Error genérico
ERROR_REPORT(ERROR_SEVERITY_WARNING, ERROR_COMPONENT_WIFI, 
             err, "WiFi failed", false, NULL);

// Shortcuts
ERROR_WIFI(err, "Connection failed");
ERROR_CRITICAL(ERROR_COMPONENT_SD_CARD, err, "SD mount failed");
```

### 🔍 Troubleshooting con Core Components

#### Problema: Stack Overflow

**Síntoma:**
```
***ERROR*** A stack overflow in task my_task has been detected.
```

**Diagnóstico:**
```c
task_manager_print_stack_usage();
```

**Solución:**
Migrar la tarea al Task Manager con stack size mayor:
```c
// ANTES: xTaskCreate(task, "name", 2048, NULL, 5, &handle);

// DESPUÉS: Usar stack MEDIUM (4KB) o LARGE (8KB)
task_manager_create(task, "name", TASK_STACK_LARGE, NULL, 
                   TASK_PRIORITY_NORMAL, &handle);
```

#### Problema: Out of Memory (OOM)

**Síntoma:**
```
E (12345) heap: Failed to allocate 8192 bytes
```

**Diagnóstico:**
```c
heap_monitor_print_stats();
```

**Análisis:**
1. ¿Cuánta memoria está libre actualmente?
2. ¿Cuál es el bloque contiguo más grande?
3. ¿Hay fragmentación alta (>30%)?

**Soluciones:**
- Si hay fragmentación: reiniciar componentes para liberar memoria
- Si falta memoria: reducir cache sizes, buffers, o cerrar features
- Si es temporal: implementar pooling de memoria

#### Problema: Prioridades Incorrectas

**Síntoma:**
Tareas de UI tienen más prioridad que tareas críticas, causando latencia.

**Diagnóstico:**
```c
task_manager_list_all();
```

**Solución:**
Migrar tareas al Task Manager con prioridades correctas:
- GPS/WiFi/BLE → `TASK_PRIORITY_HIGH`
- UI/Display → `TASK_PRIORITY_LOW`
- Protocolos (Zigbee) → `TASK_PRIORITY_CRITICAL`

#### Problema: Memory Leak

**Síntoma:**
Memoria libre disminuye progresivamente.

**Diagnóstico:**
```c
// Monitorear por 5 minutos
heap_stats_t stats = heap_monitor_get_stats();
ESP_LOGI(TAG, "Mínimo histórico: %zu KB", stats.min_free_ever / 1024);

// Si min_free_ever sigue bajando → hay leak
```

**Solución:**
1. Identificar el módulo responsable (suspender uno por uno)
2. Revisar `malloc()` sin `free()` correspondiente
3. Verificar que tareas deletadas liberaron recursos

---

**Para más detalles, ver:**
- `ARCHITECTURAL_IMPROVEMENTS.md` - Documentación completa
- `CODE_EXAMPLES.md` - Ejemplos de uso

---

## 2. Errores de Compilación

### Error: "esp_wifi_set_config" undefined reference

**Síntoma:**
```
undefined reference to `esp_wifi_set_config'
```

**Causa:** API cambió en ESP-IDF v5.4.x

**Solución:**
```bash
# Verificar documentación actualizada
idf.py docs

# Buscar nuevo nombre de función en headers
grep -r "esp_wifi_set" $IDF_PATH/components/esp_wifi/include/
```

**Acción:** Actualizar código según nueva API

---

### Error: Incompatible types en estructuras WiFi/BT

**Síntoma:**
```c
error: incompatible types when assigning to type 'wifi_config_t' from type 'int'
```

**Causa:** Cambios en estructuras de datos

**Solución:**
1. Revisar el header actualizado:
```bash
cat $IDF_PATH/components/esp_wifi/include/esp_wifi_types.h
```

2. Actualizar inicialización de estructuras:
```c
// Antes (v5.3.2)
wifi_config_t config = WIFI_INIT_CONFIG_DEFAULT();

// Después (v5.4.x) - verificar cambios
wifi_config_t config = {
    .sta = {
        .ssid = "...",
        .password = "...",
        // Nuevos campos si aplica
    }
};
```

---

### Error: managed components version mismatch

**Síntoma:**
```
ERROR: Version conflict for component espressif/esp-zigbee-lib
```

**Solución:**
```bash
# Actualizar dependencias
cd firmware/
idf.py update-dependencies

# Si persiste, limpiar y reconstruir
rm -rf managed_components/ dependencies.lock
idf.py reconfigure
```

---

### Error: Partitions too large for flash

**Síntoma:**
```
Error: Partition table binary size 0x... does not fit in configured size 0x...
```

**Solución:**
1. Revisar `partitions.csv`:
```bash
cat partitions.csv
```

2. Ajustar tamaños (si OTA dual no es necesario):
```csv
# Opción: Convertir a OTA single (gana ~3.7MB)
ota_0, app, ota_0, , 7M,
# Eliminar ota_1
```

3. Recompilar:
```bash
idf.py fullclean
idf.py build
```

---

### Error: Stack overflow en compilación

**Síntoma:**
```
fatal error: too many errors emitted, stopping now
```

**Solución:**
```bash
# Aumentar memoria del compilador
export CFLAGS="-Wno-error"

# Compilar con verbose para identificar problema
idf.py -v build 2>&1 | tee build.log

# Revisar log
grep -i "error" build.log
```

---

## 2. Errores de Ejecución

### Device no bootea después de flash

**Síntoma:**
- Device no arranca
- Bootloop
- No hay salida serial

**Diagnóstico:**
```bash
# Monitor serial para ver bootloader
idf.py -p /dev/ttyUSB0 monitor

# Verificar bootloader
esptool.py --port /dev/ttyUSB0 read_flash 0x0 0x10000 bootloader_backup.bin
```

**Solución:**
1. **Re-flash bootloader**:
```bash
cd firmware/build
esptool.py --port /dev/ttyUSB0 --chip esp32c6 \
    write_flash 0x0 bootloader/bootloader.bin
```

2. **Full erase + reflash**:
```bash
idf.py -p /dev/ttyUSB0 erase-flash
idf.py -p /dev/ttyUSB0 flash
```

---

### Watchdog timeout: Task did not reset watchdog

**Síntoma:**
```
E (12345) task_wdt: Task watchdog got triggered. The following tasks did not reset the watchdog in time:
```

**Causa:** Tarea bloqueante o loop infinito

**Solución:**
1. **Identificar tarea problemática**:
```c
// En la tarea sospechosa, añadir:
esp_task_wdt_reset();  // Reset watchdog
```

2. **Aumentar timeout** (temporal para debugging):
```ini
# sdkconfig
CONFIG_ESP_TASK_WDT_TIMEOUT_S=30
```

3. **Refactorizar tarea bloqueante**:
```c
// MALO:
while(condition) {
    // Trabajo intensivo sin delays
}

// BUENO:
while(condition) {
    // Trabajo intensivo
    vTaskDelay(pdMS_TO_TICKS(10));  // Dar tiempo a otras tareas
    esp_task_wdt_reset();
}
```

---

### Guru Meditation Error: Core panic

**Síntoma:**
```
Guru Meditation Error: Core 0 panic'ed (LoadProhibited)
```

**Diagnóstico:**
```bash
# Usar addr2line para encontrar línea exacta
xtensa-esp32c6-elf-addr2line -e build/minino.elf 0x40380000
```

**Causas comunes:**
1. **NULL pointer dereference**
2. **Stack overflow**
3. **Heap corruption**

**Solución:**
1. **Enable heap debugging** (desarrollo):
```ini
# sdkconfig
CONFIG_HEAP_POISONING_COMPREHENSIVE=y
CONFIG_HEAP_TRACING_STANDALONE=y
```

2. **Verificar stack sizes**:
```c
// Usar task_manager con stack sizes adecuados
task_manager_create(..., TASK_STACK_MEDIUM, ...);
```

---

### Memory allocation failed

**Síntoma:**
```
E (1234) component: Failed to allocate X bytes
```

**Diagnóstico:**
```c
// Añadir en código:
size_t free_heap = esp_get_free_heap_size();
size_t min_free = esp_get_minimum_free_heap_size();
ESP_LOGI(TAG, "Free heap: %zu bytes (min ever: %zu)", free_heap, min_free);
```

**Solución:**
1. **Usar memory pools**:
```c
// En vez de malloc/free
void* obj = mem_pool_alloc(POOL_WIFI_SCAN_RESULT);
// ... usar ...
mem_pool_free(POOL_WIFI_SCAN_RESULT, obj);
```

2. **Liberar memoria no usada**:
```c
// Detener features no críticas
if (esp_get_free_heap_size() < 50000) {
    // Stop animations, logs, etc
}
```

---

## 3. Problemas de Protocolos

### WiFi no conecta / scan falla

**Síntoma:**
- WiFi scan no encuentra APs
- No se puede conectar a AP

**Diagnóstico:**
```bash
# En monitor serial, ejecutar:
wifi scan
list
connect 0
```

**Soluciones:**

1. **Verificar inicialización**:
```c
// Verificar orden correcto
esp_netif_init();
esp_event_loop_create_default();
wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
esp_wifi_init(&cfg);
esp_wifi_set_mode(WIFI_MODE_STA);
esp_wifi_start();
```

2. **Verificar coexistencia**:
```ini
# Si BLE está activo, configurar coexistencia
CONFIG_ESP_WIFI_SW_COEXIST_ENABLE=y
CONFIG_ESP_COEX_SW_COEXIST_ENABLE=y
```

3. **Verificar región WiFi**:
```c
// Asegurarse de configurar país
wifi_country_t country = {
    .cc = "MX",  // México
    .schan = 1,
    .nchan = 11,
    .policy = WIFI_COUNTRY_POLICY_AUTO
};
esp_wifi_set_country(&country);
```

---

### BLE scan no detecta devices

**Síntoma:**
- BLE scan no encuentra dispositivos
- Error al iniciar scan

**Diagnóstico:**
```bash
# Comandos de prueba
ble scan
```

**Soluciones:**

1. **Verificar que WiFi no esté interfiriendo**:
```c
// Detener WiFi antes de BLE scan
esp_wifi_stop();
// BLE scan
// Reiniciar WiFi
esp_wifi_start();
```

2. **Verificar parámetros de scan**:
```c
static esp_ble_scan_params_t ble_scan_params = {
    .scan_type = BLE_SCAN_TYPE_ACTIVE,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_interval = 0x50,  // 50ms
    .scan_window = 0x30,    // 30ms
    .scan_duplicate = BLE_SCAN_DUPLICATE_DISABLE
};
```

---

### Zigbee no forma red

**Síntoma:**
- Zigbee CLI no puede formar red
- Devices no se unen a red

**Diagnóstico:**
```bash
# En Zigbee CLI
bdb_comm start form
state
```

**Soluciones:**

1. **Verificar configuración de canal**:
```c
// Asegurarse que canal no esté en uso por WiFi
esp_zb_set_primary_network_channel_set(ESP_ZB_PRIMARY_CHANNEL_MASK);
```

2. **Verificar coexistencia IEEE 802.15.4**:
```ini
CONFIG_IEEE802154_ENABLED=y
CONFIG_IEEE802154_RECEIVE_DONE_HANDLER=y
```

3. **Factory reset**:
```c
esp_zb_factory_reset();
```

---

### GPS no obtiene fix

**Síntoma:**
- GPS no obtiene posición
- Satélites visibles pero no fix

**Diagnóstico:**
```bash
# En console
gps info
```

**Soluciones:**

1. **Verificar configuración GNSS**:
```c
// Asegurarse de habilitar multi-constelación
gps_module_configure_advanced();
```

2. **Verificar antena**:
- Verificar conexión de antena GPS
- Probar en exterior (no indoor)

3. **Cold start**:
```c
// Enviar comando de cold start al GPS
gps_module_send_command("$PCAS03,0,0,0,0,0,0,0,0*02\r\n");
```

---

## 4. Problemas de Memoria

### Heap fragmentation

**Síntoma:**
```
E (1234) heap: Failed to allocate 2048 bytes (free heap: 50000)
```

**Diagnóstico:**
```c
multi_heap_info_t info;
heap_caps_get_info(&info, MALLOC_CAP_DEFAULT);

ESP_LOGI(TAG, "Heap info:");
ESP_LOGI(TAG, "  Total free: %zu", info.total_free_bytes);
ESP_LOGI(TAG, "  Largest free block: %zu", info.largest_free_block);
ESP_LOGI(TAG, "  Fragmentation: %.1f%%", 
         100.0 * (1.0 - (float)info.largest_free_block / info.total_free_bytes));
```

**Solución:**
1. **Usar memory pools** (previene fragmentación)
2. **Reiniciar periódicamente** si fragmentación > 50%
3. **Evitar allocations grandes en runtime**

---

### Stack overflow en tarea

**Síntoma:**
```
***ERROR*** A stack overflow in task XXX has been detected.
```

**Diagnóstico:**
```c
// Monitorear high water mark de tareas
UBaseType_t uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
ESP_LOGI(TAG, "Stack high water mark: %lu bytes free", uxHighWaterMark * 4);
```

**Solución:**
1. **Aumentar stack size**:
```c
// Si stack < 10% libre, aumentar
task_manager_create(..., TASK_STACK_LARGE, ...);
```

2. **Reducir variables locales grandes**:
```c
// MALO:
void my_func() {
    uint8_t buffer[8192];  // 8KB en stack!
}

// BUENO:
void my_func() {
    uint8_t* buffer = malloc(8192);  // En heap
    // ...
    free(buffer);
}
```

---

## 5. Comandos Útiles

### Comandos ESP-IDF

```bash
# Información del proyecto
idf.py --version
idf.py size
idf.py size-components
idf.py size-files

# Build
idf.py build                    # Build normal
idf.py -v build                 # Build verbose
idf.py fullclean build          # Clean + build
idf.py app                      # Build solo app (no bootloader)

# Flash
idf.py -p PORT flash            # Flash todo
idf.py -p PORT app-flash        # Flash solo app
idf.py -p PORT erase-flash      # Erase completo

# Monitor
idf.py -p PORT monitor          # Monitor serial
idf.py -p PORT flash monitor    # Flash + monitor

# Configuración
idf.py menuconfig               # Config interactiva
idf.py reconfigure              # Re-generar config
idf.py save-defconfig           # Guardar config actual

# Partitions
idf.py partition-table          # Mostrar tabla particiones
idf.py partition-table-flash    # Flash solo tabla

# Dependencies
idf.py update-dependencies      # Actualizar managed components
```

### Análisis de Tamaño

```bash
# Ver componentes más grandes
idf.py size-components | sort -k2 -nr | head -20

# Ver archivos más grandes
idf.py size-files | sort -k2 -nr | head -30

# Analizar símbolos
xtensa-esp32c6-elf-nm -S -l --size-sort build/minino.elf | tail -50
```

### Debugging de Heap

```bash
# Habilitar heap tracing
# En sdkconfig:
CONFIG_HEAP_TRACING_STANDALONE=y
CONFIG_HEAP_TRACING_STACK_DEPTH=10

# En código:
heap_trace_start(HEAP_TRACE_LEAKS);
// ... código sospechoso ...
heap_trace_stop();
heap_trace_dump();
```

### Análisis de Tasks

```bash
# En monitor serial
tasks
cpu

# O programáticamente:
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

char* task_list = malloc(2048);
vTaskList(task_list);
printf("%s\n", task_list);
free(task_list);
```

---

## 6. Debugging Avanzado

### GDB Debugging via JTAG

```bash
# Iniciar OpenOCD (terminal 1)
openocd -f board/esp32c6-builtin.cfg

# Iniciar GDB (terminal 2)
xtensa-esp32c6-elf-gdb -x gdbinit build/minino.elf

# En GDB:
(gdb) target remote :3333
(gdb) monitor reset halt
(gdb) b app_main
(gdb) c
```

### Core Dump Analysis

```bash
# Si coredump está habilitado, extraer después de crash
idf.py coredump-info

# Analizar con GDB
idf.py coredump-debug
```

### Network Debugging

```bash
# Capturar paquetes WiFi
idf.py monitor

# En otro terminal, capturar con Wireshark
# (requiere promiscuous mode)
```

### Performance Profiling

```bash
# Habilitar profiling
# En sdkconfig:
CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS=y
CONFIG_FREERTOS_USE_TRACE_FACILITY=y

# En código:
TaskStatus_t* tasks = malloc(sizeof(TaskStatus_t) * 50);
UBaseType_t task_count = uxTaskGetSystemState(tasks, 50, NULL);
// Analizar tasks[]
free(tasks);
```

---

## Contactos de Soporte

### Recursos Oficiales
- **ESP-IDF Forum**: https://esp32.com/
- **GitHub Issues**: https://github.com/espressif/esp-idf/issues
- **Documentation**: https://docs.espressif.com/projects/esp-idf/

### Electronic Cats
- **GitHub**: https://github.com/ElectronicCats/Minino
- **Discord/Slack**: [Canal específico del proyecto]

---

## Logs de Debugging Útiles

### Template para reportar bugs

```
## Environment
- ESP-IDF Version: v5.4.x
- Board: ESP32-C6 (Minino)
- OS: Linux/Windows/macOS

## Issue Description
[Descripción clara del problema]

## Steps to Reproduce
1. ...
2. ...
3. ...

## Expected Behavior
[Qué debería pasar]

## Actual Behavior
[Qué está pasando]

## Logs
```
[Logs relevantes del monitor serial]
```

## Code Snippet
```c
[Código relevante]
```

## Additional Info
- Heap free: XXX bytes
- Task stack: XXX bytes
- Compilation warnings: [Si aplica]
```

---

**Última Actualización**: Octubre 2025  
**Versión**: 1.0  
**Mantenido por**: Equipo Minino



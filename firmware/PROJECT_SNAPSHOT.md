# 📸 Project Snapshot - Estado Actual Pre-Migración

**Documento de referencia del estado del proyecto antes de la migración**

---

## 📊 Estadísticas del Proyecto

### Código Base
```
Total archivos C/H:              1,057 archivos
Componentes personalizados:      39 componentes
Managed components:              6 dependencias
Tamaño del codebase:             ~5-7 MB (estimado)
```

### Uso de FreeRTOS
```
Archivos con xTaskCreate:        44 archivos
Tareas estimadas totales:        50-80 tareas
Uso de vTaskDelay:              154 ocurrencias
Uso de queues/semaphores:       Moderado
```

### APIs y Dependencias
```
ESP WiFi APIs:                  ~150 usos
ESP Bluetooth APIs:             ~120 usos
ESP Network APIs:               ~100 usos
TODOs en código:                5 encontrados
```

---

## 🏗️ Arquitectura Actual

### Estructura de Directorios

```
firmware/
├── main/ (244 archivos)
│   ├── apps/              109 archivos
│   │   ├── wifi/          Aplicaciones WiFi
│   │   ├── ble/           Aplicaciones Bluetooth
│   │   └── thread_sniffer Thread sniffer
│   ├── modules/           115 archivos
│   │   ├── gps/           GPS y wardriving
│   │   ├── zigbee/        Zigbee functionality
│   │   ├── settings/      Configuración
│   │   └── menus_module/  Sistema de menús
│   ├── general/           20 archivos
│   │   └── utilities      Utilidades generales
│   └── drivers/           5 archivos
│       └── oled_driver    Driver OLED
│
├── components/ (39 componentes)
│   ├── wifi_controller/   Control WiFi
│   ├── ble_hid/          BLE HID
│   ├── openthread/       OpenThread (3248 archivos!)
│   ├── zigbee_switch/    Zigbee
│   ├── nmea_parser/      GPS NMEA
│   └── [34 más...]
│
└── managed_components/ (6 externos)
    ├── espressif/esp-zboss-lib
    ├── espressif/esp-zigbee-lib
    ├── espressif/button
    ├── espressif/iperf
    ├── espressif/console
    └── espressif/esp-modbus
```

---

## 🛠️ Componentes Personalizados

### Protocolos Inalámbricos
```
✓ wifi_controller       - Control WiFi base
✓ wifi_scanner          - Scanner WiFi
✓ wifi_attacks          - Ataques WiFi (deauth, etc)
✓ wifi_sniffer          - Sniffer de paquetes
✓ wifi_app              - Aplicación WiFi
✓ wifi_ap_manager       - Gestión de AP

✓ ble_hid               - BLE HID device
✓ ble_scann             - Scanner BLE
✓ bt_gattc              - GATT Client
✓ bt_gatts              - GATT Server
✓ bt_spam               - BT Spam

✓ zigbee_switch         - Switch Zigbee
✓ zb_cli                - CLI Zigbee
✓ esp-zigbee-console    - Console Zigbee

✓ openthread            - OpenThread stack
✓ open_thread           - OpenThread wrapper
✓ thread_broadcast      - Thread broadcast
✓ ieee802154            - IEEE 802.15.4 driver
```

### Periféricos y Sistema
```
✓ nmea_parser           - Parser GPS
✓ sd_card               - SD card management
✓ flash_fs              - Flash filesystem
✓ preferences           - NVS preferences
✓ buzzer                - Buzzer control
✓ leds                  - LEDs control
✓ uart_bridge           - UART bridge
✓ uart_sender           - UART sender
```

### Utilidades
```
✓ files_ops             - Operaciones de archivos
✓ dns_server            - DNS server
✓ OTA                   - OTA updates
✓ drone_id              - Drone ID (RemoteID)
✓ console               - Console (custom)
✓ trackers_scanner      - Tracker detection
✓ cmd_wifi              - Comandos WiFi
```

---

## ⚙️ Configuración Actual

### ESP-IDF
```
Versión:                 v5.3.2
Target:                  ESP32-C6
Flash:                   8MB
Console:                 USB-Serial-JTAG
```

### FreeRTOS Config
```ini
CONFIG_FREERTOS_HZ=1000
CONFIG_FREERTOS_USE_TICKLESS_IDLE=y
CONFIG_ESP_MAIN_TASK_STACK_SIZE=7168
CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU0=n  ⚠️
CONFIG_FREERTOS_USE_TRACE_FACILITY=y
CONFIG_FREERTOS_USE_STATS_FORMATTING_FUNCTIONS=y
```

### Power Management
```ini
CONFIG_PM_ENABLE=y                          ✅
CONFIG_FREERTOS_USE_TICKLESS_IDLE=y         ✅
CONFIG_PM_SLP_IRAM_OPT=y                    ✅
CONFIG_PM_RTOS_IDLE_OPT=y                   ✅
CONFIG_GPIO_BUTTON_SUPPORT_POWER_SAVE=y     ✅
```

### Protocolos
```ini
CONFIG_BT_ENABLED=y                         ✅
CONFIG_BT_BLUEDROID_ENABLED=y               ✅
CONFIG_IEEE802154_ENABLED=y                 ✅
CONFIG_ZB_ENABLED=y                         ✅
CONFIG_OPENTHREAD_ENABLED=y                 ✅
```

---

## 💾 Particiones (8MB Flash)

```
Partición              Tipo      Tamaño    Uso
──────────────────────────────────────────────────
nvs                   data       24K      NVS storage
phy_init              data       4K       PHY calibration
zb_storage            data       16K      Zigbee storage ⚠️
zb_fct                data       1K       Zigbee factory
otadata               data       8K       OTA data
internal              data       512K     SPIFFS (assets)
ota_0                 app        3700K    App partition 0
ota_1                 app        3700K    App partition 1
──────────────────────────────────────────────────
Total:                           ~7.96MB  (99.5% uso) ⚠️
```

**⚠️ Observaciones:**
- OTA dual ocupa 7.4MB (muy grande)
- Zigbee storage pequeño (16KB)
- Poco margen para crecimiento

---

## 🔧 Funcionalidades Implementadas

### WiFi Features
```
✓ WiFi Scanner (2.4 GHz)
✓ WiFi Deauth Attack
✓ WiFi Captive Portal (custom HTML desde SD)
✓ WiFi SSID Spam
✓ WiFi Modbus TCP (client + attacks)
✓ WiFi DoS Attack
✓ WiFi Analyzer
✓ WiFi Sniffer (PCAP)
✓ Drone ID Scanner
```

### Bluetooth Features
```
✓ BLE Scanner
✓ BLE HID Device (keyboard/mouse)
✓ BLE GATT Commands (read/write)
✓ BLE Tracker Detection (AirTag, etc)
✓ BT Spam (proximity spam)
✓ BT GATT Client/Server
```

### Zigbee/Thread Features
```
✓ Zigbee CLI (commands)
✓ Zigbee Switch
✓ Zigbee Sniffer
✓ Zigbee Wardriving
✓ Thread Sniffer
✓ Thread Broadcast
✓ Thread Wardriving
```

### GPS Features
```
✓ NMEA Parser (multi-constellation)
✓ GPS Info Display
✓ Wardriving WiFi (GPS + WiFi scan)
✓ Wardriving Zigbee (GPS + Zigbee)
✓ Wardriving Thread (GPS + Thread)
✓ CSV export to SD card
```

### System Features
```
✓ OLED Display (SH1106)
✓ Menu System (interactive)
✓ Settings (persistent via NVS)
✓ SD Card (FAT filesystem)
✓ File Manager (local + web)
✓ OTA Updates (WiFi)
✓ Sleep Mode
✓ Stealth Mode
✓ Buzzer + LED notifications
✓ UART Bridge
✓ Console (USB-Serial)
```

---

## 🎯 Tareas Identificadas

### Por Módulo

**GPS Module** (~4 tareas)
- GPS parser task
- Wardriving WiFi task
- Wardriving Zigbee task
- Wardriving Thread task

**WiFi Apps** (~8 tareas)
- WiFi scanner task
- Captive portal task
- Deauth attack task
- Modbus engine task
- DoS attack task
- SSID spam task
- Drone ID scanner task
- WiFi sniffer task

**Bluetooth Apps** (~5 tareas)
- BLE scanner task
- BLE HID task
- GATT command task
- Tracker scanner task
- BT spam task

**Zigbee/Thread** (~12 tareas)
- Zigbee main task
- Zigbee network tasks (4-5)
- Zigbee CLI task
- Thread main task
- Thread tasks (3-4)
- IEEE sniffer task

**System/UI** (~8 tareas)
- OLED update task
- Menu handler task
- Keyboard task
- LED animation task
- Screen saver task
- Buzzer task
- Console task
- OTA task

**Total estimado**: ~50-80 tareas concurrentes (variable según modo)

---

## 📈 Uso de Recursos (Estimado)

### Memoria RAM
```
Heap total:              ~300KB
Heap usado (típico):     ~150-200KB
Heap libre (típico):     ~100-150KB
Stack total:             ~200-300KB (todas las tareas)
```

### Memoria Flash
```
Firmware size:           ~2-3MB (estimado)
Disponible en OTA:       3.7MB
Assets (SPIFFS):         512KB
Logs/Data (SD):          Ilimitado (depende SD)
```

### CPU
```
CPU utilization:         Variable (20-80%)
Idle time:              Significativo (power mgmt funciona)
```

---

## ⚠️ Problemas Conocidos

### TODOs en Código (5 encontrados)

```c
// main/modules/settings/sd_card/sd_card_settings_module.c:116
// TODO: implement on LEFT button

// main/modules/cmd_control/gpio/cmd_uart_bridge.c:140
// TODO: Add a command to change these values

// main/modules/cat_dos/catdos_module.c:130
// TODO : Change this, with the real animation

// main/apps/wifi_analyzer/wifi_analyzer.c:76
// TODO: add an option to format the SD card

// main/modules/ota/ota_module_screens.c:24
// TODO: Change to the new version
```

### Código Problemático

**1. Override de función de seguridad (wifi_attacks.c)**
```c
// ⚠️ PELIGROSO: Desactiva checks de seguridad para ataques WiFi
int ieee80211_raw_frame_sanity_check(int32_t arg, int32_t arg2, int32_t arg3) {
  return 0;
}
```

**2. Watchdog deshabilitado en CPU0**
```ini
CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU0=n  # ⚠️ Riesgoso
```

**3. Prioridades hardcodeadas**
```c
// Casi todas las tareas usan prioridad 5 (no diferenciación)
xTaskCreate(task, "name", 4096, NULL, 5, NULL);
```

---

## 🏆 Fortalezas del Proyecto

### Bien Implementado
✅ **Power Management** - Excelente configuración  
✅ **Multi-protocolo** - Soporta WiFi, BLE, Zigbee, Thread  
✅ **GPS Integration** - NMEA parser optimizado  
✅ **Wardriving** - Features únicas (WiFi/Zigbee/Thread)  
✅ **Modularidad** - Componentes bien separados  
✅ **Features** - Muchas características implementadas  
✅ **Hardware Support** - ESP32-C6 bien aprovechado  

### Oportunidades de Mejora
⚠️ **Task Management** - Sin gestión centralizada  
⚠️ **Memory Management** - Sin pools, potential leaks  
⚠️ **Error Handling** - Inconsistente  
⚠️ **Logging** - No estructurado  
⚠️ **Testing** - Sin unit tests  
⚠️ **Documentation** - Limitada  
⚠️ **Stack Sizes** - Sin justificación  

---

## 🔮 Proyección Post-Migración

### Cambios Esperados

**Código:**
- +3,000 líneas (task_manager, mem_pool, error_handler)
- ~500-1000 líneas modificadas (migraciones)
- ~50-100 archivos tocados

**Firmware Size:**
- Actual: ~2-3 MB
- Post-migración: ~2.5-3.3 MB (+10-15%)

**RAM Usage:**
- Similar o ligeramente menor (memory pools)

**Performance:**
- Similar o mejor (mejor gestión de tareas)

**Mantenibilidad:**
- Significativamente mejor

---

## 📝 Notas Adicionales

### Hardware (ESP32-C6)
```
Chip:           ESP32-C6 (RISC-V)
CPU:            Single core @ 160 MHz
RAM:            ~300 KB SRAM
Flash:          8 MB external
Radio:          2.4 GHz (WiFi, BLE, 802.15.4)
USB:            USB-Serial-JTAG built-in
GPIO:           22 GPIO pins
Peripherals:    SPI, I2C, UART, ADC, etc.
```

### Build Stats
```
Build time:     ~5-10 min (full clean build)
Incremental:    ~30-60 sec
Flash time:     ~30-60 sec
```

### Git History
```
Commits:        [Ver git log]
Contributors:   [Ver git contributors]
Last update:    [Ver git log -1]
```

---

## 🎯 Objetivos de la Migración

Basándose en este snapshot, la migración debe:

1. ✅ **Mantener** todas las funcionalidades existentes
2. ✅ **Mejorar** arquitectura de tareas (task_manager)
3. ✅ **Optimizar** uso de memoria (memory pools)
4. ✅ **Estandarizar** manejo de errores
5. ✅ **Documentar** arquitectura y APIs
6. ✅ **Validar** estabilidad (tests 24h)

---

**Snapshot tomado**: Octubre 2025  
**ESP-IDF Version**: v5.3.2  
**Branch**: [current branch]  
**Commit**: [git rev-parse HEAD]

---

**Este documento sirve como baseline para comparar post-migración.**


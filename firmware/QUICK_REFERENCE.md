# ⚡ Quick Reference - Migración ESP-IDF v5.4

**Una página de referencia rápida para tener a mano durante la migración**

---

## 🎯 Objetivo
Migrar Minino firmware de **ESP-IDF v5.3.2** → **v5.4.x**

---

## 📅 Timeline (15-18 días)

```
SEMANA 1          SEMANA 2          SEMANA 3
Day 1-3           Day 4-8           Day 9-13          Day 14-17         Day 18-19
┌─────────┐      ┌─────────┐       ┌─────────┐       ┌─────────┐       ┌─────────┐
│ Setup & │  →   │Migration│   →   │Mejoras  │   →   │ Testing │   →   │  Docs   │
│ Prepare │      │  Core   │       │Arquitect│       │& Validac│       │& Entrega│
└─────────┘      └─────────┘       └─────────┘       └─────────┘       └─────────┘
   3 días           5 días            5 días            4 días            2 días
```

---

## 🚦 Checklist Diario

### Cada Mañana
- [ ] Review plan del día
- [ ] Check documentación relevante
- [ ] Prepare entorno (terminal, editor, hardware)

### Durante el Día
- [ ] Commit frecuente (`git commit -m "..."`)
- [ ] Test después de cada cambio
- [ ] Documentar problemas encontrados

### Cada Tarde
- [ ] Mark checkboxes completados
- [ ] Push cambios a repo
- [ ] Update notas de progreso

---

## 🔧 Comandos Más Usados

### Build & Flash
```bash
# Build completo
idf.py build

# Build verbose (debugging)
idf.py -v build 2>&1 | tee build.log

# Full clean + build
idf.py fullclean build

# Flash + Monitor
idf.py -p /dev/ttyUSB0 flash monitor

# Solo app (más rápido)
idf.py -p /dev/ttyUSB0 app-flash
```

### Debugging
```bash
# Ver tamaño
idf.py size
idf.py size-components | sort -k2 -nr | head -20

# Monitorear
idf.py -p /dev/ttyUSB0 monitor

# Erase flash
idf.py -p /dev/ttyUSB0 erase-flash

# Config
idf.py menuconfig
```

### Git
```bash
# Status check
git status
git log --oneline -10

# Commit
git add .
git commit -m "Phase X: descripción"
git push origin migration/esp-idf-v5.4

# Checkpoint
git tag -a checkpoint-dayX -m "End of day X"
```

---

## 🐛 Troubleshooting Rápido

### Error: Build fails
```bash
# 1. Clean
idf.py fullclean

# 2. Update dependencies
rm -rf managed_components/ dependencies.lock
idf.py reconfigure

# 3. Check ESP-IDF version
idf.py --version
```

### Error: Device no bootea
```bash
# 1. Erase + reflash
idf.py -p PORT erase-flash
idf.py -p PORT flash

# 2. Monitor para ver logs
idf.py -p PORT monitor
```

### Error: Watchdog timeout
```c
// En código, añadir:
esp_task_wdt_reset();  // En loop de tarea

// O aumentar timeout temporal:
// sdkconfig: CONFIG_ESP_TASK_WDT_TIMEOUT_S=30
```

### Error: Out of memory
```c
// Check heap:
size_t free = esp_get_free_heap_size();
ESP_LOGI(TAG, "Free heap: %zu", free);

// Usar memory pools (ver CODE_EXAMPLES.md)
```

---

## 📋 Phases at a Glance

### Phase 1: Preparación (D1-3)
**Goal**: Setup environment
- [ ] Backup proyecto (git tag)
- [ ] Install ESP-IDF v5.4.x
- [ ] Primera compilación
- [ ] Documentar errores

### Phase 2: Core Migration (D4-8)
**Goal**: Compilar y bootear
- [ ] Fix compilation errors
- [ ] Update configs
- [ ] Primera flash exitosa
- [ ] Test funcionalidades básicas

### Phase 3: Mejoras (D9-13)
**Goal**: Implementar mejoras
- [ ] Task Manager
- [ ] Memory Manager
- [ ] Error Handler
- [ ] Migrar tareas existentes

### Phase 4: Testing (D14-17)
**Goal**: Validar todo
- [ ] Functional tests (WiFi, BLE, Zigbee, GPS)
- [ ] Integration tests
- [ ] Stress tests (24h)
- [ ] Fix bugs

### Phase 5: Documentación (D18-19)
**Goal**: Entregar
- [ ] Update README
- [ ] Create CHANGELOG
- [ ] Write docs
- [ ] Handoff meeting

---

## 🎨 Arquitectura Mejorada

### Estructura de Prioridades
```
Priority 24 (CRITICAL)     ← IEEE 802.15.4, Zigbee
Priority 20 (HIGH)         ← GPS, WiFi, BLE
Priority 15 (NORMAL)       ← Apps normales
Priority 10 (LOW)          ← UI, Logging
Priority 5 (IDLE)          ← Background
```

### Stack Sizes
```
TINY:   1024  bytes  ← Watchdog
SMALL:  2048  bytes  ← LED, UI
MEDIUM: 4096  bytes  ← Protocol handlers
LARGE:  8192  bytes  ← Complex tasks
HUGE:   16384 bytes  ← Avoid!
```

---

## 💾 Memory Pools

```c
// En vez de malloc/free:
wifi_ap_record_t* ap = mem_pool_alloc(POOL_WIFI_SCAN_RESULT);
// ... usar ...
mem_pool_free(POOL_WIFI_SCAN_RESULT, ap);
```

**Pools disponibles:**
- `POOL_WIFI_SCAN_RESULT` (50 objetos)
- `POOL_GPS_COORDINATE` (100 objetos)
- `POOL_BLE_ADV_DATA` (30 objetos)
- `POOL_ZIGBEE_PACKET` (20 objetos)

---

## 📊 Testing Checklist

### WiFi
- [ ] Scan
- [ ] Connect
- [ ] Deauth attack
- [ ] Captive portal
- [ ] Modbus TCP
- [ ] SSID spam

### Bluetooth
- [ ] BLE scan
- [ ] BLE HID
- [ ] GATT commands
- [ ] BT spam
- [ ] Tracker detection

### Zigbee/Thread
- [ ] CLI
- [ ] Sniffer
- [ ] Wardriving

### GPS
- [ ] NMEA parsing
- [ ] Multi-constellation
- [ ] Wardriving

### System
- [ ] OTA update
- [ ] SD card R/W
- [ ] OLED display
- [ ] Settings persist

---

## 🚨 Red Flags

Stop and investigate if you see:

❌ **> 100 compilation errors** after update  
→ Consider incremental migration

❌ **Bootloop** after flash  
→ Check bootloader, partitions

❌ **Heap < 30KB** during operation  
→ Memory leak or pool exhaustion

❌ **Watchdog resets** constantly  
→ Blocking task or infinite loop

❌ **Tests failing** after "working" code  
→ Regression, rollback and fix

---

## 📱 Hardware Test Points

**ESP32-C6 (Minino)**
- GPIO0: Boot button
- GPIO9: LED (?)
- UART: USB-JTAG-Serial (built-in)
- GPS: UART1 (check schematic)
- SD Card: SPI (check schematic)
- OLED: I2C or SPI (check schematic)

---

## 📞 Emergency Contacts

### Si estás bloqueado:
1. Check `TROUBLESHOOTING.md`
2. Search ESP32 Forum: https://esp32.com/
3. Check GitHub Issues: https://github.com/espressif/esp-idf/issues
4. Contact Electronic Cats team

### Si necesitas rollback:
```bash
git checkout v5.3.2-baseline
# Fix issue, then continue
```

---

## 💡 Tips & Tricks

### Compilación más rápida
```bash
# Build solo cambios (no full clean)
idf.py build

# Parallel build (usa todos los cores)
idf.py -j $(nproc) build
```

### Logs más limpios
```bash
# Filtrar solo errores
idf.py monitor | grep -i "error\|failed"

# Save logs
idf.py monitor 2>&1 | tee monitor.log
```

### Debugging de memoria
```c
// Al inicio de función problemática
size_t before = esp_get_free_heap_size();
// ... código ...
size_t after = esp_get_free_heap_size();
ESP_LOGI(TAG, "Heap used: %zu bytes", before - after);
```

---

## 🎯 Success Criteria

Project is done when:

✅ **100%** features working  
✅ **0** regressions  
✅ **100%** tests passing  
✅ **24h** stress test passed  
✅ **All** docs updated  
✅ **Handoff** meeting completed

---

## 📚 Document References

| Document | When to Use |
|----------|-------------|
| `MIGRATION_INDEX.md` | First time / Navigation |
| `MIGRATION_PLAN.md` | Planning / Deep dive |
| `IMPLEMENTATION_ROADMAP.md` | Daily tracking |
| `CODE_EXAMPLES.md` | During implementation |
| `TROUBLESHOOTING.md` | When stuck |
| `QUICK_REFERENCE.md` | Always (this doc!) |

---

## 🔄 Daily Progress Tracker

```
Week 1
[ ] Day 1  [ ] Day 2  [ ] Day 3  [ ] Day 4  [ ] Day 5

Week 2
[ ] Day 6  [ ] Day 7  [ ] Day 8  [ ] Day 9  [ ] Day 10

Week 3
[ ] Day 11 [ ] Day 12 [ ] Day 13 [ ] Day 14 [ ] Day 15

Week 4
[ ] Day 16 [ ] Day 17 [ ] Day 18 [ ] Day 19
```

---

## 🎓 Golden Rules

1. **Commit early, commit often**
2. **Test after every change**
3. **Document problems immediately**
4. **Don't skip phases**
5. **Ask for help when stuck > 2 hours**
6. **Backup before major changes**
7. **Read error messages carefully**
8. **Keep hardware accessible**
9. **Take breaks (avoid burnout)**
10. **Celebrate checkpoints!** 🎉

---

**Print this page and keep it on your desk!**

**Good luck! 🚀**

---

**Última Actualización**: Octubre 2025  
**Versión**: 1.0


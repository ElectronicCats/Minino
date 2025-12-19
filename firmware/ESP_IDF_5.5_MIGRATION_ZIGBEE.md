# Guía de Migración ESP-IDF 5.3.x → 5.5.x - Zigbee

## Resumen

Este documento describe los cambios necesarios para migrar el soporte Zigbee desde ESP-IDF 5.3.x a 5.5.x en el proyecto Minino.

## Cambios Principales

### 1. Handler Obligatorio `esp_zb_app_signal_handler`

**Problema**: Error de linking al compilar con `CONFIG_ZB_ENABLED=y`:
```
undefined reference to `esp_zb_app_signal_handler'
referenced from libesp_zb_api.zczr.a(esp_zigbee_core.c.obj)
```

**Causa**: La librería estática `libesp_zb_api.zczr.a` ahora requiere que el símbolo `esp_zb_app_signal_handler` esté definido en la aplicación, incluso si no se usa directamente Zigbee en el código principal.

**Solución**: Se implementó un handler mínimo en:
- `main/core/zigbee_handler/zigbee_signal_handler.c`
- `main/core/zigbee_handler/zigbee_signal_handler.h`

Este handler se incluye automáticamente cuando `CONFIG_ZB_ENABLED=y`.

### 2. Cambios en Kconfig

#### IEEE 802.15.4

**Estado**: `CONFIG_IEEE802154_RX_BUFFER_SIZE` **sigue existiendo** en ESP-IDF 5.5.x.

No hay cambios en la configuración de buffers de recepción. El símbolo sigue siendo válido y se usa correctamente en:
- `components/ieee802154/driver/esp_ieee802154_dev.c`
- `components/openthread/src/port/esp_openthread_radio.c`

#### Zigbee (ZBoss)

Las opciones principales de Zigbee permanecen iguales:
- `CONFIG_ZB_ENABLED`: Habilita/deshabilita Zigbee
- `CONFIG_ZB_ZCZR`: Coordinador/ZRouter
- `CONFIG_ZB_ZED`: End Device
- `CONFIG_ZB_ZGPD`: Green Power Device
- `CONFIG_ZB_DEBUG_MODE`: Modo debug
- `CONFIG_ZB_RADIO_SPINEL_UART`: Radio Spinel via UART

### 3. Configuración Requerida en `sdkconfig.defaults`

Para habilitar Zigbee, asegúrate de tener:

```ini
# IEEE 802.15.4 (requerido para Zigbee)
CONFIG_IEEE802154_ENABLED=y
CONFIG_IEEE802154_RECEIVE_DONE_HANDLER=y

# Zboss (Zigbee Stack)
CONFIG_ZB_ENABLED=y
CONFIG_ZB_ZCZR=y          # Para Coordinador/ZRouter
# o CONFIG_ZB_ZED=y       # Para End Device
# o CONFIG_ZB_ZGPD=y      # Para Green Power Device
```

### 4. Dependencias del Component Manager

Verificar en `main/idf_component.yml`:

```yaml
dependencies:
  espressif/esp-zboss-lib: ^1.6.4
  espressif/esp-zigbee-lib: ^1.6.8
```

### 5. CMakeLists.txt - Componente `espressif__esp-zigbee-lib`

**Cambio importante**: El componente `managed_components/espressif__esp-zigbee-lib/CMakeLists.txt` ahora siempre expone el directorio `include` para que otros componentes puedan acceder a los headers, incluso cuando `CONFIG_ZB_ENABLED=n`.

Esto es necesario porque componentes como `esp-zigbee-console` necesitan acceder a tipos definidos en los headers aunque Zigbee no esté completamente habilitado.

## Funciones Obligatorias en ESP-IDF 5.5.x

### Funciones que DEBEN estar definidas

1. **`esp_zb_app_signal_handler`** ⚠️ **OBLIGATORIA**
   - Debe estar definida cuando `CONFIG_ZB_ENABLED=y`
   - Es llamada por el stack Zigbee para notificar eventos
   - Ubicación recomendada: componente `main` o componente dedicado

### Funciones opcionales pero recomendadas

Ninguna función adicional es obligatoria. El handler de señales es la única función requerida a nivel de aplicación.

## Checklist de Migración

- [x] Implementar `esp_zb_app_signal_handler` mínimo
- [x] Verificar `CONFIG_ZB_ENABLED=y` en sdkconfig
- [x] Verificar dependencias en `idf_component.yml`
- [x] Actualizar versión de `espressif__esp-zigbee-lib` a >= 1.6.8
- [x] Actualizar versión de `espressif__esp-zboss-lib` a >= 1.6.4
- [ ] Compilar y verificar que no hay errores de linking
- [ ] Probar funcionalidad Zigbee si aplica

## Notas Adicionales

### Símbolos NO Renombrados

Los siguientes símbolos siguen siendo válidos en ESP-IDF 5.5.x:
- `CONFIG_IEEE802154_RX_BUFFER_SIZE` - ✅ Existe
- `CONFIG_IEEE802154_ENABLED` - ✅ Existe
- `CONFIG_IEEE802154_RECEIVE_DONE_HANDLER` - ✅ Existe
- `CONFIG_ZB_ENABLED` - ✅ Existe
- Todos los símbolos `CONFIG_ZB_*` - ✅ Existen

### Extensión del Handler

Si necesitas agregar lógica específica de tu aplicación al handler:

1. Edita `main/core/zigbee_handler/zigbee_signal_handler.c`
2. Agrega tu código en los casos del switch correspondientes
3. Puedes llamar funciones de otros módulos desde el handler

Ejemplo:
```c
case ESP_ZB_BDB_SIGNAL_STEERING:
    if (err_status == ESP_OK) {
        ESP_LOGI(TAG, "Network steering successful");
        // Tu lógica personalizada
        my_app_on_zigbee_joined();
    }
    break;
```

## Referencias

- [ESP Zigbee SDK Documentation](https://docs.espressif.com/projects/esp-zigbee-sdk/en/latest/)
- [ESP-IDF 5.5 Release Notes](https://github.com/espressif/esp-idf/releases)
- [ESP-IDF Migration Guides](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/migration-guides/index.html)


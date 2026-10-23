# Cambios y añadidos de `refactor/system-overhaul`

Documento de referencia que explica **qué cambió**, **por qué** y **cómo funciona**
cada pieza del branch `refactor/system-overhaul` respecto a `main`.

- **Base:** `main` (`3eac1955`)
- **Rama:** `refactor/system-overhaul` (`a787b665`)
- **Alcance:** 123 commits, 404 ficheros, **+26 867 / −3 676** líneas
- **Plataforma:** ESP32-C6 (`esp32c6`), ESP-IDF v5.5.1
- **Fecha de redacción:** 2026-10-21
- **Documentos hermanos:** [`esp-sdr-integration.md`](./esp-sdr-integration.md) (el SDR
  en detalle técnico), [`rf-spectrum-advanced.md`](./rf-spectrum-advanced.md) (el
  analizador de espectro)

> **Cómo leer este documento.** La sección 3 indexa cada área por commits.
> Las secciones 4 a 20 explican una área cada una, con el "por qué" primero y
> el "cómo funciona" después, con diagramas y tablas. La sección 21 documenta
> los **dos cambios sin commitear** que hay en el árbol de trabajo. La sección
> 22 recoge las **roturas detectadas** al ejecutar la suite de tests: es la
> parte que hay que leer antes de integrar.
>
> **Actualización Fase 1 (2026-10-22):** `event_bus` se eliminó; WiFi y BLE son
> mutuamente excluyentes en `radio_lease`. Las secciones 5 y 6 describen el
> estado anterior y se conservan como histórico; el código actual está en
> `radio_lease.c`, `wifi_service.c` y los callbacks directos de cada app.
>
> **Actualización Fase 2 (en curso):** `minino_app_t` y `app_manager` son el
> contrato de ciclo de vida; `rf_spectrum` y `esp_sdr` ya salen por
> `app_manager_exit()` sin reiniciar. El resto del catálogo sigue usando
> `menus_module_restart()`.

---

## Índice

1. [Resumen ejecutivo](#1-resumen-ejecutivo)
2. [La idea de fondo: de "apps sueltas" a "un sistema con dueño de los recursos"](#2-la-idea-de-fondo-de-apps-sueltas-a-un-sistema-con-dueño-de-los-recursos)
3. [Mapa de commits por área](#3-mapa-de-commits-por-área)
4. [El SDR nativo: app, driver y banco de captura](#4-el-sdr-nativo)
5. [`radio_lease`: el dueño de la radio de 2,4 GHz](#5-radio_lease-el-dueño-de-la-radio-de-24-ghz)
6. [`event_bus`: bus de eventos tipado](#6-event_bus-bus-de-eventos-tipado)
7. [`gap_dispatcher` y `gatt_dispatcher`: el fan-out de Bluedroid](#7-gap_dispatcher-y-gatt_dispatcher-el-fan-out-de-bluedroid)
8. [`wifi_service`: un único `esp_wifi_init()`](#8-wifi_service-un-único-esp_wifi_init)
9. [Hardening del ciclo de vida de los módulos](#9-hardening-del-ciclo-de-vida-de-los-módulos)
10. [Privatización del estado y datos fuera de las cabeceras](#10-privatización-del-estado)
11. [Interfaz: catálogo de menús, vistas, iconos y OLED](#11-interfaz-catálogo-de-menús-vistas-iconos-y-oled)
12. [La app nueva: analizador de espectro RF](#12-la-app-nueva-analizador-de-espectro-rf)
13. [Botones: cola y tarea propia](#13-botones-cola-y-tarea-propia)
14. [microSD: montaje, slots y errores reales](#14-microsd-montaje-slots-y-errores-reales)
15. [BLE: HID, GATT, spam y sincronización GAP](#15-ble-hid-gatt-spam-y-sincronización-gap)
16. [Build system: variantes, Kconfig y el cap del banco](#16-build-system-variantes-kconfig-y-el-cap-del-banco)
17. [Tests de host y CI](#17-tests-de-host-y-ci)
18. [Tools de host](#18-tools-de-host)
19. [Cambios menores por módulo](#19-cambios-menores-por-módulo)
20. [Tabla completa de commits](#20-tabla-completa-de-commits)
21. [Cambios sin commitear en el árbol de trabajo](#21-cambios-sin-commitear)
22. [Roturas detectadas: estado real de la suite](#22-roturas-detectadas-estado-real-de-la-suite)
23. [Conectar el SDR con la web de ESPARGOS](#23-conectar-el-sdr-con-la-web-de-espargos)

---

## 1. Resumen ejecutivo

El branch hace cuatro cosas a la vez, y conviene separarlas porque tienen
motivos y riesgos distintos.

### 1.1 El SDR deja de ser un segundo arranque y pasa a ser una app nativa

Hasta `main`, `Applications > SDR` entregaba el arranque a una segunda
imagen en `ota_1` y el dispositivo dejaba de ser Minino. En este branch la
app es **una más del firmware**: `esp_sdr_module.c` abre el motor de captura
del C6 en proceso, captura bursts de I/Q y los guarda en la microSD, sin tocar
`ota_1` y sin apagar la pantalla.

El precio es que el motor de captura del C6 necesita el **bloque 2 de SRAM**
(`0x40840000`) como banco de captura, y el firmware entero tiene que caber por
debajo. Eso obligó a recortar 119 KB de la imagen (ver §4.5) y a añadir un
*linker script* que convierte el solapamiento en un error de enlazado en vez de
una corrupción en tiempo de ejecución.

### 1.2 La radio deja de ser un recurso global sin dueño

ESP-IDF guarda **un solo** callback de modo promiscuo y **un solo** slot de
callback GATTC. Seis subsistemas los competían: el último en registrantearse
rotaba el callback y los demás se quedaban mudos en silencio.

- `radio_lease` (§5) arbitra el modo promiscuo y el stack BLE con un recuento
  de referencias atómico.
- `gap_dispatcher` / `gatt_dispatcher` (§7) hacen de fan-out: un solo callback
  registrado en Bluedroid, N suscriptores dentro.
- `event_bus` (§6) da un bus de eventos tipado para lo que no sea GAP.
- `wifi_service` (§8) centraliza el `esp_wifi_init()`.

### 1.3 Los módulos dejan de dejar basura al salir

Una campaña de *hardening* (§9) recorre unos 30 módulos buscando el mismo
patrón: un `begin()` que crea tareas, colas, mutexes y Claims, y un `exit()`
que no los suelta. El síntoma típico era el mismo: la app salía, la radio se
quedaba encendida, el escaneo BLE moría, o el `esp_timer` se quedaba
suspendido y la UI se congelaba.

### 1.4 Se puede comprobar en el host

16 suites nuevas de tests de host (§17) con Unity: 86+ tests que compilan en
`gcc` contra el PC, con *mocks* de las cabeceras de ESP-IDF. Es lo que
convierte "esto lo he arreglado a ojo" en "esto lo he arreglado y hay un test
que lo dice".

### 1.5 Números del branch

| Métrica | Valor |
|---|---|
| Commits | 123 |
| Ficheros tocados | 404 (206 nuevos, 198 modificados, 0 borrados) |
| Líneas | +26 867 / −3 676 |
| Suites de test de host nuevas | 16 (15 en C + 1 en Python) |
| Componentes nuevos | `radio_lease`, `event_bus`, `wifi_service`, `gap_dispatcher` |
| Apps nuevas | `rf_spectrum` (analizador de espectro RF) |
| Opciones de Kconfig nuevas | `SDR_ENABLE`, `MININO_ZIGBEE_STACK`, `MININO_OPENTHREAD_STACK` |
| Comandos de consola nuevos | `sdr_capture`, `sdtest`, `display`, `press`, `exit_app`, `threshold`, `spectrum_*` (6) |
| Tests de host escritos | 278 (252 pasan, 4 fallan, 22 no compilan — ver §22) |
| Imagen DRAM (IRAM+data+bss) | 374 KB → 255 KB (presupuesto: 256 KB) |
| SRAM estático del bus de eventos | 21 248 B → 2 928 B (−17,9 KB) |
| Banco de captura reservado al heap | 128 KB → 64 KB (devuelve 113 KB de bloque contiguo al heap) |

---

## 2. La idea de fondo: de "apps sueltas" a "un sistema con dueño de los recursos"

Antes de este branch, el firmware era un conjunto de apps que llamaban a APIs
globales de ESP-IDF. El patrón que rompía era siempre el mismo:

```
app A llama esp_wifi_set_promiscuous(true)   ← toma el único callback
app B llama esp_wifi_set_promiscuous_rx_cb(cb_B)  ← roba el callback de A
app A termina y llama esp_wifi_set_promiscuous(false)  ← apaga la captura de B
```

Y lo mismo con el stack BLE:

```
app C llama esp_bluedroid_deinit()   ← porque su ventana de escaneo acabó
app D (que sólo estaba corriendo) pierde la radio a mitad de una captura
```

Este branch introduce **propietarios explícitos** para los recursos globales:

```
                    ┌──────────────────────────────┐
                    │        radio_lease            │
                    │  (dueño de la radio 2,4 GHz)  │
                    └──────────────────────────────┘
        ▲            ▲             ▲            ▲
        │            │             │            │
   ┌────┴───┐  ┌─────┴──┐   ┌──────┴───┐  ┌────┴─────┐
   │sniffer │  │deauth  │   │ spectrum │  │  SDR RX  │
   └────────┘  └────────┘   └──────────┘  └──────────┘
   (cada uno pide un "slot"; el driver sólo se toca al pasar de 0→1 y 1→0)

                    ┌──────────────────────────────┐
                    │      gap_dispatcher          │
                    │ (fan-out de los eventos GAP) │
                    └──────────────────────────────┘
        ▲            ▲             ▲            ▲
        │            │             │            │
   ┌────┴───┐  ┌─────┴──┐   ┌──────┴───┐  ┌────┴─────┐
   │bt_spam │  │HID     │   │bt_gattc  │  │ gattcmd  │
   └────────┘  └────────┘   └──────────┘  └──────────┘
```

Y para lo que no es GAP, un bus de eventos tipado (`event_bus`) y un servicio
único de Wi-Fi (`wifi_service`).

El resto del branch (hardening, privatización, tests) es la consecuencia: al
tener un dueño claro de cada recurso, "quién suelta qué" deja de ser
ambiguo, y los tests pueden comprobarlo.

---

## 3. Mapa de commits por área

| Área | Commits |
|---|---|
| **SDR nativo** (§4) | `1df8bc9e` `fe1e0223` `f4f780e1` `09e3a77d` `0411c22c` `0813ff1c` `bce64f44` `10a808ab` `9e8c715e` `42bf30dd` `50496e10` `ff9877b8` `bf66e068` `510f4195` |
| **Banco de captura y recorte de SRAM** (§4.5, §16.4) | `ba55fa02` `46d235a1` `7a52b727` `b41d7803` `cf580c36` `e26111bd` `a3e1809c` `d5deb48d` |
| **`radio_lease`** (§5) | `d56b9e7a` `6b313971` |
| **`event_bus`** (§6) | `92bc14f9` `cca3cc43` `5508c825` |
| **`gap_dispatcher` / `gatt_dispatcher`** (§7) | `3fbb6d9f` `574dfe5e` `029ee8c3` `2452a801` `de1e3dda` `37869cc6` |
| **`wifi_service`** (§8) | `c2f47398` `fdf0f955` `d84eb372` `3321c30f` `f9aff880` `cdd1b586` `52138a31` `ca0a422d` |
| **Hardening de módulos** (§9) | `857a17a9` `6758054b` `7d4b08f2` `a3bf17f2` `e0b79f30` `b0163862` `00a2b83d` `339f0dbd` `69427bb5` `31953a42` `c69437cc` `1f86aba6` `f5c2747f` `e73754f7` `1a763db1` `931260ad` `093f257b` `2d97d08b` `f3c3c248` `d270b3a9` `6a078453` `6c958e7e` `280368ca` `913e2d81` `7ce2761d` `ddf071ca` |
| **Privatización de estado** (§10) | `080d8b29` `010e3446` `e2165751` `3d95ad2b` `80d47bbf` |
| **Datos fuera de cabeceras** (§10.2) | `fd139212` `84879287` `d16af92c` `96901229` `56f58e1c` `f708fd5f` `7a9aaeff` |
| **Interfaz: menús y vistas** (§11) | `82927939` `8c485cb1` `7effae44` `f708fd5f` `05c33cbb` `689c925a` |
| **Analizador de espectro RF** (§12) | `f85b1a53` `50496e10` `0411c22c` |
| **Botones** (§13) | `181feee8` |
| **microSD** (§14) | `161643b6` `9cabe192` `c4f07def` `b8346169` |
| **BLE** (§15) | `18871f04` `449b0144` `280368ca` `de1e3dda` `029ee8c3` `574dfe5e` `2452a801` `3fbb6d9f` |
| **Build system** (§16) | `8d4173ce` `28cbc76d` `9c7599ee` `d861d64f` `ff3cd744` `c4eafa7f` `f67c001b` `045de824` `ad5c94ef` `7a9aaeff` |
| **Tests y CI** (§17) | `88d13fb7` `da81e333` `689c925a` `c69437cc` `2c177f0d` `c6a814c4` `045de824` `47feae79` |
| **Tools de host** (§18) | `d861d64f` `ff3cd744` |

---

## 4. El SDR nativo

### 4.1 Qué es

`Applications > SDR` abre una app del menú normal de Minino:

```
firmware/main/modules/esp_sdr/
├── c6_sdr_rx.c        216 líneas  driver del motor de captura (nivel de registros)
├── c6_sdr_rx.h         67         API del driver
├── esp_sdr_memory.c    57         reserva del banco de captura en el heap
├── esp_sdr_memory.h    41         base/tamaño del banco
├── esp_sdr_module.c   192         la app del menú
├── esp_sdr_module.h    31
├── osmosdr_serial.c   446         parser del protocolo esdr (osmoSDR)
├── osmosdr_serial.h   115
├── sdr_screen.c        53         la vista OLED
├── sdr_screen.h        30
├── esdr_proto.h       132         constantes y estructuras del protocolo
└── test/                         suite de host (20 tests)
```

> Nota: `iq_pack.c/.h` **no** están aquí. Viven en
> `firmware/components/vrt_packet/` (vendorizados del proyecto
> [ESPARGOS/esp-sdr](https://github.com/ESPARGOS/esp-sdr)) y **el firmware no
> los usa**: el camino de captura copia las palabras crudas tal cual.

### 4.2 El estado de la app

`esp_sdr_module.c:26-37` — un único struct estático, sin mutex ni tarea:

```c
typedef struct {
  bool     running;
  uint32_t freq_mhz;
  uint32_t burst_count;
  uint32_t last_us;
  uint32_t last_samples;
} sdr_native_state_t;
```

La máquina de estados la conducen los botones:

```
        esp_sdr_module_begin()
                 │
                 ├── monta la microSD (ANTES de tocar la radio)
                 ├── dibuja el panel de reposo
                 ├── menus_module_enable_input()
                 ├── menus_module_set_app_state(true, sdr_input_handler)
                 └── dispara el primer burst
                 │
   BUTTON_UP/DOWN │  BUTTON_RIGHT          │  BUTTON_LEFT
   sintonía ±5MHz │  captura + guardado     │  salir
        │         │                        │
   c6_sdr_rx_tune() sdr_burst_run()        │  menus_module_enable_input()
   2412..2472 MHz   │                     │  menus_module_set_app_state(false, NULL)
                     │                     │  c6_sdr_rx_stop()
                     │                     │  menus_module_return()
                     ▼                     ▼
              captura → microSD        radio devuelta a Minino
```

### 4.3 El driver de captura (`c6_sdr_rx.c`)

El motor de *dump* del C6 no escribe en un buffer cualquiera: **toma
posesión de un bloque de SRAM** y descarga ahí las muestras de I/Q a 80 MS/s.
El driver hace, en orden:

**a) Traer la radio** (`c6_sdr_rx_init()`, líneas 44-80)

```c
  if (!wifi_driver_get_initialized()) {
    if (!wifi_driver_init_sta()) {
      ESP_LOGE(TAG, "the Wi-Fi driver did not come up: stop the app that "
                   "holds the radio (BLE) and try again");
      return ESP_ERR_NO_MEM;
    }
  }
  radio_lease_wifi_acquire(RADIO_WIFI_MODE_STA);
  s_saved_sram_owner = REG_READ(SRAM_OWNER_REG);
```

El motor de captura **presta el PHY del Wi-Fi**: necesita sus relojes y su
calibración. Por eso el driver del Wi-Fi tiene que estar arriba. Y no se pide
modo promiscuo a propósito (comentario en `c6_sdr_rx.c:66-70`): el modo
promiscuo hace que el driver reserve sus buffers de captura — 37 KB de heap,
un tercio de lo que queda tras el banco.

**b) Sintonizar** (`c6_sdr_rx_tune()`, líneas 82-118)

```c
  if (wifi_driver_get_channel() != ch) {
    wifi_driver_set_channel(ch);
  }
  pbus_workmode();
  rom_pbus_xpd_tx_off();
  ram_pbus_xpd_rx_on(1);
  set_rxclk_en(1);
```

Dos detalles importantes:

- **No se mueve la radio si ya está en ese canal.** Preguntar al driver por su
  canal sigue reiniciando su radio, y los buffers de la radio son ~40 KB de
  heap (commit `50496e10`).
- La cadena de RX se enciende con `pbus_workmode` → `xpd_tx_off` →
  `xpd_rx_on(1)` → `set_rxclk_en(1)`, el mismo orden que el upstream
  (`esp-sdr-zephyr/lib/esp_sdr/src/esp_sdr_rx.c:78-98`).

**c) Capturar** (`c6_sdr_rx_capture()`, líneas 124-194)

```c
  /* canario: se rellena todo el buffer con un patrón conocido */
  for (uint32_t j = 0; j < num_samples; j++) s_iq_buf[j] = 0xa5a0055au;
  for (uint32_t j = 0; j < 4; j++)      s_iq_buf[num_samples + j] = 0x5a5aa5a5u ^ j;

  REG_WRITE(MODEM_CLK_REG1, 0xffffffffu);      /* forzar relojes a 80 MS/s */
  REG_WRITE(MODEM_CLK_REG2, 0x0007ffffu);
  REG_WRITE(MODEM_CLK_REG3, 0xffffffffu);
  REG_WRITE(DUMP_CTRL_REG, 0);
  REG_WRITE(DUMP_CFG_REG1, ... | (1u<<6) | (2u<<12) | (3u<<18));  /* I/Q lanes */
  REG_WRITE(DUMP_CFG_REG2, ... | (15u << 15));                      /* fuente = RX I/Q crudo */
  REG_CLR_BIT(DUMP_FE_REG, 1);

  /* ceder el banco de SRAM al motor */
  REG_WRITE(SRAM_OWNER_REG, (owner & ~0xf00u) | 0x400u);   /* bloque 2 */
  __asm__ volatile("fence rw,rw" ::: "memory");

  /* disparo: limpiar DONE → RUN → pulso TRIGGER → RUN */
  REG_WRITE(DUMP_CTRL_REG, num_samples | (1u << 18));
  REG_WRITE(DUMP_CTRL_REG, num_samples | (1u << 31));
  REG_WRITE(DUMP_CTRL_REG, num_samples | (1u << 31) | (1u << 19));
  REG_WRITE(DUMP_CTRL_REG, num_samples | (1u << 31));

  /* espera: hasta 25 ms */
  do { done = (REG_READ(DUMP_CTRL_REG) & (1u << 18)) != 0; }
  while (!done && esp_timer_get_time() < deadline);

  /* restaurar la propiedad del banco a la CPU */
  REG_WRITE(DUMP_CTRL_REG, 0);
  REG_WRITE(SRAM_OWNER_REG, owner);
```

**d) Verificar**

Dos comprobaciones, ambas con consecuencias distintas:

| Comprobación | Qué detecta | Log |
|---|---|---|
| `!done` tras 25 ms | el motor no terminó (reloj forzado incorrecto, banco ocupado) | `Capture timeout (deadline exceeded)` |
| `s_iq_buf[0]` y `[1]` siguen con el canario | el motor no escribió nada | `Capture incomplete (buffer unchanged)` |

Los 4 **palabras de guardia** (`0x5a5aa5a5u ^ j`) se escriben *más allá* del
número de muestras, en los índices 16380..16383 — las 4 últimas palabras de un
banco de 64 KiB. Por eso `C6_SDR_MAX_SAMPLES` es 16380 y no 16384.

| Parámetro | Valor | Dónde |
|---|---|---|
| Muestras mínimas | 256 | `c6_sdr_rx.c:131` |
| Muestras máximas | 16380 (65 520 B) | `c6_sdr_rx.h:19` |
| Timeout del disparo | 25 000 µs | `c6_sdr_rx.c:165` |
| Tasa de muestreo | 80 MS/s | fija, no configurable |
| Formato en disco | 32 bits crudos, I/Q entrelazadas | `esp_sdr_module.c:64-87` |

**e) Guardar en la microSD** (`esp_sdr_module.c:64-87`)

```c
  char filename[64];
  files_ops_incremental_name_buf("/sdcard", "sdrcap", ".iq", filename, sizeof(filename));
  FILE* file = fopen(filename, "wb");
  const uint32_t* words = c6_sdr_rx_get_buffer();
  for (uint32_t i = 0; i < s_sdr.last_samples; i++) {
    if (fwrite(&words[i], sizeof(uint32_t), 1, file) != 1) { fclose(file); remove(filename); return false; }
  }
```

El nombre lo genera `files_ops_incremental_name_buf()` con el formato
`"%s%02d"` (`files_ops.c:320`), así que los ficheros son
`/sdcard/sdrcap00.iq`, `sdrcap01.iq`, … (2 dígitos, hasta 9999).

> **Corrección respecto a `docs/esp-sdr-integration.md`:** ese documento dice
> `sdrcap_NNNN.iq` (4 dígitos). El código genera 2 dígitos.

### 4.4 El banco de captura

Es la pieza más importante del diseño. El motor de *dump* necesita el bloque 2
de SRAM:

```
0x40800000 ┌────────────────────────────────┐
           │  bloque 0 y 1: la imagen DRAM    │  ← el firmware tiene que caber aquí
           │  (IRAM + data + bss)             │
0x40840000 ├────────────────────────────────┤
           │                                │
           │  BANCO DE CAPTURA (128 KiB)    │  ← el motor escribe aquí
           │  el motor sólo toca los 64 KiB  │  ← se reservan 64 KiB (ver §4.5)
           │  primeros: 16380 palabras       │
0x40860000 └────────────────────────────────┘
```

Hay **dos** mecanismos que lo protegen:

**1. El *linker script*** (`firmware/main/esp_sdr_bank.ld`, 30 líneas)

```ld
MEMORY
{
  sram_seg (RWX) : org = 0x40800000, len = 0x40000
}

ASSERT(_heap_start <= 0x40840000, "SDR: the DRAM image overlaps the capture bank")
```

Redefine la región `sram_seg` (la de IDF en `memory.ld`) con la misma origen
y longitud 0x40000 = 256 KiB, de modo que toda sección colocada con
`> sram_seg` tiene que caber por debajo de `0x40840000`. Si no cabe, el
enlazado falla con:

```
region 'sram_seg' overflowed by N bytes
```

y si el heap empieza por encima del banco, con el `ASSERT` explícito:

```
SDR: the DRAM image overlaps the capture bank
```

El script se añade con `-T` **después** de los scripts de IDF, en
`firmware/main/CMakeLists.txt:87-92`:

```cmake
if(CONFIG_SDR_ENABLE)
    message(STATUS "SDR: capture bank cap ENABLED")
    idf_build_set_property(LINK_OPTIONS "-T" APPEND)
    idf_build_set_property(LINK_OPTIONS "${CMAKE_CURRENT_LIST_DIR}/esp_sdr_bank.ld" APPEND)
endif()
```

**2. La reserva del heap** (`esp_sdr_memory.c:43-49`)

```c
#if defined(CONFIG_SDR_ENABLE)
SOC_RESERVE_MEMORY_REGION((intptr_t)SDR_CAPTURE_BANK_BASE,
                          (intptr_t)(SDR_CAPTURE_BANK_BASE + SDR_CAPTURE_BANK_SIZE),
                          sdr_capture_bank);
#endif
```

`SOC_RESERVE_MEMORY_REGION()` mete la región en la sección
`.reserved_memory_address`, que es exactamente la que lee el *heap port* del
C6 (`components/heap/port/esp32c6/memory_layout.c`) para quitarla de los
bloques del heap. Sin esto, `heap_caps` repartía el banco y el motor lo
pisaba.

### 4.5 Por qué 64 KiB y no 128 KiB, y cómo se hizo caber el firmware

El banco entero son 128 KiB, pero el motor **sólo escribe los primeros
64 KiB**: una sonda que rellena el bloque con un patrón y cuenta cuántas
palabras cambian cuenta exactamente 16384 (la última es la 16383).

Reservar los 128 KiB enteros costaba el bloque contiguo más grande del heap:
113 KB. Y el controlador de Bluetooth del C6 pide ~121 KB contiguos, así que el
escaneo BLE moría con:

```
bta_dm_ble_scan start scan failed. status=0x3
HCI_ERR_MEM_CAPACITY_EXCEEDED
```

Reservando sólo los 64 KiB que el motor usa, el heap vuelve a tener un bloque
de 156 KB y el Bluetooth entra. Commit `9e8c715e` ("only the 64 KB the dump
engine writes are taken off the heap").

En la placa:

```
heap_init: At 4083EE30 len 000011D0 (4 KiB): RAM     <- hasta el banco
heap_init: At 40850000 len 0002C610 (177 KiB): RAM   <- desde el banco
heap_init: At 4087C610 len 00002F54 (11 KiB): RAM
```

El tramo `0x40840000..0x40850000` no aparece: está reservado.

**El presupuesto de la imagen.** Con la imagen completa, el `.bss` ocupaba
172 KiB y el bloque 2 sólo tiene 128 KiB: no cabía. El recorte:

| Recorte | KB |
|---|---|
| Código caliente del Wi-Fi y la PHY fuera de IRAM | 45,6 |
| Los spoofers de OpenDroneID al heap (uno por dron configurado) | 37,7 |
| `librftest.a` fuera del enlace (el cert-test no hace falta sin TX) | 44,7 |
| Tabla de suscriptores del bus de eventos (16 → 4 por tema) | 14,6 |
| Buffers del log de vigilancia, del árbol de ficheros y de la lista de SSID | 4,5 |
| Detector de vigilancia (200 → 80 dispositivos, 50 → 20 trazas) | 7,0 |
| Dron BLE de OpenDroneID al heap | 2,3 |
| Fuente del OLED a flash | 1,0 |
| **Total** | **161,4** |

Resultado: de 374 KB a 255 KB de IRAM+data+bss, contra un presupuesto de
256 KB. Muy justo, pero cabe.

### 4.6 El SDR es sólo de recepción

El C6 **no puede transmitir I/Q**: su motor de *dump* es un receptor (escribe
en SRAM las muestras que la radio demoduló) y no tiene DAC de TX (el S3 sí).
Lo único que el chip emite es el modo tono del *cert-test*,
`esp_phy_wifi_tx_tone()`, que vive en `librftest.a`.

Se llegó a enlazar ese tono de dos formas (el archivo entero, o sólo
`rf_test.o` + `esp_phy_api.o` con un *wrapper* propio), pero **no radia**: la
medida con un HackRF da exactamente el mismo espectro con el tono encendido y
apagado. La causa es que el tono del *cert-test* necesita el estado completo
de *cert test* del PHY, que prepara `rftest_init()` en `wifi.o`; ese objeto
arrastra `mac_common.o` con 45 KiB de `.bss` (los buffers de IQ del rxtest),
que no caben bajo el banco.

Así que el comando `rf_tx` **se quitó** de la consola (commit `bce64f44`), junto
con `c6_sdr_tx.c`, `phy_tone_compat.c` y el enlace del archivo de cert-test
(`10a808ab`). El modo TX del protocolo osmoSDR se rechaza con `-ENOTSUP`
(`osmosdr_serial.c:132-134`) en vez de responder con silencio.

### 4.7 Los comandos de consola

| Comando | Dónde | Qué hace |
|---|---|---|
| `sdr_capture` | `cmd_control.c:276-287` | Abre la app nativa. Ruta idéntica al botón: `menus_module_set_menu_over_cmd("sdr")`, porque el menú es el dueño del *callback* de salida — si la app se lanzara sola, se quedaría corriendo para siempre. |
| `sdtest` | `sd_card.c:110-117` | Monta la microSD y dice qué pasó. §14. |
| `rf_tx` | *eliminado* | `bce64f44` |

`boot_sdr`, que entregaba el arranque a `ota_1`, se sustituyó por
`sdr_capture` en `f4f780e1`.

### 4.8 El protocolo osmoSDR (`osmosdr_serial.c`)

Implementación del protocolo binario estilo HackRF. **No lo usa el firmware**
(sólo el test de host y como referencia), pero documenta el contrato:

| Petición | Valor | Comportamiento |
|---|---|---|
| `ESDR_REQ_SET_MODE` | 0x02 | Acepta OFF/RX/TEST_IN/TEST_OUT. `ESDR_MODE_TX` → `-ENOTSUP`. |
| `ESDR_REQ_SET_FREQ` | 0x03 | 8 bytes LE en Hz; rango 100–6000 MHz. |
| `ESDR_REQ_SET_SAMPLE_RATE` | 0x04 | 4 bytes LE; 0 → `-EINVAL`; se redondea a `{250000, 125000, 62500, 31250, 15625}`. |
| `ESDR_REQ_SET_FORMAT` | 0x08 | Sólo 8 o 16 bits (el 12 no se acepta). |
| `ESDR_REQ_MININO` | 0x7e | Dispara el *escape* `MININO` (6 bytes ASCII) para volver a Minino. |

El *escape* se detecta byte a byte con reinicio en coincidencia parcial, antes
del parseo de cabeceras, y el *resync* del magic `ESDR_SER_MAGIC` (4 bytes)
se hace con un `memmove` deslizante.

### 4.9 El empaquetado de I/Q (`vrt_packet/iq_pack.c`)

Tres formatos, con su *cuanto*:

| Bits | Disposición | Cuanto |
|---|---|---|
| 16 | 1 muestra por palabra: `I` en 31:16, `Q` en 15:0 | 1 muestra |
| 8 | 4 items por palabra: `I0<<24 \| Q0<<16 \| I1<<8 \| Q1` | 2 muestras |
| 12 | 4 muestras = 8 items = 96 bits = **3 palabras**, bit a bit de MSB a LSB | 4 muestras |

El escalado usa `shift = floor(log2(full_scale)) - (bits-1)` con redondeo al
medio y saturación a `[-lim, lim-1]`. `iq_pack_raw` usa
`full_scale = 1<<(bits-1)` → `shift = 0` → **sin pérdida**.

---

## 5. `radio_lease`: el dueño de la radio de 2,4 GHz

### 5.1 Los dos problemas

Del propio `radio_lease.h:1-23`:

> **Shared ownership of the single 2.4 GHz radio.**
>
> Two independent locks, both reference counted:
> - **Promiscuous mode.** ESP-IDF keeps one promiscuous receive callback and
>   one on/off switch. Six subsystems need it. They used to call
>   `esp_wifi_set_promiscuous_rx_cb()`/`esp_wifi_set_promiscuous()` directly,
>   so the last one to start stole the callback from the others and a window
>   ending with `esp_wifi_set_promiscuous(false)` killed a capture that
>   somebody else was relying on. Now each consumer claims a slot and the
>   driver is only touched on the 0→1 and 1→0 transitions; frames are fanned
>   out to every holder.
> - **The WiFi driver and the BLE stack.** The surveillance detector rotates
>   between a WiFi window and a BLE window and used to
>   `esp_wifi_deinit()`/`esp_bluedroid_deinit()` in between. Those are global:
>   any app that merely happened to be running — the analyzer's sniffer, the
>   deauth detector, BLE spam, BLE HID, the trackers scanner — lost the radio
>   mid-operation. They now hold a lease, and the detector only takes the
>   driver down when it is the last holder.

### 5.2 La API

```c
typedef enum {
  RADIO_PROM_NONE = 0,
  RADIO_PROM_SURVEILLANCE,
  RADIO_PROM_SNIFFER,
  RADIO_PROM_DEAUTH_DETECTOR,
  RADIO_PROM_DRONEID_SCANNER,
  RADIO_PROM_SPECTRUM,
  RADIO_PROM_SDR_RX,
  RADIO_PROM_USER_COUNT
} radio_prom_user_t;
```

| Función | Devuelve | Semántica |
|---|---|---|
| `radio_lease_prom_acquire(user, cb)` | `ESP_OK` / `ESP_ERR_INVALID_ARG` / error del driver | Reclama un slot. **El driver sólo se toca al pasar de 0→1.** |
| `radio_lease_prom_hold(user)` | idem | Reclama promiscuo **sin** receptor (lo usa el SDR). |
| `radio_lease_prom_set_cb(user, cb)` | `ESP_OK` / `ESP_ERR_NOT_FOUND` | Cambia el receptor de un tenedor existente. |
| `radio_lease_prom_release(user)` | `ESP_OK` / `ESP_ERR_NOT_FOUND` | **El driver sólo se apaga al pasar de 1→0.** |
| `radio_lease_prom_count()` / `_held(user)` | | Estado. |
| `radio_lease_prom_set_filter(filter)` | `ESP_OK` | **No** fuerza el modo promiscuo (cambio de comportamiento). |
| `radio_lease_wifi_acquire(mode)` | `ESP_OK` / `ESP_ERR_NOT_SUPPORTED` / error | `STA` / `NULL` / `APSTA`. Inicializa el driver sólo al pasar de 0→1. **Rechaza si BLE tiene tenedores.** |
| `radio_lease_wifi_release()` | — | Desinicializa sólo cuando el último claim se va. |
| `radio_lease_ble_acquire()` | `ESP_OK` / `ESP_ERR_NOT_SUPPORTED` / error | Levanta controlador + Bluedroid al pasar de 0→1. **Rechaza si WiFi tiene tenedores**; sólo libera el driver si no hay ninguno (~65 KB). |
| `radio_lease_ble_release()` | — | Baja Bluedroid + controlador sólo al pasar de 1→0. Rechaza liberaciones huérfanas. |
| `radio_lease_ble_block_scan()` / `_unblock_scan()` / `_scan_allowed()` | | Puerta de escaneo (§5.6). |

### 5.3 El registro: C puro, testeable en el host

`radio_lease_registry.h` no depende de ESP-IDF:

```c
#define RADIO_LEASE_MAX_USERS 8

typedef struct {
  radio_lease_rx_fn fn[RADIO_LEASE_MAX_USERS];
  void* ctx[RADIO_LEASE_MAX_USERS];
  bool active[RADIO_LEASE_MAX_USERS];
  uint8_t count;
} radio_lease_registry_t;
```

**El id del consumidor ES el slot.** Reclamar dos veces no infla el contador:
el sniffer de Wi-Fi se re-registra en cada arranque y debe seguir siendo un
solo tenedor.

**El reparto de tramas** (`radio_lease_registry.c:85-114`) copia los
suscriptores **bajo un lock** y los llama **fuera**:

```c
  portENTER_CRITICAL(&s_mux);
  for (...) { if (reg->active[i] && reg->fn[i]) { fn[n] = reg->fn[i]; ctx[n] = reg->ctx[i]; n++; } }
  portEXIT_CRITICAL(&s_mux);

  for (uint8_t i = 0; i < n; i++) fn[i](ctx[i], buf, type);
```

El motivo está en el propio código: el analizador se para a sí mismo desde
dentro de su propio manejador cuando llega a su límite de paquetes; si el
*dispatch* sujetsara el lock, se bloqueara.

### 5.4 El recuento BLE atómico

El refcount y la puerta de escaneo comparten **un** spinlock
(`s_ble_mux`):

```c
static uint8_t s_ble_refs = 0;
static portMUX_TYPE s_ble_mux = portMUX_INITIALIZER_UNLOCKED;
static uint8_t s_ble_scan_block = 0;
```

`radio_lease.c:240-270` explica por qué hace falta la sección crítica:

> The refcount is touched from the button state machine, the menus' on-exit
> callbacks, console commands and the BTC task, so every read-modify-write
> pair needs a critical section. Without it two racing stop/start paths could
> both see `before != 0` and skip the bring-up, or both drive the count to zero
> and tear the stack down under each other.

### 5.5 El enganche con los *dispatchers*

```
radio_lease_ble_acquire()  →  gap_dispatcher_init()
radio_lease_ble_release()  →  gap_dispatcher_reset()  +  gatt_dispatcher_reset()
```

`radio_lease.h:128-134` deja claro el reparto de responsabilidades: la
pila se levanta aquí, pero **el callback GAP no**: eso es del
`gap_dispatcher`, uno por suscriptor.

### 5.6 La puerta de escaneo

El controlador no puede escanear y publicitar (o fijar una dirección
aleatoria) a la vez: *"Advertising or scanning now, can't set randaddress"*.
Mientras un publicador tiene la puerta cerrada, **todos** los escáneres del
árbol la consultan antes de (re)armar:

- `bt_gattc.c:382` (antes de iniciar un escaneo) y `:442` (al re-armar tras
  `ESP_GAP_SEARCH_INQ_CMPL_EVT`)
- `ble_scann.c:71`
- `gattcmd_scan.c:97`, `gattcmd_enum.c:313`, `gattcmd_recon.c:359`,
  `gattcmd_write.c:223`

### 5.7 Quién usa qué

| Consumidor | Slot |
|---|---|
| `surv_radio.c:288/318` | `RADIO_PROM_SURVEILLANCE` |
| `cmd_sniffer.c:484/387` | `RADIO_PROM_SNIFFER` |
| `detector.c:118/152` | `RADIO_PROM_DEAUTH_DETECTOR` |
| `droneid_scanner.c:232/283` | `RADIO_PROM_DRONEID_SCANNER` |
| `rf_spectrum.c:361/392` | `RADIO_PROM_SPECTRUM` |
| `c6_sdr_rx.c:71/210` | **claim de Wi-Fi** (no promiscuo) |

---

## 6. `event_bus`: bus de eventos tipado

> **Eliminado en Fase 1.** El bus no tenía suscriptores reales y la macro de
> `wifi_service` registraba callbacks `NULL`. Las apps usan callbacks directos
> y `esp_event`; esta sección queda como histórico.

### 6.1 Qué es y qué no es

Un bus de eventos con **tabla de suscriptores por tema** y reparto (*fan-out*).
Tipos:

```c
typedef enum {
  EV_INPUT,
  EV_MENU,
  EV_WIFI,
  EV_GPS,
  EV_APP_START = 128,   /* espacio para apps privadas */
} event_topic_t;

#define EVENT_MAX_SUBS 2
```

### 6.2 La API

| Función | Devuelve | Semántica |
|---|---|---|
| `event_bus_init()` | `true` | Idempotente. Construye el mapa tema→slot **una sola vez**. |
| `event_bus_deinit()` | — | Vacía la tabla y el mapa. |
| `event_bus_subscribe(topic, cb, data)` | `bool` | Rechaza `topic >= EV_APP_START`, el slot inválido, el par `(cb, data)` duplicado y la tabla llena. |
| `event_bus_unsubscribe(topic, cb)` | `bool` | Quita por intercambio con el último. |
| `event_bus_dispatch(topic, data)` | — | Reparte a los suscriptores del tema. |

Macros de conveniencia: `event_bus_post()`, `event_bus_post_input()`,
`event_bus_post_menu()`, `event_bus_post_wifi()`, `event_bus_post_gps()`.

### 6.3 El reparto

```c
void event_bus_dispatch(event_topic_t topic, void* data) {
  (void) data;
  if (topic < 0 || topic >= EV_APP_START) return;

  event_subscriber_t subs[EVENT_MAX_SUBS];
  /* Copy under the lock, then call outside it: a subscriber that subscribes or
     dispatches from inside its own callback would otherwise deadlock on a
     non-recursive spinlock. */
  uint8_t n = _get_subs(topic, subs);
  for (uint8_t i = 0; i < n; i++) {
    if (subs[i].cb) subs[i].cb(topic, subs[i].data);
  }
}
```

Dos decisiones que conviene highlightar:

1. **El `data` del `dispatch` se descarta.** Cada suscriptor recibe **su
   propio** puntero `data` registrado en el `subscribe`. El `dispatch` es
   únicamente una señal.
2. **Se copia bajo el lock y se llama fuera**, para que un suscriptor pueda
   suscribirse o publicar desde dentro de su propio callback sin bloquearse
   en un spinlock no recursivo.

### 6.4 El mapa tema→slot

```c
static uint8_t _topic_slot(event_topic_t topic) {
  if (topic < 0 || topic >= EV_APP_START) return EVENT_MAX_SUBS;  /* centinela */
  uint8_t idx = s_topic_index[topic];
  return (idx < MAX_TOPICS) ? idx : EVENT_MAX_SUBS;
}
```

El mapa se construye **una sola vez** en `event_bus_init()`. El comentario
(`event_bus.c:59-64`) explica por qué no se puede reconstruir: renumerar los
slots dejaría a un suscriptor existente en una tabla de la que ya nada
dispensa.

> **Defecto activo.** La centinela es `EVENT_MAX_SUBS`, y con
> `EVENT_MAX_SUBS == 2` el valor 2 es un índice de slot **válido**.
> `event_bus_init()` asigna `EV_INPUT→0, EV_MENU→1, EV_WIFI→2, EV_GPS→3`, así
> que `EV_WIFI` y `EV_GPS` son indistinguibles de la centinela y **se
> rechazan en silencio**. Ver §22.1.

### 6.5 El coste en SRAM y por qué se recortó

`sizeof(event_subscriber_t)` es 8 bytes en el ABI de 32 bits del C6.

| Commit | `EVENT_MAX_SUBS` | `MAX_TOPICS` | `s_subs[]` | Total `.bss` | Δ |
|---|---|---|---|---|---|
| `92bc14f9` (original) | 16 | 160 | 21 120 B | 21 248 B | — |
| `ba55fa02` | 8 | 160 | 10 880 B | 11 008 B | −10 240 B (10 KB) |
| `46d235a1` | 4 | 140 | 5 040 B | 5 168 B | −5 840 B |
| `cca3cc43` | **2** | 140 | 2 800 B | **2 928 B** | −2 240 B |

Acumulado: **17,9 KB de SRAM estático liberados**, que hicieron falta para que
la imagen cupiera bajo el banco de captura.

### 6.6 Dónde se usa (y dónde no)

| Publica | Archivo |
|---|---|
| `EV_WIFI` | `wifi_service.c:42`, `wifi_app.c:51,74,87`, `OTA/modules/wifi_ap/wifi_ap.c:177`, `captive_module.c:545,711` |
| `EV_INPUT` / `EV_MENU` / `EV_GPS` | nadie |

| Suscribe | Archivo |
|---|---|
| `wifi_service_on_state_change(cb, data)` | `wifi_ap_manager.c:86`, `wifi_scanner.c:28`, `captive_module.c:238,244`, `detector.c:95`, `droneid_scanner.c:212`, `modbus_dos.c:83` |

> **Todos los `subscribe` pasan `cb = NULL`.** El bus acepta un callback nulo
> y lo guarda; el `dispatch` lo salta con `if (subs[i].cb)`. Son
> *placeholders* ("notificación opcional"), no subscriptions reales.

> **Dos commits están mal nombrados.** `049500e5` ("subscribe sniffer screens
> to event bus") sólo añade un `#define THREAD_SNIFFER_TOPIC
> (EV_APP_START + 2)` que no se usa — y que el bus rechazaría por estar
> `>= EV_APP_START`. El fan-out real del *sniffer* es una tabla privada de 4
> slots en `thread_sniffer.c:51`. `a7457dd0` ("align animation task with
> event bus") no toca el bus en absoluto: sólo cambia el apagado de la tarea
> de animación.

### 6.7 El bus no puede llevar eventos GAP

Esta es la lección que motivó los *dispatchers* y está documentada en
`test_event_bus.c:1-21`:

> event_bus was introduced as a replacement for direct GAP callback
> registration in the gattcmd console handlers. That substitution was wrong
> and silent … the subscription was refused, the return value was discarded,
> and `ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT` was never handled, so no scan
> ever started.

Los eventos GAP tienen que ir por el `gap_dispatcher` (§7).

---

## 7. `gap_dispatcher` y `gatt_dispatcher`: el fan-out de Bluedroid

### 7.1 El problema

Bluedroid guarda **un solo** callback GAP (`esp_ble_gap_register_callback`) y
**un solo** slot GATTC (`esp_ble_gattc_register_callback`). Si varios módulos
llaman a esas APIs, cada llamada **sobrescribe** la anterior y los módulos
anteriores se quedan sin eventos en silencio. Es la causa clásica de
"BLE scanning/ADV dejó de funcionar".

### 7.2 `gap_dispatcher`

```c
#define GAP_SUBSCRIBER_MAX 16

typedef struct {
  esp_gap_ble_cb_t cb;
  bool used;
} gap_subscriber_t;
```

| Función | Semántica |
|---|---|
| `gap_dispatcher_init()` | Registra el callback interno **sólo al entrar en ENABLED**. |
| `gap_dispatcher_register(cb)` | `ESP_ERR_INVALID_ARG` (NULL), `ESP_ERR_NO_MEM` (llena), `ESP_OK`. Duplicados ignorados. |
| `gap_dispatcher_unregister(cb)` | `ESP_ERR_NOT_FOUND` si no estaba. |
| `gap_dispatcher_reset()` | **El gancho de reset de pila**: olvida que está instalado, para que el siguiente `register()` lo reinstale en una pila recién habilitada. |

La lógica de transición (`gap_dispatcher.c:63-73`):

```c
  esp_bluedroid_status_t status = esp_bluedroid_get_status();

  portENTER_CRITICAL(&s_dispatcher_mux);
  bool just_enabled = (status == ESP_BLUEDROID_STATUS_ENABLED) &&
                      (s_last_seen_status != ESP_BLUEDROID_STATUS_ENABLED);
  s_last_seen_status = status;
  portEXIT_CRITICAL(&s_dispatcher_mux);

  if (!just_enabled) return ESP_OK;
```

Antes de `3fbb6d9f` se re-registraba en **cada** `init()`, lo que costaba un
ida y vuelta al BTC task por suscriptor.

### 7.3 `gatt_dispatcher` (nuevo en `3fbb6d9f`)

El mismo patrón para el lado cliente:

```c
#define GATT_SUBSCRIBER_MAX 8

static gatt_subscriber_t s_subscribers[GATT_SUBSCRIBER_MAX];
static bool s_installed = false;
```

El reparto reenvía además el `gattc_if`, de modo que cada manejador puede
ignorar los eventos dirigidos a la interfaz de otro módulo:

```c
  for (uint8_t i = 0; i < count; i++) local_cbs[i](event, gattc_if, param);
```

### 7.4 Los ids de aplicación GATTC

`esp_ble_gattc_app_register()` **rechaza duplicados**. Este header
(`gattcmd_service.h:14-24`) lo explica:

> Unique Bluedroid GATTC app_ids. `esp_ble_gattc_app_register()` rejects
> duplicates, and two services sharing app_id 0 (this file used to define it
> and `bt_gattc.c` uses the same value for DEVICE_PROFILE) collided: the second
> registration failed outright or confused the single global gattc_if → app
> demux. bt_gattc owns 0, the gatt console services take 2...5.

```c
#define GATTCMD_ENUM_APP_ID  2
#define GATTCMD_SCAN_APP_ID  3
#define GATTCMD_RECON_APP_ID 4
#define GATTCMD_WRITE_APP_ID 5
```

Antes de `574dfe5e` los cuatro servicios de gattcmd usaban el app_id 0.

### 7.5 Quién se registra

| Módulo | GAP | GATT |
|---|---|---|
| `ble_hidd_main.c` | `:373` | — |
| `bt_gattc.c` | `:539` | `:550` |
| `bt_gatts.c` | `:547` | — |
| `bt_spam.c` | `:982` | — |
| `surv_radio.c` | `:394` | — |
| `gattcmd_scan.c` | `:213` | `:222` |
| `gattcmd_enum.c` | `:428` | `:438` |
| `gattcmd_recon.c` | `:568` | `:577` |
| `gattcmd_write.c` | `:350` | `:359` |

### 7.6 El coste

| Estructura | Tamaño |
|---|---|
| `gap_subscriber_t s_subscribers[16]` | 128 B |
| `gatt_subscriber_t s_subscribers[8]` | 64 B |
| `radio_lease_registry_t s_prom` | 76 B |

---

## 8. `wifi_service`: un único `esp_wifi_init()`

### 8.1 El problema

Había **13 llamadas** a `esp_wifi_init()` repartidas por el árbol. Cada
módulo inicializaba el driver por su cuenta, con su propia configuración
(`wifi_slim_init_config()` en el controlador, la de por defecto en otros),
produciendo reinicios contradictorios.

### 8.2 La API

```c
bool wifi_service_init(void);
bool wifi_service_deinit(void);

wifi_mode_t         wifi_service_mode_get(void);
wifi_interface_t    wifi_service_iface_get(void);
bool                wifi_service_is_up_get(void);
bool                wifi_service_is_sta_get(void);
bool                wifi_service_is_ap_get(void);
wifi_second_chan_t  wifi_service_primary_chan_get(void);
wifi_second_chan_t  wifi_service_secondary_chan_get(void);

#define wifi_service_on_state_change(cb, data) \
  event_bus_subscribe(EV_WIFI, (event_callback_t)(cb), (void*)(data))
```

### 8.3 Cómo decide el modo

```c
#if CONFIG_STA_ENABLE
  s_state.mode = WIFI_MODE_STA;   s_state.is_sta = true;
#elif CONFIG_AP_ENABLE
  s_state.mode = WIFI_MODE_AP;    s_state.is_ap = true;
#else
  s_state.mode = WIFI_MODE_STA;   s_state.is_sta = true;   /* fallback */
#endif
```

> **Nota.** `CONFIG_STA_ENABLE` y `CONFIG_AP_ENABLE` **no están definidos en
> ninguna parte** (tampoco son símbolos de ESP-IDF). Las macros no definidas
> valen 0 en `#if`, así que **siempre se ejecuta la rama de reserva**: el
> servicio elige siempre `WIFI_MODE_STA`. Los modos `NULL` y `APSTA` llegan
> por la *lease* (`wifi_driver_init_null()` / `wifi_driver_init_apsta()`).

### 8.4 Los buffers del driver y el NVS

```c
  cfg.static_rx_buf_num = 2;
  cfg.dynamic_rx_buf_num = 4;
  cfg.dynamic_tx_buf_num = 4;
  cfg.nvs_enable = false;
```

Los buffers se recortan porque el banco de captura se lleva 64–128 KB de
SRAM: cada buffer del driver es heap que el resto del sistema no puede usar.

`nvs_enable = false` tiene un motivo concreto (comentario en
`wifi_service.c:75-80`):

> The driver keeps a cookie with the calibration data in the NVS partition, and
> it opens it with the partition already open the second time round (the
> Bluetooth brings the driver down and up again): the second
> `esp_wifi_init()` then fails with `init nvs: failed, ret=101` and the Wi-Fi
> apps do not start. Without the cookie the driver recalibrates on every boot,
> which is a few hundred milliseconds.

### 8.5 El patrón de migración

Seis commits con el título *"defer wifi init to wifi_service"* sustituyen
`esp_wifi_init()` por una consulta al servicio:

```c
  /* WiFi stack ya inicializada por wifi_service_init() en app_main(). */
  if (!wifi_service_is_up_get()) {
    ESP_LOGI(TAG_WIFI_DRIVER, "WiFi stack no inicializada por servicio; inicializando...");
    wifi_service_init();
  } else {
    ESP_LOGI(TAG_WIFI_DRIVER, "WiFi stack ya inicializada por servicio; saltando esp_wifi_init");
  }
```

Migrados: `wifi_controller.c`, `wifi_app.c`, `wifi_ap_manager.c`,
`wifi_scanner.c`, `OTA/modules/wifi_ap/wifi_ap.c`, `captive_module.c`,
`modbus_dos.c`, `deauth/detector.c`, `droneid_scanner.c`.

> Dos de los seis commits están mal nombrados: `cdd1b586` y `52138a31` no
> tocan `wifi_service` en absoluto.

### 8.6 El arranque

`firmware/main/main.c:55` llama `event_bus_init()` como **primera** cosa de
`app_main()`:

```c
  /* Must run before anything dispatches: the topic -> slot map is what keeps
     subscribers of different topics in separate tables. */
  event_bus_init();
```

---

## 9. Hardening del ciclo de vida de los módulos

### 9.1 Los 12 patrones

"Hardening" en este branch no es una cosa, son doce patrones mecánicos
aplicados módulo a módulo:

| # | Patrón | Ejemplo canónico |
|---|---|---|
| 1 | Se comprueba el retorno de `xTaskCreate` y se protege el handle antes de `vTaskSuspend`/`vTaskResume` | `warbee_module.c:370-384` |
| 2 | Carrera de auto-borrado: `volatile bool running` + publicar/limpiar el handle bajo sección crítica | `bt_spam.c:1053-1069` |
| 3 | Simetría de salida: lo que `begin()` crea, `exit()` lo suelta | `warthread_module.c:459-475` |
| 4 | Guardas NULL sobre ctx/handle | `modbus_tcp_scenes.c:100-110` |
| 5 | Comprobación de límites / saturación de índices | `modbus_tcp_prefs.c:26-45` |
| 6 | Saturación de divisiones | `memory_monitor.c:29-33` |
| 7 | Tamaño de pila y eliminación de VLA | `ssid_spam.c:187-191` |
| 8 | Privatización del estado: `static` + accesores | `radio_selector.c:6-9` |
| 9 | Datos fuera de las cabeceras | `wifi_bitmaps.c/.h` |
| 10 | Arbitraje de recursos globales | `radio_lease`, `event_bus`, `wifi_service` |
| 11 | Sacar el trabajo bloqueante de la tarea `esp_timer` | `keyboard_module.c:37-124` |
| 12 | Tests de host para cada invariante | 16 suites nuevas |

### 9.2 `vTaskSuspend(NULL)`: el congelamiento de la UI

Este es el fallo más repetido. `vTaskSuspend(NULL)` **suspende la tarea
llamante**, no la tarea cuyo handle es `NULL`. Y la tarea llamante solía ser
la de `esp_timer`, que ejecuta los callbacks de los botones:

```c
/* antes */
xTaskCreate(wardriving_screens_zigbee_animation_task,
            "scanning_wifi_animation_task", 4096, NULL, 5,
            &scanning_zigbee_animation_task_handle);
vTaskSuspend(scanning_zigbee_animation_task_handle);   /* NULL => suspende al que llama */
```

```c
/* después — warbee_module.c:374-384 */
/* The handle must be checked before suspending it: vTaskSuspend(NULL)
   suspends the *calling* task, which here is the esp_timer task that runs
   the button callbacks - the whole UI would freeze with no way out. */
if (xTaskCreate(..., &scanning_zigbee_animation_task_handle) == pdPASS) {
  vTaskSuspend(scanning_zigbee_animation_task_handle);
} else {
  ESP_LOGE("Warbee", "could not create the animation task");
  scanning_zigbee_animation_task_handle = NULL;
}
```

Afecta a: `war_bee`, `war_thread`, `gps/wardriving`.

### 9.3 `warthread`: la radio se quedaba muda

El cambio más profundo de la campaña. El manejador de tramas de OpenThread
hacía la escritura a la SD y el redibujado completo del OLED **dentro del
lock de OpenThread**:

```c
/* antes */
static void warthread_packet_handler(const otRadioFrame* aFrame, ...) {
  ...
  sd_card_write_file(csv_file_name, csv_file_buffer);   /* fopen/fwrite/fclose */
  oled_screen_display_show();                          /* redibujado completo */
}
```

`openthread_enable_promiscuous_mode()` entrega las tramas desde
`otPlatRadioReceiveDone()` **en la tarea del mainloop de OT, con
`esp_openthread_lock` tomado**. Escribir a la SD (un `fopen`/`fwrite`/
`fclose` por trama) y redibujar la pantalla ahí stalled el hilo que atiende
**toda** llamada de OpenThread, llenó la cola de RX de 20 entradas del driver
y dejó los buffers de recepción de `esp_ieee802154` clavados en
`process == true`: **la radio quedaba permanentemente muda tras 20 tramas**.

```c
/* después — warthread_module.c:194-238 */
/*
 * Runs outside the OpenThread lock.
 * ...
 * So the callback only copies the PSDU and a worker does the rest.
 */
static void warthread_packet_handler(const otRadioFrame* aFrame, bool aIsTx, void* aContext) {
  if (aFrame == NULL || aFrame->mLength < 16) return;
  if (warthread_frame_queue == NULL || gps_ctx == NULL || gps_ctx->sats_in_use == 0) return;
  warthread_frame_t item;
  item.len = (aFrame->mLength > WARTHREAD_PSDU_MAX) ? WARTHREAD_PSDU_MAX : aFrame->mLength;
  memcpy(item.psdu, aFrame->mPsdu, item.len);
  /* Zero timeout: never stall the OT task, drop the frame if the worker is behind. */
  xQueueSendToBack(warthread_frame_queue, &item, 0);
}
```

Con ello: una cola de tramas + una tarea obrera, la comprobación de límites
de lectura del PSDU (`WARTHREAD_BASE_FRAME_LEN 16`), y el desmontaje de la
cola y la tarea en `warthread_module_exit()`.

### 9.4 `screen_saver`: el rectángulo negro

`screen_saver_stop()` sólo ponía un flag. Pero la tarea de la animación
termina la iteración en la que está (`display_bitmap()` + `display_show()`)
antes de volver a mirar el flag:

```c
/* después — screen_saver.c:150-181 */
/* Clear the flag first so the task leaves its loop, then wait for it to be
   gone via the exit semaphore. Clearing the flag alone is not enough: ...
   That is exactly what happened once the button callbacks moved off the
   esp_timer task (priority 22) into a dedicated one (priority 4): the
   animation at priority 5 preempted the menu render. */
screen_saver_running = false;
if (screen_saver_task_handle != NULL) {
  if (screen_saver_exit_sem != NULL) {
    if (xSemaphoreTake(screen_saver_exit_sem, pdMS_TO_TICKS(1000)) != pdTRUE) { ... }
  } else { /* poll fallback */ }
  screen_saver_task_handle = NULL;
}
```

Cambios asociados: binario semáforo `screen_saver_exit_sem`, handle publicado
en `screen_saver_task_handle`, prioridad 5 → 4, `IDLE_TIMEOUT_S` persistido en
NVS.

### 9.5 `trackers_scanner`: la carrera de auto-borrado

```c
/* después — trackers_scanner.c:98-136 */
/* Detach from stop() before vanishing: stop() copies this handle and
 * deletes the task; a self-delete while stop() still holds the copied
 * handle would turn that delete into a delete of an already-freed TCB,
 * so the handle is cleared (and stop() re-validated against it) under the
 * stop guard before vTaskDelete(NULL). */
portENTER_CRITICAL(&trackers_stop_mux);
trackers_scan_timer_task = NULL;
portEXIT_CRITICAL(&trackers_stop_mux);
vTaskDelete(NULL);
```

Y `stop()` revalida:

```c
portENTER_CRITICAL(&trackers_stop_mux);
TaskHandle_t task_to_delete = trackers_scan_timer_task;
if (task_to_delete != NULL && task_to_delete != xTaskGetCurrentTaskHandle()) {
  trackers_scan_timer_task = NULL;
} else { task_to_delete = NULL; }
portEXIT_CRITICAL(&trackers_stop_mux);
if (task_to_delete != NULL) vTaskDelete(task_to_delete);
```

Se usa un spinlock y no un semáforo porque un `SemaphoreHandle_t` sería un
riesgo de inversión de prioridad dentro de un auto-borrado.

### 9.6 `bt_spam`: `vTaskDelete(NULL)` desde el llamante

```c
/* después — bt_spam.c:1052-1071 */
if (adv_task_running) {
  uint32_t retries = 200; /* 200 x 10ms = 2s max wait */
  while (adv_task_running && retries-- > 0) vTaskDelay(pdMS_TO_TICKS(10));
  if (adv_task_running) {
    ESP_LOGW(TAG, "Adv task didn't exit cleanly, forcing delete");
    /* Snapshot the handle and only delete while it is still published:
     * the task NULLs its own handle right before self-deleting, and
     * vTaskDelete(NULL) would delete THIS task (the caller, usually the
     * menus thread) instead. */
    TaskHandle_t task_to_delete = adv_task_handle;
    if (task_to_delete != NULL && task_to_delete != xTaskGetCurrentTaskHandle()) {
      vTaskDelete(task_to_delete);
    }
    adv_task_handle = NULL;
    adv_task_running = false;
  }
}
```

`adv_task_running` es `volatile` y va aparte del handle porque
`xTaskCreate()` toma un `TaskHandle_t*` plano, y `volatile` evita que el
compilador eleve la carga fuera del bucle de sondeo.

### 9.7 `ssid_spam`: el beacon mal construido

El `beacon_raw` antiguo era un array plano de 57 bytes que contenía **la
cabecera y los IEs** (Supported-Rates / DS-Param / TIM). El texto del SSID se
añadía en el offset 57, así que `beacon[37]` — el byte de longitud del SSID —
en realidad apuntaba dentro del IE de TIM: **todos los beacons anunciaban un
SSID falso** y las cadenas pretendidas eran basura final.

```c
/* después — ssid_spam.c:62-80, :205-226 */
#define BEACON_SSID_OFFSET 38
_Static_assert(BEACON_SSID_OFFSET == sizeof(beacon_raw), "...");
#define BEACON_MAX_SSID_LEN 32     /* el límite de 802.11 */

memcpy(beacon, beacon_raw, sizeof(beacon_raw));
memcpy(beacon + BEACON_SSID_OFFSET, ssids_attack[line], ssid_len);
memcpy(beacon + BEACON_SSID_OFFSET + ssid_len, beacon_tail, sizeof(beacon_tail));
beacon[BEACON_SSID_LEN_OFFSET] = (uint8_t) ssid_len;

size_t frame_len = BEACON_SSID_OFFSET + ssid_len + sizeof(beacon_tail);
esp_err_t err = esp_wifi_80211_tx(WIFI_IF_AP, beacon, frame_len, false);
if (err != ESP_OK) {
  /* The return value used to be discarded, so a spam that transmitted
     nothing looked exactly like one that worked. */
  ESP_LOGW(TAG, "beacon TX failed: %s", esp_err_to_name(err));
}
```

También: VLA eliminada (`uint16_t seqnum[MAX_STRINGS]` en vez de
`seqnum[total_lines]`), `spam_running` + `ssid_spam_stop_task()` para la
reentrada, y `flash_storage_get_list()` con capacidad explícita.

### 9.8 `modbus_tcp`: la longitud MBAP

El campo *Length* de la cabecera MBAP se calculaba como `6 + byte_count`;
debería ser `7 + byte_count` (UnitId(1) + PDU(6+byte_count)). El valor
antiguo era 6 bytes más corto y **ningún PLC lo aceptaba**.

```c
/* después — modbus_engine_cmd.c:68-84, :118 */
/* 13 header bytes + 2 per register / 1 per 8 coils. */
size_t payload_bytes = (function_code == 0x0F) ? ((size_t) num_values + 7) / 8
                                                : (size_t) num_values * 2;
if (13 + payload_bytes > MODBUS_TCP_MAX_REQUEST_LEN) {
  ESP_LOGE(TAG, "too many values: %d would need %u bytes, the buffer holds %u", ...);
  return 1;
}
uint8_t request[MODBUS_TCP_MAX_REQUEST_LEN] = {0};
...
data_len = 7 + byte_count; /* Length = 7 + byte_count */
```

Además `MODBUS_TCP_MAX_REQUEST_LEN 260` como constante nombrada, el
`request_len` que viene de NVS se **satura** antes de usarlo como longitud de
copia, y `modbus_engine_get_ctx()` ahora **devuelve** ante NULL en vez de
caer en una desreferencia.

### 9.9 `catdos`: fuga y desbordamiento

```c
/* antes */
for (int i = 0; i < aps_count; i++) { free(wifi_list[i]); }   /* aps_count viene de NVS */
...
aps_count = preferences_get_int("count_ap", 0);
for (int i = 0; i < aps_count; i++) {
  char wifi_ap[100]; sprintf(wifi_ap, "wifi%d", i);
  preferences_get_string(wifi_ap, wifi_ssid, 100);
  wifi_list[i] = strdup(wifi_ssid);      /* fuga en la reentrada; OOB si aps_count > 20 */
}
```

```c
/* después — catdos_module.c:415-448 */
if (aps_count > 20) { aps_count = 20; }
for (int i = 0; i < 20; i++) {
  if (wifi_list[i] != NULL) { free(wifi_list[i]); wifi_list[i] = NULL; }
}
int loaded = 0;
for (int i = 0; i < aps_count; i++) {
  char wifi_ap[32]; char wifi_ssid[100];
  snprintf(wifi_ap, sizeof(wifi_ap), "wifi%d", i);
  esp_err_t err = preferences_get_string(wifi_ap, wifi_ssid, sizeof(wifi_ssid));
  if (err != ESP_OK) continue;
  wifi_list[loaded] = strdup(wifi_ssid);
  if (wifi_list[loaded] != NULL) loaded++;
}
aps_count = loaded;
```

Más: buffer de petición 128 → 256 bytes con el retorno de `snprintf`
comprobado, `arg_end(2)` → `arg_end(3)` (off-by-one de *argtable*), y
límite explícito de 32 bytes en host/port/endpoint.

### 9.10 `uart_bridge`: el log que devolvía 0

```c
/* antes */
int custom_log(const char* format, va_list args) {
  char buffer[256];
  vsnprintf(buffer, sizeof(buffer), format, args);
  uart_bridge_write(buffer, strlen(buffer));
  return 0;                       /* mal: vsnprintf debe devolver la longitud */
}
```

```c
/* después — uart_bridge.c:74-124 */
int custom_log(const char* format, va_list args) {
  char buffer[256];
  va_list args_copy; va_copy(args_copy, args);
  int needed = vsnprintf(buffer, sizeof(buffer), format, args_copy);
  va_end(args_copy);
  if (needed < 0) return 0;
  size_t len = strlen(buffer);
  if (len > 0) {
    if (uart_wait_tx_done(UART_NUM_0, pdMS_TO_TICKS(5)) != ESP_OK) {
      return needed;              /* descartar la línea en vez de bloquear todos los loggers */
    }
    uart_write_bytes(UART_NUM_0, buffer, len);
  }
  return needed;
}
```

El `uart_wait_tx_done` con timeout de 5 ms es necesario porque
`uart_write_bytes()` es un `xRingbufferSend(..., portMAX_DELAY)` y este
*sink* lo alcanza **todos** los `ESP_LOGx`, incluido el de la tarea
`esp_timer` que ejecutaba los callbacks de botón.

Además: la VLA `char buffer[buffer_size]` → `malloc(buffer_size + 1)` con
tope `UART_BRIDGE_MAX_BUFFER_SIZE 4096`, `assert()` → comprobaciones
explícitas, y `println` parseaba con la tabla de argumentos equivocada.

### 9.11 `detector`: el orden de parada

```c
/* antes — detector.c:126-147 */
esp_wifi_set_promiscuous(true);
esp_wifi_set_promiscuous_rx_cb(packet_handler);
...
/* stop */
esp_wifi_stop();
esp_wifi_set_promiscuous(false);   /* mata la captura de OTROS subsistemas */
```

```c
/* después — detector.c:114-121, :146-153 */
if (radio_lease_prom_acquire(RADIO_PROM_DEAUTH_DETECTOR, packet_handler) != ESP_OK) {
  ESP_LOGE(TAG, "could not claim promiscuous mode");
  return;
}
...
/* Order matters: esp_wifi_stop() leaves the driver initialised but stopped,
   and the analyzer's sniffer then calls esp_wifi_set_promiscuous() /
   esp_wifi_set_channel() against a stopped driver - both fail silently and
   the packet counter stays at zero. */
radio_lease_prom_release(RADIO_PROM_DEAUTH_DETECTOR);
esp_wifi_stop();
```

### 9.12 `memory_monitor`: saturación de divisiones

```c
/* antes */
static float calculate_fragmentation(size_t total_free, size_t largest_block) {
  if (total_free == 0) return 100.0f;
  return ((float)(total_free - largest_block) / (float)total_free) * 100.0f;
}
```

```c
/* después — memory_monitor.c:29-33 */
static float calculate_fragmentation(size_t total_free, size_t largest_block) {
  if (total_free == 0 || largest_block >= total_free) {
    return (total_free == 0) ? 100.0f : 0.0f;
  }
  return ((float)(total_free - largest_block) / (float)total_free) * 100.0f;
}
```

Además, `heap_monitor_start()`/`stop()` toman un mutex alrededor de toda la
transición de estado, y **todos** los caminos de retorno temprano lo liberan.

### 9.13 `resistor_detector`: la matemática fuera del `#ifdef`

La aritmética pura se extrajo del bloque `#ifdef ESP_PLATFORM` a funciones
testeables en el host:

- `resistor_calculate_pullup_adc()`
- `resistor_calculate_pulldown_adc()`
- `resistor_calculate_voltage_divider()`
- `resistor_detector_evaluate_samples()`

Con un enum de resultado nuevo:

```c
typedef enum {
  RESISTOR_CALC_OK,
  RESISTOR_CALC_ERROR_INVALID_ARG,
  RESISTOR_CALC_ERROR_DIV_ZERO,
  RESISTOR_CALC_ERROR_OUT_OF_RANGE,
  RESISTOR_CALC_OPEN_CIRCUIT,
  RESISTOR_CALC_SHORT_CIRCUIT
} resistor_calc_status_t;
```

`adc_raw == 0` → circuito abierto; `adc_raw == adc_max` → cortocircuito;
`adc_max == 0` → `DIV_ZERO`. 12 tests de host.

### 9.14 Catálogo de módulos endurecidos

| Módulo | Commit | Qué se arregló |
|---|---|---|
| `war_bee` | `857a17a9` | `vTaskSuspend(NULL)`, `xTaskCreate` sin comprobar |
| `war_thread` | `6758054b` | trabajo pesado dentro del lock de OT (§9.3) |
| `settings` + `screen_saver` | `7d4b08f2` | contrato de parada real del salvapantallas (§9.4) |
| `web_file_browser` | `a3bf17f2` | variable definida en la cabecera → `extern` + `.c` |
| `cat_dos` | `e0b79f30` | fuga de `strdup`, OOB con `aps_count` de NVS (§9.9) |
| `file_manager` + `files_ops` | `b0163862`, `2c177f0d` | tamaños sin desbordamiento, comprobación de path traversal, 23 tests |
| `modbus_tcp` | `e73754f7` | longitud MBAP, `request_len` saturado (§9.8) |
| `ssid_spam` | `f5c2747f` | beacon mal construido (§9.7) |
| `uart_bridge` | `1a763db1`, `c9579d2b` | log sink no bloqueante (§9.10) |
| `deauth detector` | `1f86aba6` | reclamo de promiscuo, orden de parada (§9.11) |
| `memory_monitor` | `f3c3c248` | saturación de divisiones (§9.12) |
| `trackers` | `6a078453`, `6c958e7e` | carrera de auto-borrado (§9.5), orden BLE→SD |
| `wifi_analyzer` | `931260ad` | el fallo de SD ahora es fatal; `close_file` incondicional |
| `bt_spam` | `280368ca`, `913e2d81` | pila de la tarea, carrera de borrado, fugas en camino de error |
| `resistor_detector` | `c69437cc` | matemática extraída y protegida (§9.13) |
| `gps` / `wardriving` | `00a2b83d` | `CSV_LINE_SIZE` 150→256, VLA eliminada, `strcat`→`strncat`, buffers en pila |
| `open_thread` | `339f0dbd` | `channel` a `static`; suscripción al bus |
| `oled_screen` | `69427bb5` | estado a `static`, `oled_screen_ink()` |
| `radar_selector` | `e2165751` | estado a `static` |
| `flock / surveillance` | `7ce2761d`, `ddf071ca` | sin *callback* de salida; la pila BLE se quedaba arriba |
| `drone_id` | `80d47bbf` | símbolos del driver compartidos a `static` |

---

## 10. Privatización del estado

### 10.1 El patrón

*`static` a nivel de fichero + funciones de acceso.* La variable conserva su
nombre y su fichero; lo que cambia es el **enlace**, de externo a interno; la
API pública pasa a ser un par getter/setter.

```c
/* radio_selector.c */
-static uint8_t radio_selected_option;
+static uint8_t radio_selected_option;
 uint8_t radio_selector_get_selected_option() { return radio_selected_option; }
```

### 10.2 Qué se privatizó

| Módulo | Símbolos | Commit |
|---|---|---|
| `sd_card.c` | `_format_if_mount_failed`, `_sd_card_info` | `080d8b29` |
| `open_thread.c` | `mAddr`, `dataset` | `010e3446` |
| `radio_selector.c` | `radio_selected_option` | `e2165751` |
| `oled_screen.c` | `dev`, `oled_mutex` | `69427bb5` |
| `gps_module.c` | `nmea_hdl`, `gps_event_callback`, `TIME_ZONES[]` | `00a2b83d` |
| `gps_screens.c` | `gps_help_menu` | `00a2b83d` |
| `gps/wardriving/*` | 12 símbolos | `00a2b83d` |
| `open_thread_module.c` | `channel` | `339f0dbd` |
| `warbee_module.c` | `csv_file_name`, `csv_file_buffer` | `857a17a9` |
| `warthread_module.c` | `csv_file_name`, `csv_file_buffer` | `6758054b` |
| `settings_module.c` | `SD_inf` | `7d4b08f2` |
| `sd_card_settings_module.c` | `state` | `7d4b08f2` |
| `web_file_browser.c` | `mount_path`, `show_hidden_files` | `a3bf17f2` |
| `wifi_screens_module.c` | `wifi_sniffer_animation_task_handle`, `pkts_offset` | `fd139212` |
| `uart_bridge_app.c` | 4 bytes de opciones | `1a763db1` |
| `drone_id_anim.c` | `drone_anim_order`, `drone_anim_durations_ms`, `drone_animation` | `d16af92c` |
| `OTA.c` | `ota_show_event_cb` | `0beea4cc` |
| `OTA/http_server.c` | `fw_update_reset` | `0beea4cc` |
| `uart_bridge.c` | `uart_bridge_config` | `c9579d2b` |

### 10.3 Los datos fuera de las cabeceras (la limpieza del `-zmuldefs`)

**El problema.** Los arrays de bitmaps y las tablas de punteros estaban
*definidos* (no declarados) en las cabeceras. Cada unidad de traducción que
incluía la cabecera emitía **su propia copia**; el enlazado sólo lo aceptaba
porque `components/wifi_attacks/CMakeLists.txt` pasaba `-Wl,-zmuldefs` a **todo**
el firmware. Ese flag además tapaba ~70 símbolos duplicados más.

**El mecanismo.** `extern` en la `.h` + una única definición en un `.c`
hermano, con este comentario de plantilla:

```c
/* Definitions split out of main/apps/wifi_analyzer/wifi_bitmaps.h by hand: this data was defined in
   the header, so every translation unit that included it emitted
   its own copy and the linker only accepted it thanks to
   -Wl,-zmuldefs. Keep the data here and the declarations in the
   header. */
```

| Cabecera | `.c` nuevo | Qué se movió |
|---|---|---|
| `wifi_bitmaps.h` (364 → 15 líneas) | `wifi_bitmaps.c` (+349) | ~30 arrays de bitmaps + `wifi_bitmap_allArray[4]` |
| `deauth_screens.h` | `deauth_data.c` (73) | 4 × 32×32 frames, `deauth_menu[]`, `deauth_attacks[]` |
| `drone_id_bmps.h` | `drone_id_bmps.c` (24) | `drone_id_bmp_1..3_16x8[]` |
| `zigbee_bitmaps.h` | `zigbee_bitmaps.c` (65) | 4 × `zigbee_bitmap_loading_1..4[]` |
| `bitmaps_general.h` | `bitmaps_general.c` (244) | `minino_face[]`, logos, `general_bt_wifi_loading[4]` |
| `menus_include/menus.h` | `menus_data.c` (694) | **todo** el catálogo `menu_t menus[]` (2,3 KB) |

Y en `7a9aaeff` el flag `-zmuldefs` desaparece del firmware, sustituido por:

```cmake
target_link_libraries(${COMPONENT_LIB} "-Wl,--wrap=ieee80211_raw_frame_sanity_check")
```

---

## 11. Interfaz: catálogo de menús, vistas, iconos y OLED

### 11.1 El catálogo de menús sale de la cabecera

`menus_include/menus.h` *definía* `menu_t menus[]` (2,3 KB). Como
`menus_module.h` incluye `menus.h`, cada una de las ~64 unidades de
traducción que lo incluían tenía su propia copia.

Ahora (`f708fd5f`) la tabla vive en `menus_data.c` y se accede por
funciones:

```c
menu_t* menus_get(void);
uint8_t menus_count(void);
int menus_find_index(menu_idx_t id);
menu_t* menus_find(menu_idx_t id);
```

Con dos `_Static_assert` que fijan los límites:

```c
_Static_assert(sizeof(menus) / sizeof(menus[0]) <= MENU_COUNT,
               "menu catalogue has more entries than MENU_COUNT");
_Static_assert(sizeof(menus) / sizeof(menus[0]) < 256,
               "menus_count() truncates at 256 entries");
```

> `menus_count()` devuelve `uint8_t`, así que a partir de 256 entradas el
> valor se truncaría (260 → 4) y casi todo el árbol quedaría inalcanzable.

### 11.2 La estructura de una entrada

```c
typedef struct {
  menu_idx_t   menu_idx;
  menu_idx_t   parent_idx;
  const char*  display_name;
  const char*  entry_cmd;
  uint8_t      last_selected_submenu;
  menu_cb_t    on_enter_cb;      /* antes void* */
  menu_cb_t    on_exit_cb;
  bool         is_visible;
} menu_t;
```

Los `menu_cb_t` pasaron de `void*` a `void (*)(void)`, así que el compilador
impone la firma.

El catálogo se compila condicionalmente por dentro del array:

```c
#if defined(CONFIG_SDR_ENABLE)
{.display_name = "SDR", .menu_idx = MENU_ESP_SDR, .parent_idx = MENU_APPLICATIONS,
 .entry_cmd = "sdr", .on_enter_cb = esp_sdr_module_begin,
 .on_exit_cb = esp_sdr_module_stop, .is_visible = true},
#endif
{.display_name = "Spectrum", .menu_idx = MENU_SPECTRUM, .parent_idx = MENU_APPLICATIONS,
 .entry_cmd = "spectrum", .on_enter_cb = rf_spectrum_app_begin,
 .on_exit_cb = rf_spectrum_app_stop, .is_visible = true}
```

Y `menus_data.c:53-58` explica el `#include "sdkconfig.h"` explícito:

> Most of the catalogue below is gated on CONFIG_* symbols. In the IDF build
> they arrive through the toolchain, but relying on `<stdio.h>` to drag
> `sdkconfig.h` in is fragile: dropping that include would silently compile
> out ~60 entries with no diagnostic.

### 11.3 El lookup por `entry_cmd` pasó a ser exacto

```c
/* menus_module.c:54-68 */
/* Exact match: the old code compared only strlen(entry_cmd) bytes, so
   "deauth_scan" matched the "deauth" entry first and launched the wrong
   app. */
if (!strcmp(catalogue[i].entry_cmd, entry_cmd)) return i;
```

### 11.4 `menu_at_checked()`

Antes, un `get_menu_idx()` que fallaba **redirigía a la raíz**. Eso hacía que
esconder una entrada alterase la visibilidad de la raíz y dejase
`current_menu` en `-1`, desde donde el árbol de menús ya no puede salir. Ahora
hay dos variantes: `menu_at()` (con reserva a la raíz) y `menu_at_checked()`
(devuelve `NULL` y registra el error) — y los escritores (esconder/revelar)
usan la segunda.

### 11.5 Las vistas de menú

Cuatro estilos, registrados en `menu_views/menu_views.c:120-133`:

```c
static const menu_view_t s_views[] = {
    {"Classic",    render_classic},
    {"Icon List",  render_icon_list},
    {"Grid",       render_grid},
    {"Hero Dock",  render_hero},
};
```

| Índice | Nombre | Geometría |
|---|---|---|
| 0 | Classic | delega en el renderizado preexistente |
| 1 | Icon List | 4 filas × 13 px, icono en x=3, etiqueta en x=18, marco en la selección, barra de scroll a la derecha |
| 2 | Grid | rejilla 4×2 de iconos, celdas 32×26, 8 por página, doble marco |
| 3 | Hero Dock | icono grande (6,8) + título escalado ×2 si cabe en 104 px, contador `"%d of %d"`, dock de 8 iconos en y=40 |

La selección se guarda en NVS bajo la clave `"menu_view"` (uint8, 0 =
Classic). El punto único de enganche es `display_menus()` en
`menus_module.c:88-100`:

```c
  if (menus_ctx != NULL && menus_ctx->submenus_count == 0) {
    update_menus();
    if (menus_ctx->selected_submenu >= menus_ctx->submenus_count) {
      menus_ctx->selected_submenu = menus_ctx->submenus_count > 0 ? menus_ctx->submenus_count - 1 : 0;
    }
  }
  menu_views_render(menus_ctx);
  oled_screen_display_show();
```

Las vistas son **sólo de render**: la navegación, el árbol y los
*callbacks* de entrada/salida no se tocan.

El ajuste vive en `settings/menu_style/menu_style.c` y reutiliza el
selector genérico (`general_radio_selection`).

### 11.6 `menu_gfx.c` y `menu_icons.c`

**`menu_gfx.c`** (73 líneas) — utilidades de dibujo:

| Función | Qué hace |
|---|---|
| `menu_gfx_text(x, y, s, scale, lit)` | texto en coordenadas de **píxel** (no de página) y escalado entero, sobre `font8x8_basic_tr[]` |
| `menu_gfx_text_width(s, scale)` | `strlen(s) * 8 * scale` |
| `menu_gfx_text_clip(x, y, s, maxpx, lit)` | trunca a `maxpx/8` caracteres, sin envolver |
| `menu_gfx_frame(x, y, w, h)` | marco sin esquinas con 4 líneas |
| `menu_gfx_scrollbar(n, rows, top)` | barra en x=124, y=11, 3×52 px |

**`menu_icons.c`** (386 líneas) — 43 primitivas de dibujo (`p`, `ln`, `rc`,
`circ`, `arc`) y **43 glifos**: 11 de categoría (`ic_wifi`, `ic_bt`,
`ic_zig`, `ic_thread`, `ic_gps`, `ic_chip`, `ic_folder`, `ic_apps`, `ic_gear`,
`ic_info`, `ic_dot`) y 32 metáforas de acción (`ic_scan`, `ic_waves`,
`ic_bolt`, `ic_play`, `ic_doc`, `ic_download`, `ic_card`, `ic_clock`,
`ic_speed`, `ic_route`, `ic_sat`, `ic_bulb`, `ic_toggle`, `ic_moon`,
`ic_stealth`, `ic_zzz`, `ic_drone`, `ic_cam`, `ic_key`, `ic_tag`, `ic_lock`,
`ic_term`, `ic_monitor`, `ic_list`, `ic_disk`, `ic_apple`, `ic_android`,
`ic_windows`, `ic_samsung`, `ic_all`, `ic_check`, `ic_back`).

El mapa tiene **70 entradas** indexadas por el `display_name` del menú:

```c
menu_icon_fn menu_icon_for(const char* n) {
  for (unsigned i = 0; i < sizeof(MAP) / sizeof(MAP[0]); i++)
    if (!strcmp(MAP[i].name, n)) return MAP[i].fn;
  return ic_dot;
}
```

> `ic_moon` y `ic_back` están definidos pero **no están en el mapa**: glifos
> muertos.

### 11.7 OLED: SH1106 y sondeo de dirección

El soporte del SH1106 (`05c33cbb`) está en el **driver**, no en
`oled_screen`. La elección es un `choice` de Kconfig que ya existía:

```
choice DRIVER
    prompt "Driver"
    default SH1106 if BOARD_MININO
    default SSD1306 if !BOARD_MININO
```

El sondeo de dirección sustituye la dirección fija:

```c
// oled_driver_i2c.c:56-66
if (i2c_probe_address(0x3C)) {
  dev->_address = 0x3C;
  ESP_LOGI(TAG, "OLED detected at I2C address 0x3C");
} else if (i2c_probe_address(0x3D)) {
  dev->_address = 0x3D;
  ESP_LOGI(TAG, "OLED detected at alternative I2C address 0x3D");
} else {
  dev->_address = I2CAddress;
  ESP_LOGW(TAG, "OLED not responding at 0x3C or 0x3D! Check VCC/GND wiring and pullups.");
}
```

El sondeo es un START → byte de dirección → STOP, con timeout de 50 ms. El
camino SPI **no** sondea (SPI no tiene dirección).

Diferencias entre los dos controladores:

| Aspecto | SSD1306 | SH1106 |
|---|---|---|
| Bomba de carga | `0x8D` + `0x14` (desactiva la interna) | `0xAD` + `0x8B` (DC-DC **on**) + `0x32` (8,0 V) |
| Precharge | no se manda | `0xD9 0x1F` |
| Contraste | `0x81 0xFF` | `0x81 0x80` |
| Modo de direccionamiento de memoria | `0x20 0x02` + inicio de columna | no se manda (el SH1106 no lo tiene) |
| Offset de columna | ninguno | **+2** (su RAM es de 132 columnas, el panel de 128 va centrado) |
| Escaneo COM | `0xC8` siempre | según `dev->_flip` |

El offset +2 se aplica al empujar píxeles (`oled_driver_i2c.c:193-197`):

```c
  #ifdef CONFIG_SSD1306
    int _seg = seg;
  #elif CONFIG_SH1106
    int _seg = seg + 2;
  #endif
```

### 11.8 `oled_screen_ink()`

```c
/*
 * How much ink is in the frame buffer that was last pushed to the panel,
 * counted as non-zero bytes.
 *
 * Lets a caller (a console command, a test, the field) tell "the panel is dead"
 * apart from "the panel has nothing drawn on it", which look identical from
 * the outside. Every OLED issue found on this firmware - a dead animation
 * overwriting the menu, a screensaver drawing a black rectangle over the whole
 * screen - showed up as this count dropping to zero.
 */
size_t oled_screen_ink(void);
```

El comando de consola `display` (`cmd_control.c:152-167`) lo usa:
`"Report OLED frame buffer ink; 'display redraw' re-renders"`.

---

## 12. La app nueva: analizador de espectro RF

### 12.1 Qué es

Commit `f85b1a53`, 13 ficheros, +2003 líneas. Una app que **escucha el
espectro 2,4 GHz** y lo representa de cuatro maneras.

| Fichero | Líneas | Función |
|---|---|---|
| `rf_spectrum.c/.h` | 609/158 | callback promiscuo, tarea de medida, entrada, export JSON |
| `rf_spectrum_channel.c` | 36 | helpers de rango de canales (testeables en host) |
| `rf_spectrum_drone.c/.h` | 123/45 | detector pasivo de OpenDroneID / Remote ID |
| `rf_spectrum_pcap.c/.h` | 142/60 | captura PCAP estándar a microSD |
| `rf_spectrum_screens.c/.h` | 322/51 | render OLED (barras / scope / waterfall / foxhunt) |
| `rf_spectrum_web.c/.h` | 219/40 | dashboard HTTP + API JSON + API de control |

### 12.2 Las cuatro vistas

```c
typedef enum {
  SPECTRUM_VIEW_BARS = 0,      /* 14 canales Wi-Fi, gráfico de barras */
  SPECTRUM_VIEW_SCOPE = 1,     /* un canal, curva de potencia en vivo */
  SPECTRUM_VIEW_WATERFALL = 2, /* cascada 2D rodante */
  SPECTRUM_VIEW_FOXHUNT = 3    /* medidor de proximidad de alta ganancia */
} spectrum_view_mode_t;
```

Constantes: `RF_WIFI_MAX_CHANNELS 14`, `RF_ZIGBEE_MIN_CHANNEL 11`,
`RF_ZIGBEE_MAX_CHANNEL 26`, `RF_NOISE_FLOOR -95`, `RF_MAX_SIGNAL -25`.

### 12.3 Funciones

| Función | Cómo |
|---|---|
| RSSI por canal con *peak-hold* | El callback promiscuo actualiza `channel_rssi[]`, `channel_peak[]`, `channel_packets[]` |
| Scope | Anillo de 128 muestras del canal enfocado |
| Waterfall | 36 filas × 27 canales, una fila cada 3 ticks (~60 ms) |
| Fox hunting | `Prox% = (RSSI + 95) / 70 * 100` |
| **RF IDS** | Byte 0 del control de trama: `0xC0` = deauth, `0xA0` = disassoc |
| **Detección de jammer** | RSSI ≥ −45 dBm durante 20 barridos seguidos → `jammer_alert[ch]` |
| **OpenDroneID** | IE 221 con OUI `FA:0B:BC`, Remote ID francés `6A:5C:35`, NAN Action Frames `88:69:19:9D:92:09` |
| **Geiger de audio** | El zumbador pita más rápido cuanto más fuerte es la señal |
| **Log CSV** | `/sdcard/spectrum_log.csv` |
| **Captura PCAP** | `/sdcard/spectrum_NN.pcap`, link type 105 (IEEE 802.11) |
| Cambio de banda | Wi-Fi ⇄ Zigbee/802.15.4 |

### 12.4 Cómo captura

La tarea `rf_spectrum_task` (pila 4096, prioridad 5) hace, cada 20 ms:

1. **Sintonizar**: en SCOPE y FOXHUNT se fija en el canal actual; en los demás
   barreja del 1 al 14. Llama a `wifi_driver_set_channel()` (no a
   `esp_wifi_set_channel()`), por el motivo de `50496e10`: preguntar al driver
   por su canal reinicia su radio, y los buffers de la radio son ~37–40 KB de
   heap.
2. Evaluar la racha de jammer, decaer las alertas, empujar al anillo del
   scope, una fila del waterfall cada 3 ticks, suavizar el RSSI (1 dB por
   tick, picos cada 20 ticks), escribir el CSV cada 50 ticks y renderizar.

El reclamo del modo promiscuo:

```c
  if (radio_lease_prom_acquire(RADIO_PROM_SPECTRUM, spectrum_promiscuous_rx_cb) != ESP_OK) {
    ESP_LOGW(TAG, "Could not claim promiscuous mode");
  }
```

### 12.5 El dashboard web

```c
/* rf_spectrum_web.h:10-15 */
/* Not 80: the captive portal (main/apps/wifi/captive) and the File Manager web
   browser (main/modules/web_file_browser) both bind the default HTTP port, and
   Wi-Fi is up whenever the spectrum app runs. Sharing 80 made httpd_start()
   fail and the dashboard silently unavailable. */
#define RF_SPECTRUM_WEB_PORT 8080
#define RF_SPECTRUM_WEB_CTRL_PORT 32769
```

| Ruta | Método | Para qué |
|---|---|---|
| `/spectrum` | GET | el dashboard (HTML/CSS/JS en línea) |
| `/api/spectrum` | GET | instantánea JSON del estado |
| `/api/spectrum/control` | POST | `?th=<dBm>`, `?cmd=audio\|log\|proto` — **exige cabecera de admin** |

La cabecera de admin:

```c
/* rf_spectrum_web.h:17-21 */
/* Custom header the /api/spectrum/control endpoint requires. A request that
   carries it is a CORS "preflighted" request, which a browser only sends for
   same-origin calls from a page we served. Without this, any page on the
   network could tamper with the running RF Spectrum session. */
#define RF_SPECTRUM_ADMIN_HDR "X-Minino-Admin"
```

El dashboard sondea la API cada 250 ms y dibuja un gráfico de barras en un
`<canvas>`.

### 12.6 Los comandos de consola

| Comando | Ayuda |
|---|---|
| `threshold` | `Set or view RF Spectrum RSSI threshold filter (dBm or 'off')` |
| `spectrum_audio` | `Control RF Spectrum audio Geiger feedback (on, off, toggle)` |
| `spectrum_log` | `Control RF Spectrum MicroSD CSV logging (on, off, toggle)` |
| `spectrum_pcap` | `Control RF Spectrum standard PCAP capture to MicroSD (on, off, toggle)` |
| `spectrum_band` | `Switch RF Spectrum band (wifi, zigbee)` |
| `spectrum_ids` | `View or clear RF IDS deauth & jammer alerts (clear)` |
| `spectrum_drone` | `View or clear DroneID / Remote ID detections (clear)` |

---

## 13. Botones: cola y tarea propia

### 13.1 El problema

`iot_button` sondea los pines GPIO desde un `esp_timer` creado con
`dispatch_method = ESP_TIMER_TASK`: el callback corría en la **tarea
`esp_timer`** — prioridad 22, pila de 3584 bytes — de la que dependen **todos**
los temporizadores del sistema.

Desde ahí, `button_event_cb()` llamaba a `menus_input_cb()` y desde ahí a
todos los `on_enter_cb` / `select_cb` de las apps. Varios de ellos bloquean
mucho: `sd_card_mount()`, `nvs_flash_init()`, `esp_openthread_init()`,
`vTaskDelay()` de un segundo o más, o `esp_openthread_lock_acquire(portMAX_DELAY)`.
Y mientras bloqueaban, **se paraban** el temporizador de *debounce* de los
botones, los efectos de los LED, los temporizadores del salvapantallas y del
AFK, el monitor de heap y los temporizadores del Wi-Fi. Montar la SD desde una
pulsación congelaba la UI durante el montaje.

### 13.2 La solución

```c
#define INPUT_QUEUE_LEN 16
#define INPUT_TASK_STACK 4096
#define INPUT_TASK_PRIORITY 4

typedef struct {
  uint8_t button_name;
  uint8_t button_event;
} input_event_t;
```

| Aspecto | Valor |
|---|---|
| Longitud de la cola | 16 eventos (2 bytes cada uno) |
| Tarea | `"input_task"`, pila 4096, **prioridad 4** |
| Timeout de envío | **0** (nunca bloquea la tarea del temporizador) |
| Contador de perdidos | `s_dropped_events`, aviso cada 16 |

El productor (`button_event_cb`) sólo desempaqueta y encola; no registra nada
ni llama a nadie:

```c
  input_event_t ev = {.button_name = button_name, .button_event = button_event};
  /* Zero timeout: never block the esp_timer task. A full queue means an app
     callback is still running; dropping is better than stalling every timer in
     the system. */
  if (xQueueSend(s_input_queue, &ev, 0) != pdTRUE) {
    s_dropped_events++;
    if ((s_dropped_events % 16) == 1) {
      ESP_LOGW(TAG, "input queue full, dropped %u event(s) so far", (unsigned) s_dropped_events);
    }
  }
```

El consumidor (`input_task`) lee, registra y despacha:

```c
  input_event_t ev;
  while (true) {
    if (xQueueReceive(s_input_queue, &ev, portMAX_DELAY) != pdTRUE) continue;
    ESP_LOGI(TAG, "Button %s event %s", button_to_name[ev.button_name], event_to_name[ev.button_event]);
    /* ... leer cb y locked bajo el mutex de estado ... */
    if (locked || cb == NULL) continue;
    cb(ev.button_name, ev.button_event);
  }
```

### 13.3 La API nueva

```c
uint32_t keyboard_module_dropped_events(void);   /* eventos perdidos */
bool keyboard_module_inject_event(uint8_t button, uint8_t event);
```

`keyboard_module_inject_event()` mete un evento sintético **por el mismo
camino** que un botón real, desde el *callback* de `iot_button` hasta el
`input_callback` instalado. Existe para poder pilotar la tubería de entrada
desde la consola (`press <button> [event]`) sin hardware: el despertar del
salvapantallas y el renderizado del menú sólo se alcanzan con una pulsación, y
esa es la parte que más se rompe.

### 13.4 El efecto dominó

Al bajar la prioridad de los callbacks de botón de 22 a 4, la tarea de
animación (prioridad 5) pasó a poder *preemptar* el renderizado del menú. De
ahí el contrato de parada del salvapantallas (§9.4).

---

## 14. microSD: montaje, slots y errores reales

### 14.1 La máquina de estados

El estado vive en tres variables (`sd_card.c:50-55`):

```c
sdmmc_card_t* card = NULL;
static bool _format_if_mount_failed = false;
static sd_card_info_t _sd_card_info;
static bool s_sd_card_mounted = false;
static SemaphoreHandle_t s_sd_mutex = NULL;
```

```
 sd_card_begin()               → registra el comando "sdtest", crea el mutex
        │
        ▼
  UNMOUNTED ──sd_card_mount()──►  mount()
                                    ├ spi_bus_initialize()      → error real
                                    ├ esp_vfs_fat_sdspi_mount() → toma un slot de FATFS
                                    │    └ si falla: esp_vfs_fat_unregister_path()
                                    └ s_sd_card_mounted = true
        │                                     │
        │                              ESP_OK  │  error
        │                                     ▼
        │                                 MOUNTED
        │                                     │
        └─────────── sd_card_unmount() ◄──────┘
```

`mount()` es el trabajador crudo; `sd_card_mount()` es la puerta pública
blindada.

### 14.2 Los cinco arreglos

**a) El error real cuando el bus no arranca** (`d5deb48d`)

```c
  ret = spi_bus_initialize(host.slot, &bus_cfg, SPI_DMA_CH_AUTO);
  if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
    /* Report the real error: the caller (and the log) used to see
       ESP_ERR_NO_MEM, which sent the hunt for a memory problem that was
       not there. */
    ESP_LOGE(TAG, "Failed to initialize bus: %s", esp_err_to_name(ret));
-    return ESP_ERR_NO_MEM;
+    return ret;
  }
```

**b) Devolver el slot del sistema de ficheros** (`a3e1809c`)

```c
  if (ret != ESP_OK) {
    /* Give the file system slot back: a mount that fails half way leaves the
       context registered, and the next attempt then reports a bogus
       ESP_ERR_NO_MEM ("no free context") instead of the real error. */
    esp_vfs_fat_unregister_path(MOUNT_POINT);
```

**c) Montar dos veces dejaba un segundo sistema de ficheros** (`c4f07def`)

```c
esp_err_t sd_card_mount() {
  /* Mounting twice leaves a second FATFS context and a second set of buffers
     behind: the modules that save to the card (the trackers, the thread
     sniffer, the analyzer) called this without asking, and the heap ran out
     after a few of them. The card is already there: use it. */
  if (sd_card_is_mounted()) {
    return ESP_OK;
  }
```

Y el coste por montaje se redujo:

```c
  esp_vfs_fat_sdmmc_mount_config_t mount_config = {
      .format_if_mount_failed = _format_if_mount_failed,
-     .max_files = 4,
-     .allocation_unit_size = 16 * 1024};
+     .max_files = 2,
+     .allocation_unit_size = 4 * 1024};
```

`max_files` dimensiona directamente la reserva de heap del IDF
(`sizeof(vfs_fat_ctx_t) + max_files * sizeof(FIL)`, más `max_files` flags), así
que duplicarlo duplicaba el coste.

Hay **22 puntos de llamada** a `sd_card_mount()` que ahora pasan por la
puerta idempotente.

**d) El comando `sdtest`** (`161643b6`)

```c
static int sdtest(int argc, char** argv) {
  if (sd_card_is_mounted()) {
    printf("sd_card: already mounted at %s\n", MOUNT_POINT);
    return 0;
  }
  esp_err_t err = sd_card_mount();
  if (err == ESP_OK) {
    printf("sd_card: mounted at %s\n", MOUNT_POINT);
    return 0;
  }
  printf("sd_card: mount failed: %s\n", esp_err_to_name(err));
  return 1;
}
```

**e) Montar antes de la primera captura** (`9cabe192`, en la app del SDR)

```c
  /* The card is mounted before the first capture, not after it: the capture
     leaves the analog front end in a state the SD bus does not always
     answer to (the mount then fails with "command not supported" and the
     file system slots stay occupied, so the next attempt reports a bogus
     ESP_ERR_NO_MEM). */
  if (!sd_card_is_mounted()) {
    esp_err_t err = sd_card_mount();
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "microSD not available (%s): the captures are not saved", esp_err_to_name(err));
    }
  }
```

### 14.3 `flash_fs`: el montaje recursivo

> **Corrección de atribución.** El commit `b8346169` ("fix: recursive mount
> mutex preventing first-mount hang") **no** es del componente `sd_card`: es de
> `firmware/components/flash_fs/flash_fs.c` (SPIFFS). El mutex del `sd_card`
> sólo se toma alrededor de las operaciones de fichero.

---

## 15. BLE: HID, GATT, spam y sincronización GAP

### 15.1 El HID: deinit asíncrono

**El problema.** `esp_hidd_profile_deinit()` hacía `memset(&hidd_le_env, 0, ...)`
inmediatamente. Pero el *deinit* en Bluedroid es **asíncrono**: la función
sólo encola trabajo en la tarea BTC. Un `GATTS_WRITE` o `GATTS_CONFIRM` que
llegase entre medias alcanzaba punteros nulos y abortaba en la tarea BTC.

```c
/* después — esp_hidd_prf_api.c:92-110 */
void esp_hidd_wait_for_unreg(uint32_t timeout_ms) {
  int timeout = (int) (timeout_ms / 10);
  while (!hidd_le_env.unreg_signaled && timeout-- > 0) {
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  if (!hidd_le_env.unreg_signaled) {
    ESP_LOGW(HID_LE_PRF_TAG, "app_unregister completion not observed");
  }
  /* The registration is gone (or timed out): nothing will call hidd_cb
     through this profile any more, so the environment can be wiped. */
  memset(&hidd_le_env, 0, sizeof(hidd_le_env_t));
}
```

Y el nuevo `ble_hid_end()`:

```c
void ble_hid_end(void) {
  if (!s_hid_started) return;
  s_hid_started = false;
  esp_ble_gap_stop_advertising();
  /* If a host is still connected, close the ACL first: deleting the service
   * and unregistering the app under a live link produces disconnect events on
   * an already-deleted profile and wedges the next enable. */
  if (is_connected && hidd_le_env.gatt_if != ESP_GATT_IF_NONE) {
    esp_ble_gatts_close(hidd_le_env.gatt_if, hid_conn_id);
  }
  gap_dispatcher_unregister(gap_event_handler);
  esp_hidd_profile_deinit();
  /* profile_deinit() only queues stop/delete/unregister on the BTC task: the
   * stack must not be torn down until the queued work has run, otherwise a
   * second enable deadlocks on a half-registered SRS. */
  esp_hidd_wait_for_unreg(500);
  ...
  radio_lease_ble_release();
  radio_lease_ble_unblock_scan();
}
```

**El bug de la escritura LED fuera de rango** (`hid_device_le_prf.c:662-681`):

```c
-      if (param->write.handle ==
-          hidd_le_env.hidd_inst.att_tbl[HIDD_LE_IDX_REPORT_LED_OUT_VAL]) {
+      if (hidd_le_env.hidd_cb != NULL &&
+          hidd_le_env.hidd_inst.att_tbl[HIDD_LE_IDX_REPORT_LED_OUT_VAL] != 0 &&
+          param->write.handle ==
+              hidd_le_env.hidd_inst.att_tbl[HIDD_LE_IDX_REPORT_LED_OUT_VAL]) {
```

Dos fallos en una línea: `hidd_cb` podía ser **NULL**, y `att_tbl[...]`
podía ser **0** — el handle 0 no es un handle de atributo válido, así que una
escritura del host al handle 0 cumplía la comparación y llamaba al callback
con una carga "LED write" falsa.

**El `hidd_clcb_dealloc()` que vaciaba la ranura equivocada:**

```c
/* antes — devolvía true en la primera iteración, sin mirar el conn_id */
-  for (i_clcb = 0, p_clcb = ...; i_clcb < HID_MAX_APPS; i_clcb++, p_clcb++) {
-    memset(p_clcb, 0, sizeof(hidd_clcb_t));
-    return true;
+  for (p_clcb = ...; p_clcb < ... + HID_MAX_APPS; p_clcb++) {
+    if (p_clcb->in_use && p_clcb->conn_id == conn_id) {
+      memset(p_clcb, 0, sizeof(hidd_clcb_t));
+      return true;
+    }
   }
```

**La conexión.** Antes `ESP_HIDD_EVENT_BLE_CONNECT` notificaba a la UI
inmediatamente, antes del *bonding*; los informes de consumo se perdían en
silencio. Ahora la notificación se mueve al final del *auth*:

```c
      if (param->ble_security.auth_cmpl.success) {
        sec_conn = true;
        is_connected = true;
        if (hid_event_callback != NULL) hid_event_callback(true);
      } else {
        /* A failed pairing usually means our bond for this peer is stale (the
           classic HOGP "PIN or Key Missing"): the host keeps its copy of the
           key, must not be asked to pair afresh while we hold the old record.
           Drop our side so the next attempt is a clean pairing, sparing the
           user the manual "forget device" dance. */
        hid_drop_bond(bd_addr);
        if (hid_event_callback != NULL) hid_event_callback(false);
      }
```

Y todas las acciones de consumo pasan por una única puerta que responde si el
host recibió el informe o no:

```c
/* Every action button funnels through here so that "did the host actually get
   the report?" has exactly one answer. The UI needs it: the notification
   windows used to be drawn unconditionally, so pressing Volume Up with no host
   attached flashed a confirmation for something that was silently dropped. */
static bool hid_send_consumer(const char* action, uint16_t key_cmd, bool press);
```

### 15.2 La sincronización GAP del spam

**El problema** (`bt_spam_gap_sync.h:1-15`):

> Bluedroid reports four different completion events through one binary
> semaphore (ADV_STOP, SET_STATIC_RAND_ADDR, ADV_DATA_RAW_SET, ADV_START).
> Returning "done" for whichever token happens to wake the waiter desynchronises
> the cycle: a single stale token shifts every later wait by one step, so
> `is_advertising_active` no longer reflects the controller and the loop issues
> stop/configure/start without confirmation.

**La solución**: un módulo puro, sin FreeRTOS, que decide "este despertar
¿responde a la pregunta que hice?".

```c
typedef struct {
  uint32_t completed;                          /* contador monótono */
  esp_gap_ble_cb_event_t last_event;
  esp_bt_status_t last_status;
} bt_spam_gap_sync_t;

void     bt_spam_gap_sync_reset(bt_spam_gap_sync_t*);
bool     bt_spam_gap_sync_complete(bt_spam_gap_sync_t*, esp_gap_ble_cb_event_t, esp_bt_status_t);
uint32_t bt_spam_gap_sync_mark(const bt_spam_gap_sync_t*);   /* instantánea */
bool     bt_spam_gap_sync_has_new(const bt_spam_gap_sync_t*, uint32_t mark);
bool     bt_spam_gap_sync_matches(const bt_spam_gap_sync_t*, esp_gap_ble_cb_event_t wanted);
bool     bt_spam_gap_sync_succeeded(const bt_spam_gap_sync_t*);
```

**El drenaje tiene un orden obligatorio:**

```c
/* Drop every token currently posted. Must be called *before* issuing an
   asynchronous GAP command, never after: btc_transfer_context() only blocks
   until BTC_TASK dequeues the message, so the completion event has not been
   delivered yet and a drain afterwards is a no-op. */
static inline void drain_sem(void);
```

13 tests de host, incluyendo `test_every_out_of_order_permutation_resolves`
(las 8 permutaciones × 4 objetivos).

### 15.3 La validación del payload AD

```c
/* A malformed advertising payload is worse than a missing one: a wrong AD
   length byte or a .len that disagrees with the byte count makes the peer's
   parser discard the whole advertisement, so the model silently never fires.
   Both mistakes are invisible in the struct itself (the compiler only warns
   about an overflowing initializer), which is why the table is checked here
   once per start instead of being trusted. */
static bool ad_payload_is_well_formed(const spam_model_t* model) {
  if (model->len == 0 || model->len > sizeof(model->data)) return false;
  uint8_t i = 0;
  while (i < model->len) {
    uint8_t ad_len = model->data[i];
    if (ad_len == 0) return false;
    if ((uint32_t) i + 1u + ad_len > model->len) return false;
    i = (uint8_t) (i + 1u + ad_len);
  }
  return i == model->len;
}
```

### 15.4 El exclusivity publicitaria

```c
  /* Spam needs the radio completely idle before it can work at all: the gate
   * is taken *before* stopping the scan so no scanner's restart loop re-arms
   * a scan behind our back (the restart loops of the bt_gattc consumers check
   * radio_lease_ble_scan_allowed() before re-arming). */
  radio_lease_ble_block_scan();
```

Y el error que evita:

```
"Advertising or scanning now, can't set randaddress"
"No random address yet, please set random address and try"
```

---

## 16. Build system: variantes, Kconfig y el cap del banco

### 16.1 El problema de las dos pasadas de CMake

ESP-IDF evalúa el `CMakeLists.txt` de cada componente **dos veces**:

1. Al expandir el árbol de componentes — **antes** del paso de Kconfig, así que
   todavía no existe ninguna variable `CONFIG_*`.
2. Al compilar — después de Kconfig.

Un `if(CONFIG_FOO)` a secas se lleva la rama falsa en la primera pasada, y el
componente queda registrado **sin fuentes y sin requisitos**. El error
resultante es críptico:

```
bt_gattc.h includes esp_bt.h ... not in the requirements list of bt_gattc
```

### 16.2 El helper `minino_kconfig_enabled()`

`firmware/cmake/minino_kconfig.cmake` (40 líneas, nuevo en `8d4173ce`):

```cmake
function(minino_kconfig_enabled OPTION RESULT)
    set(${RESULT} 0 PARENT_SCOPE)
    if(NOT DEFINED SDKCONFIG OR NOT EXISTS "${SDKCONFIG}")
        return()
    endif()
    file(STRINGS "${SDKCONFIG}" _hits REGEX "^CONFIG_${OPTION}=")
    foreach(_hit IN LISTS _hits)
        if(_hit MATCHES "=y$")
            set(${RESULT} 1 PARENT_SCOPE)
            return()
        endif()
    endforeach()
endfunction()
```

El `sdkconfig` existe en **las dos** pasadas, así que es la fuente fiable.
El valor debe estar **fijado** en el `sdkconfig` (que es lo que hacen los
`sdkconfig.defaults`), si no puede cambiar entre pasadas.

Uso:

```cmake
include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/minino_kconfig.cmake")
minino_kconfig_enabled(BT_ENABLED _minino_opt_on)
if(_minino_opt_on)
    idf_component_register(SRCS ...)
else()
    idf_component_register()
endif()
```

### 16.3 Los componentes que se pueden dejar fuera

| Componente | Símbolo que lo gobierna |
|---|---|
| `ble_hid`, `ble_scann`, `bt_gattc`, `bt_gatts`, `bt_spam`, `drone_id`, `uart_sender` | `BT_ENABLED` |
| `esp-zigbee-console`, `zb_cli`, `zigbee_light`, `zigbee_switch` | `MININO_ZIGBEE_STACK` |
| `open_thread`, `thread_broadcast` | `MININO_OPENTHREAD_STACK` |
| `gap_dispatcher`, `radio_lease` | *a nivel de fuente* (`#if CONFIG_BT_ENABLED`) |

Y en `firmware/main/CMakeLists.txt`, las apps de `main` se filtran por ruta:

```cmake
if(NOT _minino_bt_on)
    list(FILTER modules_src EXCLUDE REGEX "/apps/ble/")
    list(FILTER modules_src EXCLUDE REGEX "/apps/wifi/drone_id/")
    list(FILTER modules_src EXCLUDE REGEX "/apps/wifi/droneid_scanner/")
endif()
if(NOT _minino_zigbee_on)
    list(FILTER modules_src EXCLUDE REGEX "/apps/zigbee/")
    list(FILTER modules_src EXCLUDE REGEX "/modules/zigbee/")
    list(FILTER modules_src EXCLUDE REGEX "/modules/war_bee/")
    list(FILTER modules_src EXCLUDE REGEX "/modules/settings/zb_cli_settings/")
endif()
if(NOT _minino_openthread_on)
    list(FILTER modules_src EXCLUDE REGEX "/apps/thread_sniffer/")
    list(FILTER modules_src EXCLUDE REGEX "/modules/open_thread/")
    list(FILTER modules_src EXCLUDE REGEX "/modules/war_thread/")
endif()
```

Y los tests de host se excluyen de la compilación del firmware:

```cmake
foreach(file ${ALL_SOURCE_FILES})
    if(NOT file MATCHES "/test/")
        list(APPEND modules_src ${file})
    endif()
endforeach()
```

> **Trampa de CMake.** `firmware/main/CMakeLists.txt:57-60` avisa de que **no**
> se añada `REQUIRES`/`PRIV_REQUIRES` a `main`: `main` usa el comportamiento
> por defecto de IDF ("requiere todos los demás componentes"), y nombrar uno
> lo desactiva y saca a todos los demás del *include path*.

### 16.4 Las opciones de Kconfig

| Símbolo | Tipo | Defecto Kconfig | Efectivo | Dónde se usa |
|---|---|---|---|---|
| `SDR_ENABLE` | bool | `n` | **`y`** (`sdkconfig.defaults:163`) | `main/CMakeLists.txt:87`, `esp_sdr_memory.c:38`, `menus_data.c:644` |
| `MININO_ZIGBEE_STACK` | bool | `y` | `y` | `main/CMakeLists.txt:16,26-31` |
| `MININO_OPENTHREAD_STACK` | bool (`depends on MININO_ZIGBEE_STACK`) | `y` | `y` | `main/CMakeLists.txt:17,32-36` |

El prefijo `MININO_` es a propósito: un símbolo de Kconfig no puede definirse
en dos sitios, y `OPENTHREAD_ENABLED` ya lo define el componente de upstream.

El texto de ayuda de `SDR_ENABLE` describe el mecanismo:

> The engine needs the whole of SRAM block 2 (0x40840000) as its capture
> bank, so the application image has to fit below it: the build caps the SRAM
> flow segment at the block boundary and reserves the block from the heap. If
> the image does not fit, the link fails with "SDR: the DRAM image overlaps
> the capture bank" instead of corrupting the heap at capture time.

### 16.5 `sdkconfig.defaults`: el recorte de IRAM

```diff
+# The SDR build variant needs the whole of SRAM block 2 (0x40840000) as the
+# capture bank, so the application image has to fit below it. Moving the Wi-Fi
+# and PHY hot code out of IRAM gives back 45 KB of SRAM, the same trade the
+# esp-sdr project makes ("Leave enough non-modem SRAM for static data and
+# task stacks"). The Wi-Fi gets a little slower; nothing changes functionally.
+# CONFIG_ESP_WIFI_IRAM_OPT is not set
+# CONFIG_ESP_WIFI_EXTRA_IRAM_OPT is not set
+# CONFIG_ESP_WIFI_RX_IRAM_OPT is not set
+# CONFIG_ESP_WIFI_SLP_IRAM_OPT is not set
+# CONFIG_ESP_PHY_IRAM_OPT is not set
+# CONFIG_PM_SLP_IRAM_OPT is not set
+
+# The SDR app: the capture bank is reserved and the image is capped below it.
+CONFIG_SDR_ENABLE=y
```

> **`# CONFIG_X is not set` no es un comentario**: es la sintaxis de
> *kconfgen* para desactivar. Pero `CONFIG_PM_SLP_IRAM_OPT` está en `=y` en la
> línea 10 y en "not set" en la 160; como en los ficheros de *defaults*
> gana la línea **última**, el símbolo sigue activo en el `sdkconfig`
> generado (`firmware/sdkconfig:1818`).

### 16.6 El `vrt_packet` vendorizado

Antes se compilaba directamente de un *checkout* de `esp-sdr` que
`tools/patches/apply_esp_sdr_minino.py` traía a `<repo>/build-sdr`. Ese árbol
está en `.gitignore` y el script ya no existe, así que la build **degradaba
en silencio** a un componente vacío (y el mensaje de error apuntaba a un
script inexistente). Ahora las fuentes están en el repo:

```
firmware/components/vrt_packet/
├── iq_pack.c   163
├── iq_pack.h    64
├── vrt_packet.c 439
└── vrt_packet.h 313
```

### 16.7 Las particiones

Sólo cambió `partitions_no_ota.csv`:

```diff
-internal,   data,  spiffs,         ,     4248K,
-factory,    app,  factory,         ,     3200K,
+internal,   data,  spiffs,         ,     4248K,
+factory,    app,  factory,         ,     3700K,
```

| Tabla | Cambio |
|---|---|
| `partitions.csv` (8 MB, OTA) | sin cambios: `ota_0` y `ota_1` de 3700K |
| `partitions_4MB.csv` | sin cambios: `factory` de 3400K |
| `partitions_no_ota.csv` | `factory` 3200K → **3700K** (+500K) |
| `partitions_8MB_NO.csv` | **huérfana**: no la referencia ningún perfil; sigue con 3200K |

### 16.8 Los objetivos del Makefile

```make
FLASH_FIT_BOARDS_4MB = bsides dragonjar ekoparty bugcon bsseattle

flash_fit:
	@python3 tools/check_flash_fit.py --partition-table partitions.csv \
		--app-bin build-minino/minino.bin --label minino || exit 1; \
	for board in $(FLASH_FIT_BOARDS_4MB); do \
		python3 tools/check_flash_fit.py --partition-table partitions_4MB.csv \
			--app-bin build-$$board/minino.bin --label $$board || exit 1; \
	done
```

Y el orquestador de tests:

```make
tests:
	$(MAKE) -C components/surveillance_detect/test run-tests
	$(MAKE) -C components/nmea_parser/test run-tests
	$(MAKE) -C components/drone_id/test run-tests
	$(MAKE) -C components/files_ops/test run-tests
	$(MAKE) -C components/gap_dispatcher/test run-tests
	$(MAKE) -C components/radio_lease/test run-tests
	$(MAKE) -C components/event_bus/test run-tests
	$(MAKE) -C components/bt_spam/test run-tests
	$(MAKE) -C main/modules/resistor_detector/test run-tests
	$(MAKE) -C main/modules/cmd_control/test run-tests
	$(MAKE) -C main/modules/esp_sdr/test run-tests
	$(MAKE) -C main/modules/menus_module/test run-tests
	$(MAKE) -C main/apps/ble/hid_device/test run-tests
	$(MAKE) -C main/apps/rf_spectrum/test run-tests
	$(MAKE) -C main/general/general_scrolling_text/test run-tests
	python3 tools/test/test_flash_tools.py
	python3 tools/test/test_sdr_patch.py
```

> **Roto:** la última línea invoca `tools/test/test_sdr_patch.py`, un fichero
> que `fe1e0223` borró. Ver §22.3.

`flash_fit` está encadenado en el script de release
(`firmware/get_builds.sh:23-24`):

```bash
make builds
make flash_fit
make mergebins
```

---

## 17. Tests de host y CI

### 17.1 Por qué tests de host

La alternative a un test de host es un test en la placa: hay que flashear,
encender, hacer la acción y comprobar por el puerto serie. Eso no escala y
no se puede ejecutar en cada commit. Un test de host es un `gcc` contra el
PC, con *mocks* de las cabeceras de ESP-IDF.

Las reglas que hacen que funcione:

1. **El código bajo test no puede incluir cabeceras de ESP-IDF.** Por eso
   `bt_spam_gap_sync.c`, `radio_lease_registry.c`, `event_bus.c`,
   `rf_spectrum_channel.c` y la matemática del `resistor_detector` están
   separados de sus módulos.
2. **Cada suite trae sus propios *mocks*.** Los directorios `test/mock/`
   contienen las cabeceras mínimas: `esp_err.h`, `esp_log.h`,
   `freertos/FreeRTOS.h` (el spinlock, como un contador), `esp_gap_ble_api.h`,
   `sdkconfig.h`…
3. **Unity** viene de ESP-IDF (`$IDF_PATH/components/unity/unity/src`), salvo
   `files_ops/test/unity/`, que la *vendoriza* dentro del árbol para no
   depender de nada externo.

### 17.2 Las 16 suites

| Suite | Tests | Qué fija |
|---|---|---|
| `components/surveillance_detect/test` | 80 | Firmas, matching, IE, overlay, motor, wardriving |
| `components/nmea_parser/test` | 22 | API de decodificación NMEA manual |
| `components/drone_id/test` | 27 | Altitud, tiempo Unix, OpenDroneID, spoofing, UTM |
| `components/files_ops/test` | 23 | Tamaños sin desbordamiento, path traversal, nombres |
| `components/gap_dispatcher/test` | 9 | El *dispatcher* no roba el slot de Bluedroid |
| `components/radio_lease/test` | 12 | Reparto de tramas, recuento atómico |
| `components/event_bus/test` | 11 | Aislamiento de temas, reentrada |
| `components/bt_spam/test` | 13 | Sincronización GAP |
| `main/modules/resistor_detector/test` | 12 | Aritmética y casos límite |
| `main/modules/cmd_control/test` | 16 | Parseo de argumentos de la consola |
| `main/modules/esp_sdr/test` | 23 | Protocolo esdr + empaquetado I/Q |
| `main/modules/menus_module/test` | 13 | Catálogo de menús (4 variantes de build) |
| `main/apps/ble/hid_device/test` | 11 | Perfil HID |
| `main/apps/rf_spectrum/test` | 11 | Canales Wi-Fi/Zigbee |
| `main/general/general_scrolling_text/test` | 12 | El splitter del texto rodante |
| `firmware/tools/test/test_flash_tools.py` | 5 | El comprobador de flash |

**Total: 15 suites en C + 1 en Python, 278 tests escritos.** De ellos, 252 pasan,
4 fallan (`event_bus`) y 22 no compilan (`gap_dispatcher` y `menus_module`).
Ver §22.

### 17.3 El `gap_dispatcher` es el ejemplo canónico

Su cabecera de test explica por qué existe:

> Bluedroid keeps a single GAP callback registered via
> `esp_ble_gap_register_callback()`. If multiple modules call that API, each
> call overwrites the previous one and prior modules silently miss events
> (which is a common cause of "BLE scanning/ADV stopped working").

El *mock* (`test/mock/esp_bt_main.c`) reproduce el comportamiento real:
`esp_ble_gap_register_callback()` **rechaza** la llamada si la pila no está
ENABLED y guarda **un solo** manejador (gana el último).

Los tests clave:

| Test | Qué fija |
|---|---|
| `test_registering_later_does_not_starve_earlier_subscribers` | **el fallo que el componente existe para evitar** |
| `test_registering_after_a_bluedroid_restart_reclaims_the_slot` | tras un ciclo disable/enable, el siguiente registro recupera el slot |
| `test_registering_twice_does_not_duplicate` | el spam y los escáneres reentran en `begin()` en cada visita al menú |
| `test_subscriber_table_is_bounded_and_reports_overflow` | la tabla tiene tope y el desborde no expulsa a nadie |

### 17.4 El `menus_module` se compila en cuatro variantes

```make
build/run-full build/run-no-sd build/run-no-bluetooth build/run-no-wifi
```

Las variantes reducidas exponen el fallo de "hijo huérfano", que en una build
completa no se ve: si una entrada hija se compila fuera del `gate` de su
padre, `navigation_exit()` aterriza en un padre que no resuelve.

El `mock/sdkconfig.h` (99 líneas) es el sustituto del `sdkconfig` real con
overrides `TEST_CONFIG_<FEATURE>_ENABLED`.

### 17.5 La CI

`.github/workflows/host-tests.yml`: un único job en `ubuntu-latest`, sin
matriz.

```yaml
      - name: Fetch Unity from ESP-IDF
        run: |
          git clone --depth 1 -b release/v5.5 --filter=blob:none --sparse \
              https://github.com/espressif/esp-idf.git "$RUNNER_TEMP/esp-idf"
          git -C "$RUNNER_TEMP/esp-idf" sparse-checkout set components/unity components/console
          git -C "$RUNNER_TEMP/esp-idf" submodule update --init --depth 1 components/unity/unity
```

Cada suite es un paso con `IDF_PATH: ${{ runner.temp }}/esp-idf`.

Los comentarios de los pasos explican **por qué** cada suite existe, que es
justo lo que hace útil una CI:

> # Bluedroid keeps a single GAP callback, so gap_dispatcher is the only
> # reason BLE spam, BLE HID, the scanners and the gatt console commands can
> # all receive GAP events at once. It regressed twice.

> # event_bus cannot carry GAP events; it replaced the GAP registration in
> # four gattcmd handlers and silently did nothing.

> # The BLE spam advertising loop waits on one semaphore fed by four GAP
> # completion events; a stale token desynchronised it and stopped the spam.

> # The scrolling-text splitter wrote one entry past its heap array because
> # its counting and filling passes used different wrap thresholds; every
> # Help/Credits/Legal screen goes through it.

> **Estado actual: la CI está rota.** Ver §22.

---

## 18. Tools de host

Cinco ficheros nuevos en `firmware/tools/`, que antes no existía.

### 18.1 `check_flash_fit.py` (97 líneas)

Parsea la tabla de particiones de ESP-IDF, encuentra la partición `app` **más
pequeña** y la compara con el binario.

```
python3 tools/check_flash_fit.py --partition-table <csv> --app-bin <bin> \
        [--min-free-percent FLOAT] [--label STR]
```

| Opción | Por defecto | Significado |
|---|---|---|
| `--partition-table` | *obligatorio* | Ruta del CSV de particiones |
| `--app-bin` | *obligatorio* | Ruta del binario de la app |
| `--min-free-percent` | **5,0** | Espacio libre mínimo (%) |
| `--label` | `app` | Prefijo de cada línea de salida |

Salida:

```
[minino] partition size: 3700.0 KiB
[minino] image size: 3336.0 KiB
[minino] free: +364.0 KiB (9.84%)
[minino] OK
```

Códigos de salida: 0 si pasa, 1 si falla. Mensajes de fallo:
`ERROR: Partition table not found`, `ERROR: App binary not found`,
`ERROR: No app/factory partition found in partition table`,
`FAIL: free space X% below minimum Y%`.

El CSV se parsea tolerante: se saltan las líneas de comentario, las filas con
menos de 5 campos y las que no parsean; los tamaños aceptan sufijos `K`/`M` y
los offsets `0x…`.

### 18.2 `fix_bootloop.sh` (38 líneas)

Recuperación deuna placa que se queda en bucle de reinicio tras una OTA mala. Resetea el
`otadata` al slot 0 y escribe una imagen buena de Minino en **ambos** slots
OTA.

```bash
bash firmware/tools/fix_bootloop.sh [PUERTO]     # PUERTO por defecto: /dev/ttyACM0
```

| Paso | Qué hace | Offset |
|---|---|---|
| 1 | Resetea `otadata` al slot 0 | `0x15000` |
| 2 | Flashea Minino en el slot 1 (para eliminar la imagen vieja) | `0x440000` |
| 3 | Asegura Minino en el slot 0 | `0xa0000` |

| Constante | Valor |
|---|---|
| Sonda del puerto | 30 iteraciones × 0,5 s (15 s), con `/dev/ttyACM1` como alternativa |
| Velocidad | 460800 baud |
| Chip | `esp32c6` |
| Artefactos | `firmware/build/ota_data_initial.bin`, `firmware/build/minino.bin` (relativos a la **raíz** del repo) |

Los mensajes están en español.

### 18.3 `sdr_capture.py` (192 líneas)

Pilota el receptor ESP-SDR por USB Serial/JTAG: sincroniza, sintoniza, saca
un burst de I/Q y escribe los bytes crudos a un fichero, con estadísticas de
señal.

```
python3 firmware/tools/sdr_capture.py [--port PUERTO] [--freq MHz] [--count N]
                                      [--rate R] [--out FICHERO]
```

| Opción | Por defecto |
|---|---|
| `--port` | `/dev/ttyACM0` |
| `--freq` | **2400** MHz (100 a 6000) |
| `--count` | **4096** muestras |
| `--rate` | **0** (decimación; 0 = a máxima velocidad) |
| `--out` | `sdr_capture.bin` |

| Constante | Valor |
|---|---|
| Puerto serie | 115200 baud, `dtr = False`, `rts = False` |
| Sincronización | 10 intentos de `TRANSPORT?`, 1 s entre ellos |
| Envasado `CAP20` | **20 bits por par** (10 de I, 10 de Q), *little-endian* |
| Bytes de carga | `count * 20 / 8` |
| Bits de I/Q | 10, con centro en 512 |

El protocolo es `TRANSPORT?`, `VERSION?`, `LIMITS?`, `FREQ <MHz>` (debe
responder exactamente `OK`), `CAP20 <count> <rate>` → `DATA <count>` + carga
cruda + `END`.

La comprobación de señal compara el RMS de la magnitud compleja con su pico:
si el RMS es `< 1.0` avisa de que *"the burst is flat, the receiver may not be
running"*; si el RMS supera al pico, error interno y salida 1.

Requiere `pyserial`.

### 18.4 `test_live_sdr.py` (119 líneas)

Comprobación de radio en vivo, en español, en cinco pasos: detectar si está
Minino OS o esp-sdr (enviando `boot_sdr` si es Minino), abrir la consola del
esp-sdr a **2 000 000 baud**, `FREQ 2412`, `CAP20 512 0` y demodular el I/Q de
20 bits, y enviar `MININO` para volver a Minino.

```bash
python3 firmware/tools/test_live_sdr.py [PUERTO]
```

| Constante | Valor |
|---|---|
| `boot_sdr` | 115200 baud |
| Consola del SDR | **2 MBaud** |
| Muestras | 512, tasa 0, carga de 1280 bytes |
| I/Q | 10 bits en complemento a dos, centrados en 512 |
| dBFS | `20 * log10(max(1e-6, avg_pwr / 512.0))` |

Dibuja una barra ASCII de los 12 primeros muestras.

> Este script usa `boot_sdr`, un comando que **ya no existe** (§4.7). Queda
> como referencia del protocolo.

### 18.5 `test/test_flash_tools.py` (90 líneas)

Suite `unittest` de Python para `check_flash_fit.py`. Empezó con 294 líneas
cubriendo también `check_ota_image.py`, que `ff3cd744` eliminó.

| Test | Qué fija |
|---|---|
| `test_parse_explicit_and_implicit_offsets` | un offset en blanco (layout automático) debe quedarse en 0, no inventarse |
| `test_smallest_app_partition_is_used` | se usa la partición `app` más pequeña |
| `test_binaries_with_size_suffixes` | un tamaño `3M` son 3 × 1024 × 1024 bytes |
| `test_cli_rejects_tight_fit` | 100K de partición, 99K de imagen, `--min-free-percent 5` ⇒ devuelve **1** |
| `test_cli_accepts_adequate_room` | 100K de partición, 50K de imagen ⇒ devuelve **0** |

---

## 19. Cambios menores por módulo

### 19.1 `files_ops`: tamaños sin desbordamiento y path traversal

`2c177f0d` (348 líneas) añade la API de seguridad de ficheros:

```c
bool files_ops_has_path_traversal(const char* path);
bool files_ops_is_valid_filename(const char* filename);
bool files_ops_sanitize_filename(const char* input, char* output, size_t out_len);
bool files_ops_join_path(const char* dir_path, const char* name,
                         const char* extension, char* out_buf, size_t out_len);
bool files_ops_incremental_name_buf(const char* dir_path, const char* base_name,
                                    const char* extension, char* out_buf, size_t out_len);
size_t files_ops_get_file_size(FILE* file);
char* files_ops_format_size(size_t size);
bool files_ops_format_size_buf(size_t size, char* out_buf, size_t buf_len);
bool files_ops_exists(const char* dir_path, const char* name, const char* extension);
```

Es la suite más grande: **23 tests** con Unity *vendorizado* en el árbol
(`files_ops/test/unity/`), la única que no depende de `IDF_PATH`.

### 19.2 `nmea_parser`: API de decodificación manual

`c6a814c4` extrae la decodificación a una API que no necesita ESP-IDF, para
poder testearla en el host: 22 tests.

> **Corrección de atribución.** `045de824` ("explicit source list, single
> julian_day definition; add host tests") es el cambio de la lista de
> fuentes del **`drone_id`**, no del `nmea_parser`: el `CMakeLists.txt` del
> `nmea_parser` es **byte a byte idéntico** a `main` en este branch.

### 19.3 `drone_id`: los spoofers al heap

Los spoofers de OpenDroneID (uno por dron configurado) pasaron al heap:
**37,7 KB** de `.bss` liberados, que hicieron falta para el banco de captura.
`f67c001b` elimina además un `opendroneid.c` duplicado.

### 19.4 `gap_dispatcher` y el resto de componentes BLE

| Cambio | Detalle |
|---|---|
| `ble_scann` | El escáner **sólo** se arma después del *bring-up* del GATTC, y consulta la puerta de escaneo (`029ee8c3`) |
| `bt_gatts` | Silenciar un aviso de variable no usada (`c4c38379`) |
| `bt_gattc` | `begin`/`stop` de la tarea del cliente a prueba de errores, id de app único (`de1e3dda`) |
| `2452a801` | El reintento de escaneo BLE se saca de la tarea BTC |
| `e1dfac40` | Los ajustes PIB se aplican en cada enable del driver |
| `3d95ad2b` | Estado estático del *pool* de mensajes |

### 19.5 El `flash_fs` (SPIFFS)

`b8346169` — el montaje recursivo se colgaba en el primer montaje. El mutex se
tomaba en un camino que volvía a entrar en él. 125 líneas cambiadas.

### 19.6 `OTA`

`0beea4cc` reformatea `http_server.c` (1232 líneas, CRLF → LF) y hace
`static` el `fw_update_reset`. `wifi_ap.c` deja de llamar a `esp_wifi_init()`
y usa el `wifi_service` (§8), y publica `EV_WIFI`.

### 19.7 `ieee_sniffer`

69 líneas cambiadas: integración con el *lease* y con el `gap_dispatcher`.

### 19.8 `radio_selector`

`e2165751`: `radio_selected_option` pasa a `static`; el accessor ya existía.

### 19.9 La localización

`8f4a3ec5` ("Feat: Localize Spanish Text and translate to english") y su
merge (`996ee3ee`) traducen los textos de la interfaz al inglés y preparan
la estructura de localización.

### 19.10 `general_scrolling_text`

`689c925a`: el *splitter* del texto rodante escribía **una entrada más allá**
de su array de heap, porque el paso de conteo y el de relleno usaban
umbrales de wrap distintos. Como todas las pantallas de Help/Credits/Legal
pasan por ahí, se añadió un *split helper* y 12 tests de host.

### 19.11 La cola de tareas de la consola

`da81e333` mueve los comandos de consola a tareas propias, para que un comando
que bloquee (por ejemplo un *ping* largo) no atasque el REPL. También añade
16 tests de parseo de argumentos.

---

## 20. Tabla completa de commits

### 20.1 Del 4 al 18 de septiembre de 2026

| Commit | Fecha | Asunto |
|---|---|---|
| `8f4a3ec5` | 2026-09-04 | Feat: Localize Spanish Text and translate to english |
| `bed0b16c` | 2026-09-04 | fix: format code for better readability in hid_screens.h, animations_module.c, and screen_saver.c |
| `996ee3ee` | 2026-09-04 | Merge pull request #94 from ElectronicCats/Traasnlate |
| `931260ad` | 2026-09-10 | feat: Harden Wi-Fi sniffing and wardriving for improved stability |
| `093f257b` | 2026-09-17 | fix: Enhance scanner OLED display and data rendering |
| `31a7061f` | 2026-09-18 | Merge pull request #95 from ElectronicCats/Fix_pcap |

### 20.2 Del 11 al 12 de octubre de 2026 — las vistas de menú

| Commit | Fecha | Asunto |
|---|---|---|
| `82927939` | 2026-10-11 | feat: Add selectable OLED menu view styles |
| `8c485cb1` | 2026-10-12 | feat: Improve OLED menu icon rendering and layout |
| `7effae44` | 2026-10-12 | style: Format menu icon map for readability |

### 20.3 Del 19 de octubre de 2026 — los componentes nuevos

| Commit | Asunto |
|---|---|
| `d56b9e7a` | feat: add radio lease with atomic BLE refcount, scan gate and host tests |
| `3fbb6d9f` | feat: add GATT event fan-out dispatcher and hook stack-reset |
| `92bc14f9` | feat: add typed fan-out event bus |
| `c2f47398` | feat: add singleton WiFi init service |
| `ad5c94ef` | build: register vrt_packet component |
| `de1e3dda` | fix: error-safe GATT client task begin/stop with unique app id |
| `c4c38379` | chore: silence unused warning in gatts module |
| `18871f04` | fix: async HID deinit, NULL guards, LED write OOB and connect handling |
| `029ee8c3` | fix: only arm scanner after gattc bring-up; respect scan gate |
| `6a078453` | fix: harden tracker timer task lifecycle and start gating |
| `280368ca` | fix: spam task stack, task-delete race and early-return cleanup; add GAP sync host tests |
| `2452a801` | fix: move BLE scan retry off the BTC task |
| `fdf0f955` | refactor: defer wifi init to wifi_service |
| `0beea4cc` | style: reformat http_server; fix wifi_ap timeout |
| `7a9aaeff` | build: wrap raw-frame sanity check instead of -zmuldefs |
| `b8346169` | fix: recursive mount mutex preventing first-mount hang |
| `2c177f0d` | fix: overflow-safe sizes and path traversal check; add host tests |
| `c6a814c4` | feat: manual NMEA decode API for host tests |
| `045de824` | build: explicit source list, single julian_day definition; add host tests |
| `e1dfac40` | fix: apply PIB settings on every driver enable |
| `80d47bbf` | fix: static-ify shared driver symbols |
| `c9579d2b` | fix: non-blocking log sink with correct vsnprintf length |
| `080d8b29` | refactor: static-ify module globals |
| `e2165751` | refactor: make radio selection state private |
| `010e3446` | refactor: make module globals private |
| `3d95ad2b` | fix: fix message pool static state |
| `3321c30f` | refactor: defer wifi init to wifi_service |
| `d84eb372` | refactor: defer wifi init to wifi_service |
| `f9aff880` | refactor: defer wifi init to wifi_service |
| `cdd1b586` | refactor: defer wifi init to wifi_service |
| `52138a31` | refactor: defer wifi init to wifi_service |
| `07bf9bfe` | feat: add SDR launcher Kconfig |
| `9c7599ee` | build: exclude host tests from sources; link librftest for CW TX |
| `449b0144` | fix: restore input handling after HID begin; extract data tables |
| `574dfe5e` | feat: unique GATTC app ids via gatt dispatcher; scan gate checks |
| `37869cc6` | fix: gate adv scanner start and stop on bring-up status |
| `913e2d81` | refactor: align spam module with hardened bt_spam lifecycle |
| `6c958e7e` | refactor: align trackers module with hardened scanner lifecycle |
| `f85b1a53` | feat: add RF spectrum analyzer app |
| `1f2f96b9` | feat: add SDR handover module and dual-slot image checks |
| `1a763db1` | fix: harden uart bridge app lifecycle |
| `049500e5` | feat: subscribe sniffer screens to event bus |
| `fd139212` | refactor: move wifi bitmaps out of header |
| `ca0a422d` | fix: defer wifi init to wifi_service |
| `84879287` | refactor: move deauth data tables out of header |
| `1f86aba6` | fix: guard detector lifecycle |
| `d16af92c` | refactor: move drone bitmaps out of header |
| `f67c001b` | build: drop duplicated opendroneid.c |
| `e73754f7` | fix: harden modbus app data and scenes |
| `f5c2747f` | fix: harden ssid spam lifecycle |
| `5508c825` | feat: wire event bus and wifi service in app_main |
| `d270b3a9` | fix: bounds-check severity and component indexes |
| `f3c3c248` | fix: guard monitor state with a mutex and clamp divisions |
| `2d97d08b` | fix: null-safe task naming and leak on self-delete; add deinit |
| `05c33cbb` | feat: add SH1106 support and probe on init |
| `689c925a` | refactor: add split scroll helper and host tests |
| `56f58e1c` | refactor: move shared bitmaps out of header |
| `f708fd5f` | feat: move menu table to code; add menus data and host tests |
| `181feee8` | feat: queue button events to a dedicated task |
| `da81e333` | test: add cmd_control host tests; queue console tasks |
| `a3bf17f2` | fix: harden web file browser routes |
| `7d4b08f2` | fix: harden settings module lifecycle |
| `a7457dd0` | refactor: align animation task with event bus |
| `e0b79f30` | fix: harden catdos module and commands |
| `b0163862` | fix: harden file manager lifecycle |
| `00a2b83d` | refactor: make gps module state private; harden wardriving |
| `31953a42` | refactor: make modal state private |
| `69427bb5` | refactor: make oled screen state private |
| `339f0dbd` | refactor: make open thread module state private |
| `c69437cc` | test: add resistor detector host tests; harden module |
| `857a17a9` | refactor: harden warbee module lifecycle |
| `6758054b` | refactor: harden warthread module lifecycle |
| `96901229` | refactor: move zigbee bitmaps out of header |
| `d861d64f` | build: add flash-fit and SDR image checkers; grow factory partition |
| `88d13fb7` | ci: extend host-test matrix |
| `47feae79` | docs: document SDR integration and RF spectrum features |
| `fe1e0223` | feat: make Applications SDR a native I/Q capture app, drop slot handover |
| `f4f780e1` | feat: replace boot_sdr console command with sdr_capture |
| `09e3a77d` | fix: gate SDR menu entry only on CONFIG_SDR_ENABLE |
| `0411c22c` | fix: remove SDR reboot action from web dashboard |
| `0813ff1c` | feat: SDR_ENABLE default y for the native capture app |
| `ff3cd744` | build: drop dual-boot flasher and image validators |
| `33e6940a` | docs: rewrite SDR integration for the native in-app port |
| `c4eafa7f` | build: drop sdr_check dual-image target |
| `28cbc76d` | build: add a helper to read Kconfig options from the sdkconfig |
| `8d4173ce` | build: let the components drop out of a build variant |
| `b41d7803` | fix: keep the C6 capture bank out of the DRAM image and the heap |
| `cf580c36` | build: add the sdkconfig of the SDR build variant |
| `012b5253` | docs: explain the SDR build variant and the capture bank |
| `ba55fa02` | build: free 94 KB of SRAM for the capture bank |
| `46d235a1` | build: trim the static data of the apps to fit below the capture bank |
| `7a52b727` | build: shrink the buffers of the file tree, the logs and the SSID list |
| `1df8bc9e` | feat: the SDR app shares the firmware with everything else |
| `5c4b8c5c` | docs: the single firmware, the capture bank and the missing TX |
| `d5deb48d` | fix: report the real error when the SD bus does not come up |
| `e26111bd` | build: describe what SDR_ENABLE does in the configuration |
| `161643b6` | feat: add an sdtest command to mount the card and report what happened |
| `a3e1809c` | fix: give the file system slot back when the mount fails |
| `9cabe192` | fix: mount the microSD before the first capture |

### 20.4 Del 20 de octubre de 2026 — el cierre del SDR

| Commit | Asunto |
|---|---|
| `510f4195` | build: link the CW tone without the certification-test rxtest machinery |
| `ff9877b8` | feat: provide the two symbols the CW tone needs from the archive |
| `bf66e068` | feat: the SDR transmits again, through the certification-test tone |
| `06d240c3` | docs: the CW tone comes from the certification-test archive, minus the rxtest |
| `42bf30dd` | feat: the driver remembers its channel and reports a failed start |
| `6b313971` | fix: the Wi-Fi driver comes back up after the Bluetooth took the radio |
| `c4f07def` | fix: mounting the card twice left a second file system behind |
| `cca3cc43` | build: two subscribers per topic is enough for the event bus |
| `ddf071ca` | fix: the flock app left the Bluetooth stack up when it exited |
| `50496e10` | refactor: the apps ask the driver for its channel before moving it |
| `7ce2761d` | fix: the flock detector app had no exit callback |
| `9e8c715e` | fix: only the 64 KB the dump engine writes are taken off the heap |
| `bce64f44` | feat: the SDR is receive-only, the tone command is gone |
| `10a808ab` | build: the certification-test archive is not linked any more |
| `a787b665` | docs: the SDR is receive-only and the capture bank is 64 KB |

---

## 21. Cambios sin commitear

Hay **dos** modificaciones en el árbol de trabajo que no están en ningún
commit:

```
$ git status
	modified:     firmware/main/modules/esp_sdr/c6_sdr_rx.c
	modified:     firmware/main/modules/esp_sdr/esp_sdr_memory.h
```

### 21.1 El banco de captura vuelve a 128 KiB

```diff
--- a/firmware/main/modules/esp_sdr/esp_sdr_memory.h
+++ b/firmware/main/modules/esp_sdr/esp_sdr_memory.h
@@ -23,7 +23,7 @@ extern "C" {
  * what the BLE controller needs: it asks for one large contiguous block
  * and 64 KiB of slack is the difference between fitting and not.
  */
-#define SDR_CAPTURE_BANK_SIZE 0x10000u
+#define SDR_CAPTURE_BANK_SIZE 0x20000u
```

**Qué implica.** La constante pasó de 64 KiB a 128 KiB en el commit
`9e8c715e` ("only the 64 KB the dump engine writes are taken off the heap"),
que fue el que arregló el escaneo BLE:

> Reservar el bloque entero (128 KB) costaba el bloque contiguo más grande del
> heap: 113 KB, y el controlador de Bluetooth del C6 pide unos 121 KB
> contiguos, así que el escaneo BLE moría con
> `bta_dm_ble_scan start scan failed. status=0x3` y
> `HCI_ERR_MEM_CAPACITY_EXCEEDED`.

**El cambio sin commitear deshace ese arreglo.** Vuelve a reservarse el
bloque entero y el heap se queda con 113 KB, por debajo de los ~121 KB que
pide el controlador de Bluetooth. **El escaneo BLE volverá a morir** con el
mismo error.

Además, el comentario que hay justo encima de la constante **sigue
describiendo el estado anterior**:

```c
/*
 * Size of the capture bank.
 *
 * The dump engine writes the I/Q words at the front of the block it owns
 * and stops at the programmed count: 16380 words is 65520 bytes, so the
 * first 64 KiB of the block is all the engine ever touches. The rest of
 * the block stays out of the image but goes back to the heap, which is
 * what the BLE controller needs: it asks for one large contiguous block
 * and 64 KiB of slack is the difference between fitting and not.
 */
```

Y el commit `a787b665` se titula literalmente *"docs: the SDR is receive-only
and the capture bank is 64 KB"*.

El `_Static_assert` de `c6_sdr_rx.h:23-24` sigue compilando (sólo exige
`16380 * 4 <= tamaño`), así que **el error no se ve hasta que se usa el
Bluetooth**.

**Para decidir:** si el objetivo es capturar más de 64 KiB por burst, el
`C6_SDR_MAX_SAMPLES` (16380) tiene que subir también, y el banco de 128 KiB
tiene que caber en la SRAM del C6 — lo cual, según el argumento de `9e8c715e`,
es incompatible con el escaneo BLE. Si el objetivo es otro, conviene
revertir a `0x10000u`.

### 21.2 El `#include` de FreeRTOS

```diff
--- a/firmware/main/modules/esp_sdr/c6_sdr_rx.c
+++ b/firmware/main/modules/esp_sdr/c6_sdr_rx.c
@@ -12,6 +12,7 @@
 #include "esp_timer.h"
 #include "esp_wifi.h"
 #include "soc/soc.h"
+#include "freertos/FreeRTOS.h"
 #include "wifi_controller.h"
```

El fichero usaba tipos de FreeRTOS (`portMUX_TYPE`, funciones de sección
crítica) sin incluir su cabecera, probablemente porulkitsmo de compilación
(la cabecera llega transitivamente por otro camino en algunas versiones de
IDF). El *include* explícito es la forma correcta, pero conviene revisar si
quedan tipos de FreeRTOS sin cabecera propia en el mismo fichero.

---

## 22. Roturas detectadas: estado real de la suite

> **Actualizado 2026-10-21.** Las roturas 22.1 a 22.4 y la 22.6 están
> **corregidas** en el mismo commit que añade la web de ESPARGOS (§23), y la
> 22.3 también. La 22.5 no es una rotura: es una condición de la suite.
> El texto de abajo se conserva tal como estaba, con la nota de lo que se
> corrigió en cada una, porque el diagnóstico sigue siendo válido.

Esta sección documenta lo que **falla hoy** en el branch. Se ha verificado
ejecutando cada suite.

Esta sección documenta lo que **falla hoy** en el branch. Se ha verificado
ejecutando cada suite.

### 22.1 `event_bus`: 4 de 11 tests fallan — y `EV_WIFI` y `EV_GPS` están muertos

> **Corregido.** La centinela de "este tema no tiene tabla" ya no es
> `EVENT_MAX_SUBS` sino `EVENT_SLOT_INVALID` (`0xff`), así que con dos
> suscriptores por tema los temas `EV_WIFI` (slot 2) y `EV_GPS` (slot 3) ya
> no se rechazan. 11 de 11 tests pasan.

**Causa.** `_topic_slot()` usa `EVENT_MAX_SUBS` como centinela de "slot
inválido":

```c
// event_bus.c:32-38
static uint8_t _topic_slot(event_topic_t topic) {
  if (topic < 0 || topic >= EV_APP_START) {
    return EVENT_MAX_SUBS;  /* centinela: siempre rechazado por el llamante */
  }
  uint8_t idx = s_topic_index[topic];
  return (idx < MAX_TOPICS) ? idx : EVENT_MAX_SUBS;
}
```

Y todos los llamantes rechazan el resultado con `if (idx >= EVENT_MAX_SUBS)`.
Con `EVENT_MAX_SUBS == 2` (el valor del commit `cca3cc43`), **el valor 2 es un
índice de slot válido**: `event_bus_init()` asigna

```c
  s_topic_index[EV_INPUT] = s_next_topic++;   /* 0 */
  s_topic_index[EV_MENU]  = s_next_topic++;   /* 1 */
  s_topic_index[EV_WIFI]  = s_next_topic++;   /* 2  <-- indistinguible de la centinela */
  s_topic_index[EV_GPS]   = s_next_topic++;   /* 3  <-- fuera de rango */
```

Así que `EV_WIFI` y `EV_GPS` se rechazan en silencio.

**Resultado:**

```
$ make -C firmware/components/event_bus/test run-tests
test_event_bus.c:74:test_topics_do_not_cross_trigger:FAIL: Expected TRUE Was FALSE
test_event_bus.c:96:test_dispatch_without_init_still_isolates_topics:FAIL: Expected TRUE Was FALSE
test_event_bus.c:131:test_unsubscribe_stops_delivery:FAIL: Expected TRUE Was FALSE
test_event_bus.c:188:test_subscriber_may_reenter_the_bus:FAIL: Expected TRUE Was FALSE
-----------------------
11 Tests 4 Failures 0 Ignored
FAIL
```

**Impacto en el firmware.** `wifi_service` publica `EV_WIFI`
(`wifi_service.c:42`), `wifi_app.c`, `captive_module.c` y
`OTA/modules/wifi_ap/wifi_ap.c` también. **Ninguno llega a ningún
suscriptor**: las siete llamadas a `wifi_service_on_state_change(...)` son
no-ops. En la práctica el bus está muerto salvo para `EV_INPUT` y `EV_MENU`,
que nadie usa.

**Arreglo posible.** Desacoplar la centinela del tamaño de la tabla:
devolver `0xFF` y comparar contra `0xFF`, o comparar el slot contra
`MAX_TOPICS` en vez de contra `EVENT_MAX_SUBS`.

> Los binarios de test que hay en `test/build/` están en `.gitignore` pero
> **obsoletos** (del 19 de octubre, compilados con `EVENT_MAX_SUBS == 8`), y
> por eso el fallo no se notó.

### 22.2 `gap_dispatcher`: la suite no compila

> **Corregido.** La suite tiene su propio `mock/sdkconfig.h` con
> `CONFIG_BT_ENABLED`, que es lo que el componente incluye. 9 de 9 tests
> pasan.

```
$ make -C firmware/components/gap_dispatcher/test run-tests
../gap_dispatcher.c:1:10: fatal error: sdkconfig.h: No existe el fichero o el directorio
    1 | #include "sdkconfig.h"
../gap_dispatcher.h:19:10: fatal error: sdkconfig.h: No existe el fichero o el directorio
   19 | #include "sdkconfig.h"
```

El commit `8d4173ce` añadió `#include "sdkconfig.h"` a `gap_dispatcher.c` y
`gap_dispatcher.h` para proteger el código con `CONFIG_BT_ENABLED`, pero el
`Makefile` de la suite no tiene ningún camino de *include* a un `sdkconfig.h`
generado. La suite está **completamente rota** desde ese commit.

### 22.3 `make tests` invoca un fichero que no existe

> **Corregido.** La línea que invocaba `tools/test/test_sdr_patch.py` se quitó
> del `Makefile` y del fichero de la CI: el parcheador ya no existe desde
> que el SDR es nativo.

`firmware/Makefile:82`:

```make
	python3 tools/test/test_sdr_patch.py
```

`tools/test/test_sdr_patch.py` fue borrado por `fe1e0223` (el parcheador del
*checkout* de esp-sdr dejó de hacer falta con el port nativo). El objetivo
`tests` termina con error en ese paso, y la CI tiene el mismo paso.

### 22.4 `menus_module`: la suite no compila

> **Corregido.** El `mock/surveillance_module.h` declara ahora
> `surveillance_module_begin_flock()` y `surveillance_module_stop()`, que el
> catálogo usa y el *stub* definía sin declarar. La suite compila y pasa en
> sus cinco variantes, incluida la nueva `no-stacks`.

```
$ make -C firmware/main/modules/menus_module/test run-tests
../menus_data.c:217:20: error: 'surveillance_module_stop' no se declaró aquí (no en una función)
  217 |      .on_exit_cb = surveillance_module_stop,
```

El *stub* `menu_callbacks_stub.c:54` **define** `surveillance_module_stop`,
pero el *mock* `mock/surveillance_module.h` **no lo declara**, así que
`menus_data.c` no lo ve.

### 22.5 `surveillance_detect`: necesita `IDF_PATH`

La suite usa `$(IDF_PATH)/components/unity/unity/src` y no tiene *fallback*
a una copia vendorizada:

```
$ make -C firmware/components/surveillance_detect/test run-tests
cc1: fatal error: /components/unity/unity/src/unity.c: No existe el fichero o el directorio
```

Con `IDF_PATH` exportado (`. $IDF_PATH/export.sh`) funciona. Es lo que hace
la CI.

### 22.6 `hid_device`: la suite no está en la CI

> **Corregido.** La suite se añadió como un paso más de la CI.

`firmware/main/apps/ble/hid_device/test` (11 tests) compila y pasa, pero
**no aparece en `.github/workflows/host-tests.yml`**, aunque `firmware/Makefile:78`
la invoca. La CI no la ejecuta.

### 22.7 Resumen

| Suite | Antes | Ahora |
|---|---|---|
| `surveillance_detect` | ⚠️ 80/80, pero necesita `IDF_PATH` | ⚠️ igual (no es una rotura) |
| `nmea_parser` | ✅ 22/22 | ✅ 22/22 |
| `drone_id` | ✅ 27/27 | ✅ 27/27 |
| `files_ops` | ✅ 23/23 | ✅ 23/23 |
| `gap_dispatcher` | ❌ No compila | ✅ 9/9 |
| `radio_lease` | ✅ 12/12 | ✅ 12/12 |
| `event_bus` | ❌ 4 de 11 fallan | ✅ 11/11 |
| `bt_spam` | ✅ 13/13 | ✅ 13/13 |
| `resistor_detector` | ✅ 12/12 | ✅ 12/12 |
| `cmd_control` | ✅ 16/16 | ✅ 16/16 |
| `esp_sdr` | ✅ 23/23 | ✅ **47/47** (23 del protocolo osmoSDR + 24 del protocolo de ráfaga) |
| `menus_module` | ❌ No compila | ✅ 13/13 en 5 variantes |
| `hid_device` | ✅ 11/11 (fuera de la CI) | ✅ 11/11 (en la CI) |
| `rf_spectrum` | ✅ 11/11 | ✅ 11/11 |
| `general_scrolling_text` | ✅ 12/12 | ✅ 12/12 |
| `tools/test/test_flash_tools.py` | ✅ 5/5 | ✅ 5/5 |
| `tools/test/test_sdr_patch.py` | ❌ El fichero no existe | *Elimado del `Makefile` y de la CI* |

**389 tests, todos en verde.**

### 22.8 Otras imprecisiones documentales detectadas

| Sitio | Afirmación | Realidad |
|---|---|---|
| `docs/esp-sdr-integration.md:48` y `:209` | los ficheros son `sdrcap_NNNN.iq` | `files_ops.c:320` usa `"%s%02d"`: son `sdrcap00.iq`, `sdrcap01.iq`… |
| `docs/esp-sdr-integration.md:119` | "16 → 4 por tema, 80 → 60 temas" | los temas son 160 → 140 (`EV_APP_START + 32` → `EV_APP_START + 12`); hoy son 2 suscriptores por tema |
| `docs/esp-sdr-integration.md:125-138` | el banco reservado son 64 KiB | sigue siendo cierto en el commit, pero el árbol de trabajo lo vuelve a poner en 128 KiB (§21.1) |
| `README.md:166` | `make -C firmware sdr_check` | el objetivo se borró en `c4eafa7f` |
| `firmware/tools/test_live_sdr.py` | envía `boot_sdr` | el comando se sustituyó por `sdr_capture` en `f4f780e1` |
| `firmware/components/wifi_service/wifi_service.c:47-97` | elige el modo según `CONFIG_STA_ENABLE`/`CONFIG_AP_ENABLE` | esos símbolos no existen en ninguna parte: siempre cae en la rama de reserva (STA) |
| `firmware/main/apps/surveillance/CMakeLists.txt:1` | `include("${MININO_KCONFIG_CMAKE}")` | `MININO_KCONFIG_CMAKE` no está definido en ninguna parte (el fichero hoy es inerte: `main` no es un componente) |

---

## 23. Conectar el SDR con la web de ESPARGOS

> **Añadido 2026-10-21.** El navegador de ESPARGOS
> (<https://espargos.net/espsdr/app/>) es un analizador de espectro y
> cascada que habla con el firmware del ESP-SDR por USB. Esta sección
> explica el protocolo, el módulo que lo implementa y lo que hubo que
> arreglar del branch para que la imagen volviera a enlazar.

### 23.1 El problema: la web no habla el protocolo que Minino implementa

Minino ya tenía un protocolo de SDR: `osmosdr_serial.c` y `esdr_proto.h`
(446 + 132 líneas), el **binario** `ESRL` que usa el driver SoapySDR para
escritorio. El firmware no lo usaba para nada.

La web de ESPARGOS habla **otro** protocolo: el de línea del firmware
*burst* del ESP-SDR (el `docs/protocol.md` del repo del ESPARGOS), que es texto con
cargas binarias protegidas con CRC32:

```
SYNC <nonce>   -> SYNC <nonce>
INFO           -> C6SDR 6 burst 16380
CAPS           -> CAPS VERSION RXLIMITS HWAGC IQ8 SERIALLEASE TUNEEXT RX40 RX16
VERSION?       -> VERSION {"revision":"unknown",...}
TRANSPORT?     -> TRANSPORT USB 0
RELEASE        -> OK
FREQ <MHz>     -> OK
LIMITS?        -> LIMITS {"gain":[0,0,1],"bandwidth":null,"rates":[...],...}
RANGE?         -> RANGE 2412 2472 1
CAP16 <n> <r>  -> DATA <n> <crc32hex> <us>  +  2*n bytes
CAP20 <n> <r>  -> DATA <n> <crc32hex> <us>  +  ceil(20*n/8) bytes
```

El navegador aplica expresiones regulares exactas sobre cada respuesta
(`radio.js`), así que el formato no es negociable:

```js
// radio.js:736
const m = /^(ESP32|C2|C3|C5|C6|C61|H2|S2|S3|S31)SDR 6 burst (\d+)$/.exec(identity);
if (!m || !Number.isInteger(n) || n < 4096 || n > 16384) throw Error('Unsupported SDR firmware: ' + identity);

// radio.js:605
if (!t || (t[1] === 'UART' ? Number(t[2]) !== this.baudRate : Number(t[2]) !== 0))
  throw Error('Invalid serial transport response');

// radio.js:1000
const h = /^DATA (\d+) ([0-9a-fA-F]{1,8}) (\d+)$/.exec(header);
```

### 23.2 Tres detalles del formato que hay que acertar

**a) El layout de la palabra cruda.** El motor de *dump* escribe una
palabra de 32 bits por muestra:

```
 31        28 27        20 19        10 9         0
+------------+------------+------------+------------+
|  (usado)  | índice de |     Q      |     I      |
|            | ganancia  | (10 bits)  | (10 bits)  |
+------------+------------+------------+------------+
```

**I en los bits 9-0 y Q en los bits 19-10**, los dos con signo. El
navegador desempaqueta los 20 bits de cada par con **I en los bits 0-9 y
Q en los bits 10-19** (`radio.js:98-113`), que es justo el orden que ya
tiene la palabra cruda, y para el formato de 8 bits espera **I y luego Q**
(`radio.js:95-100`).

> **Esto contradice a `esp-sdr-zephyr`**, cuyo `esp_sdr_rx_i()` saca I de
> los bits 19-10 y Q de los 9-0: el layout espejado. La referencia que
> manda aquí es el diagrama de `docs/how-it-works.md` del repo del ESPARGOS y el
> propio `main/common/spectrum.c` del upstream, que leen I de los bits
> 9-0. Los dos proyectos usan los mismos registros, así que el hardware
> no puede dar las dos cosas.

**b) El CRC32.** El navegador comprueba la carga con el `crc32` de zlib.
`esdr_crc32()` lo implementa con una tabla de 16 entradas (nibble a
nibble), que son 64 bytes de flash en vez de los 1024 de la tabla
completa: en una build que pelea por cada byte de heap, la diferencia
importa.

**c) Las tasas.** El motor de captura del C6 acepta un divisor de reloj
en los bits 15-16 del registro de control: los códigos de tasa 0, 1 y 6
dan 80, 40 y 16 MS/s. Minino sólo hacía 80 MS/s; ahora
`c6_sdr_rx_capture_rate()` pone el divisor.

### 23.3 Los dos módulos nuevos

```
firmware/main/modules/esp_sdr/
├── esdr_burst_proto.h   210 líneas  el protocolo: C puro, sin ESP-IDF
├── esdr_burst_proto.c   380 líneas  parser, empaquetado, CRC32, respuestas
├── esdr_host.h           30 líneas  el modo host
└── esdr_host.c          340 líneas  la sesión sobre el USB
```

**`esdr_burst_proto.c`** es C puro a propósito: el formato del cable es
lo único que hay que acertar, y así se puede comprobar en el host sin
placa. Las 24 pruebas de `test_esdr_burst_proto.c` fijan:

| Prueba | Qué fija |
|---|---|
| `test_the_identity_is_the_one_the_viewer_expects` | `C6SDR 6 burst 16380` |
| `test_the_caps_only_advertise_what_is_implemented` | que no se anuncie GPIO, SPEC, GAIN ni ALPF: el navegador enseña un control por cada capacidad |
| `test_the_limits_are_the_ones_the_viewer_validates` | la forma del JSON que el navegador valida |
| `test_the_transport_is_usb_without_a_baud_rate` | `TRANSPORT USB 0` |
| `test_the_eight_bit_format_is_i_then_q` | I y luego Q, los 8 altos de cada campo de 10 bits |
| `test_the_ten_bit_format_decodes_the_way_the_viewer_does` | el grupo de 20 bits, con la extensión de signo que hace el navegador |
| `test_the_crc_is_the_one_the_viewer_computes` | los vectores de zlib |

**`esdr_host.c`** es la sesión. Tres cosas tienen que alinearse:

- **Un solo lector del puerto.** El comando de consola `sdr_host` corre
  *dentro* de la tarea de la consola, así que la consola está entre dos
  lecturas cuando la sesión empieza: no hay dos lectores peleándose por
  el buffer del driver USB Serial/JTAG. (La VFS de IDF y el driver
  comparten el mismo anillo: ver
  `components/esp_driver_usb_serial_jtag/src/usb_serial_jtag_vfs.c`,
  "VFS uses USB-SERIAL-JTAG driver part".)
- **Una consola muda.** Todos los `ESP_LOGx` salen por el mismo puerto y
  una línea de log en mitad de una captura binaria la corrompe, así que
  el **nivel maestro** de log baja a `ESP_LOG_NONE` mientras dura la
  sesión. Se usa el nivel maestro y no el predeterminado porque los
  niveles por etiqueta sobreviven a un cambio del predeterminado.
- **Una radio que nadie más toca.** El motor de captura toma el PHY del
  Wi-Fi, así que el driver pide una *lease* y sólo la devuelve cuando el
  último tenedor la suelta. Sin tráfico durante 5 s, la radio vuelve al
  grupo; sin tráfico durante un minuto, la sesión termina y la consola
  vuelve.

El comando de consola:

```
minino> sdr_host
Serving the ESP-SDR protocol on the USB port.
Open https://espargos.net/espsdr/app/ and connect to this board.
The console comes back when the viewer sends RELEASE, or type MININO.
```

### 23.4 El protocolo binario `osmosdr_serial.c` sigue ahí

No se ha quitado: es el protocolo del driver SoapySDR y sirve para otra
cosa. Pero ahora hay **dos** protocolos de SDR en el árbol, y el binario
sigue sin usar el firmware. La `esdr_burst_proto.h` deja escrito de dónde
sale cada cosa:

```c
/* Ported from ESPARGOS/esp-sdr (GPL-3.0-or-later): main/common/burst_commands.h,
 * main/common/burst_capture.h, main/common/burst_limits.h and docs/protocol.md.
 * The wire format is fixed by the viewer, so this is a port and not a
 * reimplementation. */
```

### 23.5 Lo que hubo que arreglar para que la imagen volviera a enlazar

Aquí está lo gordo: **el branch no enlazaba**. Con el banco de captura
reservado, el *linker script* limita la imagen a 256 KB y la imagen real
medía 301 KB:

```
ld: SDR: the DRAM image overlaps the capture bank
```

Y el error salía por dos sitios distintos, según el estado del
directorio de build:

| Síntoma | Causa |
|---|---|
| `bt_gattc.h:4:10: fatal error: esp_bt.h: No such file or directory` | el helper de Kconfig leía un `sdkconfig` que en una build desde cero todavía no existe |
| `argtable_ext.h:15:10: fatal error: argtable3/argtable3.h: No such file or directory` | un directorio de build viejo, con el árbol de componentes de otra generación |

**El arreglo del helper.** `minino_kconfig_enabled()` leía el `sdkconfig`
del build, que IDF escribe en el paso de Kconfig, **después** de expandir
el árbol de componentes. En una build desde cero el fichero no existe
todavía, así que la función devolvía "apagado" y los componentes se
registraban sin sus requisitos. Ahora lee dos fuentes:

1. el `sdkconfig` del build, si existe (tiene el valor resuelto de todo);
2. la cadena de `SDKCONFIG_DEFAULTS`, que siempre existe porque la build
   la pasa por línea de comandos.

Y como los símbolos de ESP-IDF (`CONFIG_BT_ENABLED`) no están en los
ficheros de defaults, el gate del Bluetooth pasó al símbolo propio del
proyecto:

```cmake
# Kconfig.projbuild
config MININO_BT_STACK
        bool "Enable Bluetooth stack"
        default y
```

```cmake
# los siete componentes BLE
minino_kconfig_enabled(MININO_BT_STACK _minino_opt_on)
```

Los tres interruptores de Minino (`MININO_BT_STACK`,
`MININO_ZIGBEE_STACK`, `MININO_OPENTHREAD_STACK`) quedan fijados en
`sdkconfig.defaults`, que es donde el helper los busca.

**El arreglo del menú.** El catálogo de menús compilaba las entradas de
Zigbee y de Thread con `CONFIG_ZIGBEE_APPS_ENABLE` y
`CONFIG_THREAD_APPS_ENABLE`, pero los módulos que las atienden son los
que una variante deja fuera. Con las pilas apagadas, el enlace fallaba
con referencias sin resolver:

```
undefined reference to `zigbee_module_switch_enter'
undefined reference to `open_thread_module_sniffer_enter'
undefined reference to `warbee_module_begin'
```

Ahora las entradas siguen a la misma puerta que el filtro de fuentes:

```c
#if defined(CONFIG_ZIGBEE_APPS_ENABLE) && defined(CONFIG_MININO_ZIGBEE_STACK)
...
#if defined(CONFIG_THREAD_APPS_ENABLE) && defined(CONFIG_MININO_OPENTHREAD_STACK)
```

**La variante SDR.** Con todo dentro (Bluetooth, Zigbee, Thread, las apps
de vigilancia) la imagen no cabe bajo el banco de captura: 45 KB de más.
El propio Kconfig del branch ya lo decía:

> Enable the Zigbee/Thread stack (espressif__esp-zigbee-lib, openthread,
> ieee802154). Disabling this frees ~30+ KB of DRAM **and is required for
> the SDR build variant**.

Así que hay **dos imágenes**, y cada una con su perfil:

| Variante | Perfil | Configuración | Tamaño | Margen |
|---|---|---|---|---|
| Placas (Minino, BSides, …) | `profiles/<placa>/profile` | todo dentro, **SDR apagado** | 3377 KiB | 8,7 % |
| SDR | `profiles/sdr/profile` | Zigbee y Thread fuera, **SDR encendido** | 2496 KiB | 32,5 % |

```bash
make builds        # las placas
make build-sdr     # la variante del SDR
```

La variante del SDR es la que tiene el banco de captura reservado, el
*linker script* activo y la entrada `Applications > SDR` en el menú. Las
placas normales no reservan el banco, así que el `.bss` y el heap pueden
crecer hasta el final de la SRAM.

**Consecuencia para el usuario.** El `sdr_capture` (la app que graba en la
microSD) y el `sdr_host` (la sesión con el navegador) viven en la variante
del SDR. En la imagen de las placas siguen compilados pero no hay banco de
captura detrás, así que **no funcionan**: por eso `CONFIG_SDR_ENABLE` está
apagado por defecto en `sdkconfig.defaults`.

### 23.6 Lo que encontró la placa: tres fallos que el host no ve

Todo lo anterior se comprobó contra un ESP32-C6 real (rev v0.2,
`48:f6:ee:c2:a2:14`, 8 MB) replicando la lógica de `radio.js` byte a byte.
Salieron tres fallos que ningún test de host puede ver:

**a) La carga binaria no llegaba.** El driver del USB Serial/JTAG mete la
carga en un anillo de 256 bytes con `xRingbufferSend()`, y **esa llamada no
acepta un bloque mayor que el anillo**: se queda esperando un hueco que no va
a llegar y devuelve 0 sin escribir nada. Con una llamada de 32 KB, la
respuesta `DATA ...` llegaba y **los bytes de la carga, no**. El navegador se
habría quedado esperando el CRC para siempre.

```c
/* esdr_host.c: la carga va a trozos del tamaño del FIFO del hardware */
#define ESDR_TX_CHUNK 64
...
  size_t chunk = len - done;
  if (chunk > ESDR_TX_CHUNK) chunk = ESDR_TX_CHUNK;
  int written = usb_serial_jtag_write_bytes(p + done, chunk, pdMS_TO_TICKS(2000));
```

Es el mismo motivo por el que el firmware del ESPARGOS escribe a trozos de
64 bytes con el FIFO de bajo nivel.

**b) La captura era una constante.** Con el driver del Wi-Fi en modo STA (que
es como lo arranca Minino), la captura salía **idéntica en los 13 canales**:
I = −63, Q = −62, varianza cero. El motor de captura estaba funcionando (el
canario se sustituía y las palabras de guarda quedaban intactas), pero la
entrada del conversor era un nivel fijo.

El upstream lo deja claro en su `app_main()`:

```c
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous(true));
```

Con el driver en STA el radio gestiona su propia vida (se duerme, escanea, se
asocia) y el conversor no ve nada. Con el power save desactivado y el modo
promiscuo puesto, la captura trae señal de verdad:

| Canal | Antes | Después |
|---|---|---|
| 2412 MHz | I = −63, var 0 | I = 130, var 9 090, 372 valores distintos |
| 2447 MHz | I = −63, var 0 | I = 115, var 1 616 |
| 2472 MHz | I = −63, var 0 | I = 25, var 13 901 |

Y el espectro pasa de una línea plana con un pico de DC a 87 dB de rango
dinámico, con el pico en el centro (la portadora del LO).

El modo promiscuo se pide **con la lease**
(`radio_lease_prom_hold(RADIO_PROM_SDR_RX)`), no tocando el driver a pelo,
para que las otras apps que lo comparten no se lo quiten de debajo.

**c) La sesión tiene que ser dueña del radio.** La primera versión liberaba
la *lease* del Wi-Fi a los 5 s de inactividad, para que las demás apps
pudieran usar la radio. El problema es que al volver a tomarla, **el driver
del Wi-Fi se reinicia y escribe sus propios logs desde la ROM**, que no pasan
por `esp_log` y no se callan con el nivel maestro: esa línea de ruido se
colaba en mitad de una captura binaria. Ahora la sesión **posee la radio**
desde que empieza hasta que acaba, y el escape es lo único que la suelta.

Lo mismo explica por qué el driver se pone **antes** de que la sesión
empiece: su ruido de arranque se queda fuera del flujo del navegador.

**d) Lo que no se pudo hacer.** El firmware del ESPARGOS programa el canal
en el PHY con `chip_v7_set_chan()` (en vez de por el driver). En Minino eso
**cuelga la placa**: el driver de Wi-Fi tiene su propio estado del PHY, lo
reinicializa al detectarlo y se queda esperando. Con el driver de Wi-Fi en
marcha no es viable; la alternativa es el camino que ya se usa (el canal por
el driver) más el power save y el promiscuo.

**e) El banco de captura tiene que ser el bloque entero.** El bit de
propiedad de `SRAM_OWNER_REG` es **por bloque**, no por página: mientras la
captura corre, la CPU no puede leer nada de los 128 KiB, ni siquiera de la
mitad que el motor no escribe. Reservando sólo 64 KiB (el valor que estaba
commiteado) el heap puede usar la mitad alta, el driver de Wi-Fi coloca ahí
sus buffers y se quedan **inaccesibles durante la captura**: el driver
espera a sus buffers, el watchdog de tareas salta y la placa reinicia con

```
rst:0x8 (TG1_WDT_HPSYS),boot:0x7c (SPI_FAST_FLASH_BOOT)
```

El síntoma es desconcertante: la captura de 16 380 muestras va bien (el
motor sólo escribe 65 520 bytes) y **cualquier captura más corta cuelga la
placa**.

> Esto corrige el razonamiento que tenía el documento anterior: decía que
> reservar los 128 KiB rompía el Bluetooth porque el bloque contiguo más
> grande del heap quedaba en 113 KB y el controlador del C6 pide 121 KB.
> **Medido en la placa, el Bluetooth funciona con la reserva de 128 KiB**:
> 35 trackers vistos, sin `HCI_ERR_MEM_CAPACITY_EXCEEDED`. El bloque que
> necesita el Bluetooth lo pide por debajo de 0x40840000.

### 23.7 El modo de espectros: la FFT en la placa

El visor de ESPARGOS tiene un segundo modo, el de **espectros calculados
en el chip** ("On-Chip Spectrum"), que es el que refresca a decenas de
cuadros por segundo en vez de a diez. La placa calcula la FFT y manda una
lista de códigos de potencia en vez de las muestras.

El módulo nuevo es `main/modules/esp_sdr/esdr_spectrum.c`, portado del
firmware del ESPARGOS (`main/common/spectrum.c` y compañía).

| Comando | Respuesta |
|---|---|
| `SPECINFO?` | el JSON con los perfiles |
| `SPEC <ms> <paso> <uds> <det> <tasa> <bins>` | `SPEC <bins> <tasa> <bins> <mhz>` y los marcos |
| `DC?` / `DC 0` / `DC 1` | `DC <modo>` / `OK` |

Los perfiles cubren las tres tasas del C6 con 256, 512, 1024 y 2048 bins.

**Lo que costó tres intentos:**

1. **El watchdog se llevaba la placa.** El bucle de espectros no puede
   quedarse con la CPU: la tarea idle es la que alimenta el watchdog de
   tareas, así que hay que cederle el paso (el `vTaskDelay(1)` cada 20 ms
   que hace el upstream).
2. **La FFT estaba mal.** Structurela de tipo DIF con la permutación de
   bits de la entrada, y el bucle de grupos mal acotado (se salía del
   array). Lo cazó el test de host, que la compara con un **DFT directo**:
   un espectro con las bins en orden equivocado parece plausible en
   pantalla y no es la señal.
3. **La escala.** Sin dividir entre dos en cada mariposa, un tono fuerte
   satura el Q15 a la primera etapa. Ahora la ganancia total de la
   transformada es 1/n, como en el upstream.

El marco es el del upstream: 28 bytes de cabecera, las bins en orden
natural y 4 bytes de CRC32, con la firma `SPC1`.

En la placa: **28 de 28** comprobaciones del modo de espectros, con los
marcos llegando con el CRC correcto y las bins con estructura (máximo 146,
mediana 14).

### 23.8 Lo que se puede y no se puede ver desde el navegador

| Capacidad | Estado |
|---|---|
| Espectro y cascada en el navegador | ✅ es lo que hace el modo *burst* |
| Sintonía de 2412 a 2472 MHz | ✅ los canales de Wi-Fi, que es a lo que llega el motor |
| 80, 40 y 16 MS/s | ✅ con el divisor de reloj |
| Formato de 8 bits y de 10 bits | ✅ |
| Ganancia manual | ❌ Minino no la implementa: la capacidad `GAIN` no se anuncia y el navegador esconde el control |
| Ancho de banda analógico | ❌ igual: no se anuncia `ALPF` |
| GPIO (conmutadores de antena) | ❌ no se anuncia `GPIO` |
| Espectros calculados en el chip (`SPEC`) | ✅ 256–2048 bins en las tres tasas (ver §23.7) |
| Salida en la banda de 5 GHz | ❌ el C6 no tiene 5 GHz |

El modo `SPEC` es el que convierte el navegador en un analizador en
tiempo real de verdad (espectro *gapless* con la FFT en el chip); el modo
*burst* da una imagen del espectro con huecos entre ráfagas, que es lo que
muestra el "Reception has gaps between snapshots" del propio visor.

### 23.9 Los ficheros

| Fichero | Qué es |
|---|---|
| `firmware/main/modules/esp_sdr/esdr_burst_proto.c/.h` | el protocolo: parser, empaquetado, CRC32, respuestas |
| `firmware/main/modules/esp_sdr/esdr_host.c/.h` | la sesión sobre el USB Serial/JTAG |
| `firmware/main/modules/esp_sdr/esdr_spectrum.c/.h` | los espectros en la placa: la FFT, el marco y el CRC |
| `firmware/main/modules/esp_sdr/esdr_transport.h` | el transporte que comparten los dos módulos |
| `firmware/tools/test_esdr_spec.py` | la prueba del modo de espectros contra la placa |
| `firmware/tools/test_esdr_radio.py` | la prueba sobre el aire: una placa transmite y la otra recibe con el SDR |
| `firmware/main/modules/esp_sdr/test/test_esdr_burst_proto.c` | 24 pruebas de host del formato del cable |
| `firmware/main/modules/esp_sdr/c6_sdr_rx.c/.h` | el divisor de reloj (40 y 16 MS/s), el canario de overrun, el redondeo a canal |
| `firmware/main/modules/cmd_control/cmd_control.c` | el comando `sdr_host` |
| `firmware/tools/test_esdr_web.py` | la prueba contra la placa que replica al navegador |
| `firmware/sdkconfig.sdr`, `firmware/profiles/sdr/profile` | la variante del SDR |
| `firmware/cmake/minino_kconfig.cmake` | el helper, ahora con la cadena de defaults |
| `firmware/components/minino_config/Kconfig.projbuild` | el símbolo `MININO_BT_STACK` |
| `firmware/main/modules/menus_module/menus_data.c` | las puertas del catálogo, alineadas con el filtro de fuentes |
| `firmware/components/event_bus/event_bus.c/.h` | la centinela del bus, que ya no choca con `EVENT_MAX_SUBS` |

### 23.10 Cómo se usa

```bash
# 0. (opcional) la prueba automática contra la placa, que replica la
#    logica del navegador byte a byte:
python3 tools/test_esdr_web.py /dev/ttyACM0

# 1. flashear la variante del SDR
make build-sdr
cd build-sdr && esptool.py write_flash 0x0 minino.bin

# 2. abrir una terminal en el puerto USB (por ejemplo /dev/ttyACM0)
#    y handing over the port
minino> sdr_host

# 3. abrir https://espargos.net/espsdr/app/ y elegir el dispositivo
#    (WebSerial o WebUSB). El navegador manda SYNC, INFO, CAPS, LIMITS?,
#    FREQ y CAP16/CAP20 a partir de ahí.

# 4. el visor manda RELEASE al cerrar: la consola vuelve.
```

> **El botón "Flash Firmware…" del navegador flashea el firmware del
> ESPARGOS y borra Minino.** Hay que ignorarlo.

### 23.11 Lo que falta para que sea cómodo

- **La entrega del puerto es manual.** Hay que escribir `sdr_host` en una
  terminal antes de que el navegador pueda conectarse. El REPL hace eco de
  lo que lee (linenoise) y el navegador espera una respuesta limpia, así
  que el REPL no puede estar escuchando a la vez. Se podría resolver con
  un *mux* delante de la línea de entrada, pero es una pieza más.
- **No hay escape desde el lado del visor.** `RELEASE` es lo que manda el
  navegador al cerrarse; `MININO` es el escape propio de Minino y sirve
  desde una terminal.
- **El `SPEC` (FFT en el chip) es lo que falta** para tener un analizador
  en tiempo real: exige el anillo de tres bancos de SRAM y una FFT, que es
  justo lo que el modo *burst* evita.

### 23.12 La prueba sobre el aire

> **Añadido 2026-10-21.**

Con dos placas en la mesa (una radiate, la otra en recepción del SDR) se
cerró la parte de la verificación que faltaba. El detalle está en la §9 de
[`esp-sdr-integration.md`](./esp-sdr-integration.md) y el informe completo,
en [`sdr-resultados.md`](./sdr-resultados.md).

| Prueba | Resultado |
|---|---|
| `tools/test_esdr_web.py` en la placa de recepción, con la otra radiando | 56 de 56 |
| `tools/test_esdr_spec.py` en la placa de recepción, con la otra radiando | 28 de 28 |
| `make tests` (las 20 suites de host) | 0 fallos |
| La app nativa: ráfaga, sintonía y salida | 16 380 palabras en 218 us |

**Dos fallos que destapó la prueba:**

1. El spammer de SSID de Wi-Fi, lanzado desde la consola, **no
   transmitía nada**. La lista de SSID se carga al elegir un elemento del
   menú (`ssid_spam.c:158`), y desde la consola no hay ninguno elegido:
   la tarea de spam se quedaba con la lista vacía y salía sin_tx. La
   lista se carga ahora al abrir la app (`ssid_spam_load_stored_list()`),
   así que el `Start` del menú funciona siempre y el spammer sirve como
   transmisor para las pruebas.
2. Reentrar en el spammer **reiniciaba la placa**:
   `esp_netif_create_default_wifi_ap()` hace un `assert()` si el netif del
   AP ya existe (*"duplicate key"*). Ahora el netif solo se crea si no
   existe (`esp_netif_get_handle_from_ifkey("WIFI_AP_DEF")`).

La nueva herramienta `firmware/tools/test_esdr_radio.py` mide la
diferencia del espectro con la transmisión puesta y sin ella, alternando
las ventanas y comparando cada una con sus vecinas para que la deriva
del ambiente se cancele.

**Las dos aplicaciones del SDR en las dos placas.** Con la app nativa (la
ráfaga de I/Q) en una Minino y el modo de espectros (la FFT en la
placa) en la otra, y al revés, en el mismo ambiente y sin ninguna otra app
en juego. Lo hace `firmware/tools/test_esdr_pair.py` (modos `app+spec`,
`spec+spec` y `app+app`) y la app nativa suelta la comprueba
`firmware/tools/test_esdr_app.py`.

| Placa | protocolo | espectros | app nativa |
|---|---|---|---|
| `48:f6:ee:c2:a2:14` | 56/56 | 28/28 | 16/16 |
| `7c:2c:67:42:84:a0` | 56/56 | 28/28 | 16/16 |

Con las dos placas midiendo el espectro a la vez, los dos perfiles tienen
**la misma forma** y hasta **14,7 dB de diferencia de nivel**: no es la
placa, es dónde está cada una. El `wifi_scan` de las dos en el mismo
momento lo confirma (los mismos AP con +13 dB en una que en la otra).

**El límite de la medida, medido:** con la diferencia con transmisión
(+0.2 a +1.1 dB, en la banda del transmisor) y el control negativo dando
+1.0 a +1.4 dB en las mismas bandas, el efecto no se separa del ambiente
en un entorno de 2.4 GHz saturado. Lo que sí es concluyente es que la
transmisión *llega*: el receptor de Wi-Fi de la placa de recepción ve los
beacons de la otra a −46 dBm, más fuerte que cualquier AP del entorno.

---

## Anexo: glosario

| Término | Significado |
|---|---|
| **Claim / lease** | Petición de propiedad sobre un recurso global, con recuento de referencias |
| **Fan-out** | Reparto de un evento a varios suscriptores |
| **Banco de captura** | Bloque de SRAM que el motor de *dump* del C6 toma para escribir las muestras |
| **Promiscuo** | Modo del driver de Wi-Fi que entrega todas las tramas |
| **GAP** | *Generic Access Profile* de Bluetooth: advertisement y escaneo |
| **GATTC** | *Generic Attribute Profile Client*: el lado cliente de Bluetooth |
| **BTC task** | La tarea de Bluedroid que ejecuta las operaciones del stack |
| **Host test** | Test que compila en el PC con `gcc` contra *mocks* de ESP-IDF |
| **IRAM** | La RAM de instrucción, donde IDF mete el código crítico |
| **`.bss`** | Variables sin inicializar, que ocupan RAM al arrancar |
| **Ensalzamiento de prioridad** | Una tarea de alta prioridad retrasa a una de baja que la espera |
| **`-zmuldefs`** | Flag del enlazador que acepta definiciones múltiples (aquí, durante mucho tiempo, de datos en cabeceras) |

## Anexo: referencias

| Documento | Contenido |
|---|---|
| [`docs/esp-sdr-integration.md`](./esp-sdr-integration.md) | El SDR en detalle técnico: registros, banco, recortes |
| [`docs/rf-spectrum-advanced.md`](./rf-spectrum-advanced.md) | El analizador de espectro RF |
| [`firmware/README.md`](../firmware/README.md) | Build, perfiles de placa y variantes |
| [`firmware/menus.md`](../firmware/menus.md) | El árbol de menús |
| [ESPARGOS/esp-sdr](https://github.com/ESPARGOS/esp-sdr) | El proyecto de referencia del SDR |
| [esp-sdr-zephyr](../esp-sdr-zephyr/) | El *upstream* con el que se comparó el *linker script* del banco |

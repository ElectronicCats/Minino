# Integración del SDR nativo en Minino

> **Guia de uso:** [`sdr-guia.md`](./sdr-guia.md) (puesta en marcha, las
> tres formas de usarlo).
> **Informe de las pruebas:** [`sdr-resultados.md`](./sdr-resultados.md)
> (las dos placas, las dos aplicaciones, los perfiles del espectro).
> **El cable, byte a byte:** [`sdr-protocolo.md`](./sdr-protocolo.md)
> (los dos paquetes, los bytes reales y dónde se ve cada cosa).

Documento de referencia de la integración entre Minino (ESP-IDF) y el
firmware de radio definida por software. El arranque dual está retirado:
el SDR corre **en proceso** dentro de Minino OS, sin tocar `ota_1`.

- **Fecha:** 2026-10-19 (actualizado el 2026-10-21)
- **Placas usadas:** ESP32-C6 rev v0.1 (`f0:f5:bd:27:6b:00`) y rev v0.2
  (`48:f6:ee:c2:a2:14`), ambas de 8 MB
- **Toolchain:** ESP-IDF v5.5.1, target `esp32c6`
- **Documento hermano:** [`changes-refactor-system-overhaul.md`](./changes-refactor-system-overhaul.md)
  (todo el branch, y en su §23 la conexión con la web de ESPARGOS)

---

## 1. Resumen ejecutivo

`Applications > SDR` abre una aplicación nativa de Minino ("SDR",
`MENU_ESP_SDR`, option `CONFIG_SDR_ENABLE`): burst capture de I/Q con el
motor dump del C6, grabado en la microSD. La interfaz del usuario es la del
propio Minino, sin handover de arranque, sin segunda imagen y sin dejar de
ver la pantalla.

El arranque dual hacia una segunda aplicación en `ota_1` estaba
implementado (ver historial en git) y funcionaba con la imagen nativa
ESP-IDF de [ESPARGOS esp-sdr](https://github.com/ESPARGOS/esp-sdr), pero
fue descartado en favor del port nativo: una sola imagen, una sola
consola, sin riesgo de quedarse sin OS si el payload de `ota_1` se rompe.

> **El SDR necesita su propia variante de build** (ver la sección 5.1). El
> banco de captura se come un bloque entero de SRAM y el *linker script*
> limita la imagen a 256 KB; con la pila de Zigbee y la de Thread dentro,
> la imagen se pasa en 45 KB. Así que hay dos imágenes: la de las placas,
> con todo dentro y el SDR apagado, y la del SDR, con las dos pilas fuera
> y el SDR encendido. Ninguna de las dos toca `ota_1`.

---

## 2. Arquitectura

```
Minino OS (factory / ota_0)
└── main/modules/esp_sdr/           <- app nativa, misma imagen que todo Minino
    ├── c6_sdr_rx.c                 <- motor dump RX (registro directo, 80 MS/s)
    ├── iq_pack.c                   <- empaque I/Q 8/12/16 bits
    ├── osmosdr_serial.c            <- parser del protocolo esdr (esdr_proto.h)
    ├── sdr_screen.c                <- vista OLED del estado
    └── esp_sdr_module.c            <- app del menú: sintonía, captura, save a SD
```

- La radio la maneja `radio_lease`: el RX mantiene un claim promiscuo
  (`radio_lease_prom_hold(RADIO_PROM_SDR_RX)`), así que coexistencia con
  el resto de las apps BLE/Wi-Fi.
- `c6_sdr_rx_init()` arranca el driver Wi-Fi si no está arriba, porque el
  motor dump necesita los relojes y calibración del PHY.
- Captura de un burst hasta 16380 palabras (32 bits: I/Q compactados).
- Los bursts se graban como `sdrcap00.iq`, `sdrcap01.iq`, … en la microSD
  (`/sdcard`); sin SD, la app muestra el burst y pasa del save. El nombre
  lo genera `files_ops_incremental_name_buf()` con el formato `"%s%02d"`
  (`files_ops.c:320`): dos dígitos, hasta 9999 ficheros.

## 3. Estructura de la captura

`SDR_CAPTURE_BANK_BASE` (0x40840000, `main/modules/esp_sdr/esp_sdr_memory.h`)
es el banco de SRAM que hardware escribe directamente al tiempo que el RF
corre; no se toca con la CPU durante la captura. La app hace:

1. TUNE: configura canal Wi-Fi (2407+5n) para pillar calibración, luego
   enciende la cadena de RX con `pbus_workmode()` / `ram_pbus_xpd_rx_on()`.
2. CAPTURE: dispara el DUMP CTRL con el número de muestras y otro grupo
   de registros de configuración; el hardware descarga 80 MS/s al SRAM.
3. VERIFY: canary al inicio del buffer, timeout de 25 ms al DUMP CTRL.
4. SAVE: `iq_pack_raw` no interviene: sin conversión se copian las words
   crudas al fichero. El layout de la palabra es el del motor de dump:

   ```
    31        28 27        20 19        10 9         0
   +------------+------------+------------+------------+
   |  (usado)  | índice de |     Q      |     I      |
   |            | ganancia  | (10 bits)  | (10 bits)  |
   +------------+------------+------------+------------+
   ```

   **I en los bits 9-0 y Q en los bits 19-10**, los dos con signo
   (−512..+511). Ojo: el proyecto `esp-sdr-zephyr` los tiene al revés
   (su `esp_sdr_rx_i()` saca I de los bits 19-10); la referencia que manda
   es el diagrama de `docs/how-it-works.md` del repo del ESPARGOS, que usa los
   mismos registros.

## 4. Uso

### En el dispositivo (`Applications > SDR`)

La aplicación nativa ofrece **4 modos operativos** que se alternan presionando el botón **`BOOT`** o manteniendo presionado **`RIGHT`**:

1. **`[IDL]` / `[RX]` (RX I/Q Burst Capture)**: Captura ráfagas de 8 192 muestras I/Q a 80 MS/s, calcula el RSSI físico en `dBm` y guarda archivos `/sdcard/sdrcapXX.iq` en la microSD.
2. **`[TX:PKT]` (TX 802.11 Raw)**: Envía ráfagas de 5 tramas Probe Request 802.11 (`MININO-SDR-TX`) en el canal seleccionado (`2412-2484 MHz`).
3. **`[TX:CW]` (TX CW Tone)**: Emite una portadora continua de RF usando el sintetizador PHY (`c6_sdr_tx_carrier_start`) con límite de seguridad automático de 30 segundos.
4. **`[HOST]` (ESPARGOS WebSDR Host)**: Conecta directamente con la aplicación web <https://espargos.net/espsdr/app/> por USB Serial/JTAG.

Controles:
- Botones `UP` / `DOWN`: Sintonía `2412` a `2484 MHz` (Canales 1 a 14).
- `RIGHT` (pulsación corta): Captura y guarda un burst en `[RX]`, transmite paquetes en `[TX:PKT]`, o activa/desactiva la portadora en `[TX:CW]`.
- `BOOT` o `Hold RIGHT`: Cambia al siguiente modo operativo.
- `LEFT`: Sale al menú principal, devolviendo la radio a Minino.

> **Activación automática del Web Host:** Si estás dentro de `Applications > SDR` en cualquier modo y abres <https://espargos.net/espsdr/app/> pulsando **Connect**, la consola detecta el comando `SYNC` y cambia automáticamente la app al modo `[HOST]` sin necesidad de escribir comandos ni cambiar de modo manualmente. Además, el registro `USB_SERIAL_JTAG.chip_rst.usb_uart_chip_rst_dis = 1` evita que las señales DTR/RTS del navegador reinicien la placa.

### Por consola

- `sdr_capture`: abre la misma app (idéntica a la entrada del menú).
- `sdr_host`: entrega el puerto USB al protocolo del ESP-SDR, que es el
  que habla el navegador de <https://espargos.net/espsdr/app/>. Ver la
  sección 8.

## 5. El banco de captura: por qué el firmware tiene que caber debajo

El motor dump del C6 no escribe en un buffer cualquiera: toma el bloque 2
de SRAM (0x40840000..0x40860000, 128 KiB) mientras dura la captura, y
escribe en sus primeros 64 KiB. Lo que viva ahí mientras corre el motor:

- lo pisa el hardware (los samples), y
- si la CPU lo toca, el acceso se queda colgado, porque el bloque ya no
  es de la CPU.

El firmware normal de Minino colocaba ahí su `.bss` y su heap: al entrar en
la app SDR, el motor se comía ~121 KiB de variables vivas. El síntoma era
un *Guru Meditation* (`Instruction access fault`, PC en `0x944f00e0`, que
no es código) nada más sintonizar el canal: el heap y el `.bss` ya estaban
corruptos para entonces.

Upstream lo resuelve reservando el bloque en el linker
(`esp-sdr-zephyr/lib/esp_sdr/esp_sdr_bank_c6.ld`). Aquí, con la imagen
de Minino completa, no cabe tal cual: el `.bss` normal ocupa 172 KiB y el
bloque 2 sólo tiene 128 KiB. El firmware único es el resultado de
recortar lo justo para que quepa:

| pieza | qué hace |
|---|---|
| `firmware/main/esp_sdr_bank.ld` | redefine la región `sram_seg` con longitud 0x40000: nada de la imagen puede pasar de 0x40840000, y si no cabe el enlace falla con *"SDR: the DRAM image overlaps the capture bank"* |
| `firmware/main/modules/esp_sdr/esp_sdr_memory.c` | reserva el bloque para el heap con `SOC_RESERVE_MEMORY_REGION()` (el mismo mecanismo que usa IDF), de modo que `heap_caps` no lo reparte |
| `firmware/cmake/minino_kconfig.cmake` | helper para leer opciones de Kconfig desde el `sdkconfig` (ver más abajo) |

### 5.1 La variante de build del SDR

El banco de captura no es sólo un trozo de RAM: es un bloque entero que
sale del heap, y el *linker script* (`esp_sdr_bank.ld`) recorta la
región `sram_seg` para que la imagen quepa por debajo de
`0x40840000`. Con la imagen completa la cosa no cuadra:

| Medida | Valor |
|---|---|
| Presupuesto (los dos bloques de SRAM que quedan bajo el banco) | 256 KB |
| Imagen con Bluetooth, Zigbee y Thread dentro | 301 KB |
| Imagen con las pilas de Zigbee y Thread fuera | 249 KB |

El propio Kconfig del proyecto ya lo decía (`MININO_ZIGBEE_STACK`):

> Disabling this frees ~30+ KB of DRAM **and is required for the SDR
> build variant**.

Así que hay dos perfiles:

| Variante | Perfil | Configuración | Tamaño | Margen de flash |
|---|---|---|---|---|
| Placas | `profiles/<placa>/profile` | todo dentro, `CONFIG_SDR_ENABLE` apagado | 3377 KiB | 8,7 % |
| SDR | `profiles/sdr/profile` | Zigbee y Thread fuera, `CONFIG_SDR_ENABLE` encendido | 2496 KiB | 32,5 % |

```bash
make builds        # las placas
make build-sdr     # la variante del SDR
```

En la variante del SDR es donde están la entrada `Applications > SDR`, el
comando `sdr_capture` y el `sdr_host`. En la imagen de las placas el
código del SDR sigue compilado (la glob de `main` lo compila todo), pero
**no hay banco de captura detrás**: por eso `CONFIG_SDR_ENABLE` está
apagado en `sdkconfig.defaults` y el menú no enseña la entrada.

### Los recortes que lo hicieron posible

La imagen pasó de 374 KB a 255 KB de IRAM+data+bss (el presupuesto son
256 KB, los dos bloques de SRAM que quedan bajo el bank):

| recorte | KB |
|---|---|
| el código caliente del Wi-Fi y la PHY fuera de IRAM (el mismo cambio que hace el proyecto esp-sdr) | 45.6 |
| los spoofers de OpenDroneID al heap, uno por dron configurado | 37.7 |
| `librftest.a` fuera del enlace (el cert-test no hace falta sin TX) | 44.7 |
| la tabla de suscriptores del bus de eventos (16 -> 4 por tema, 80 -> 60 temas) | 14.6 |
| los buffers del log de vigilancia, del árbol de ficheros y de la lista de SSID | 4.5 |
| el detector de vigilancia (200 -> 80 dispositivos, 50 -> 20 trazas) y sus tablas de firmas | 7.0 |
| el dron BLE de OpenDroneID al heap | 2.3 |
| la fuente del OLED a flash (era `const` solo a medias) | 1.0 |

### Se reserva el bloque entero (128 KB), no sólo los 64 KB que se escriben

El motor de dump escribe **16384 palabras (65536 bytes) exactamente** en
0x40840000: medido con una sonda que rellena el bloque de patrón y cuenta
cuántas palabras cambian (las que cambian son 16384, la última es la
16383). El resto del bloque de 128 KB no lo toca nadie: ni el driver de
Wi-Fi, ni el de Bluetooth (comprobado con la misma sonda).

**Aun así, el bloque entero tiene que quedar fuera del heap.** El bit de
propiedad de `SRAM_OWNER_REG` es **por bloque**, no por página: mientras la
captura corre, la CPU no puede leer **nada** del bloque de 128 KB, ni
siquiera la mitad que el motor no escribe. Si el heap puede usar esa mitad
(el caso de reservar sólo 64 KB), el driver de Wi-Fi coloca ahí sus buffers
y se quedan **inaccesibles durante la captura**: el driver espera a sus
propios buffers, el watchdog de tareas salta y la placa reinicia con

```
rst:0x8 (TG1_WDT_HPSYS),boot:0x7c (SPI_FAST_FLASH_BOOT)
```

Es un fallo sutil: la captura de 16 380 muestras va bien (el motor sólo
escribe 65 520 bytes), pero cualquier captura más corta lo dispara, y el
watchdog se lleva la placa por delante.

> **Corrección al texto anterior.** Este apartado decía que reservar los
> 128 KB rompía el Bluetooth (el bloque contiguo más grande del heap
> quedaba en 113 KB y el controlador del C6 pide 121 KB). Medido en la placa
> con la reserva de 128 KB **el escaneo BLE funciona**: 35 trackers vistos,
> sin `HCI_ERR_MEM_CAPACITY_EXCEEDED`. El bloque que necesita el Bluetooth
> lo pide por debajo de 0x40840000, no en la mitad alta del bloque del
> motor.

### El banco, en la placa

Con la reserva de 128 KB:

```
heap_init: At 40833480 len 0000CB80 (50 KiB): RAM    <- por debajo del banco
heap_init: At 40860000 len 0001C610 (113 KiB): RAM   <- por encima del banco
heap_init: At 4087C610 len 00002F54 (11 KiB): RAM
```

El tramo 0x40840000..0x40860000 no aparece en la lista del heap: es del
motor.

### Nota sobre `if(CONFIG_*)` en CMake

ESP-IDF evalúa el `CMakeLists.txt` de cada componente **dos veces**: una
al expandir el árbol de componentes (antes del paso de Kconfig, así que
todavía no existe ninguna variable `CONFIG_*`) y otra al compilar. Un
`if(CONFIG_BT_ENABLED)` a secas se lleva la rama falsa en la primera
pasada, y el componente queda registrado sin fuentes ni requisitos (el
enlace falla con *"bt_gattc.h includes esp_bt.h ... not in the
requirements list of bt_gattc"*).

`firmware/cmake/minino_kconfig.cmake` resuelve eso: lee la opción del
propio fichero `sdkconfig` (disponible en las dos pasadas) y expone

```cmake
include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/minino_kconfig.cmake")
minino_kconfig_enabled(BT_ENABLED _minino_opt_on)
if(_minino_opt_on)
    idf_component_register(SRCS ...)
else()
    idf_component_register()
endif()
```

Las opciones nuevas del proyecto (`MININO_ZIGBEE_STACK`,
`MININO_OPENTHREAD_STACK`) llevan prefijo `MININO_` a propósito: un
símbolo de Kconfig no puede definirse en dos sitios, y
`OPENTHREAD_ENABLED` ya lo define el componente de upstream.

---

## 6. El TX: el C6 no tiene, y se quitó

El C6 **no puede transmitir I/Q**: su motor dump es un receptor (escribe
en SRAM las muestras que la radio demoduló, no las reproduce) y no tiene
DAC de TX (el S3 sí). Lo único que el chip emite es el **modo tono** del
cert-test, `esp_phy_wifi_tx_tone()`, que vive en `librftest.a`.

Se llegó a enlazar ese tono de dos formas (el archivo entero, o sólo
`rf_test.o` + `esp_phy_api.o` con un wrapper propio), pero **no radia**:
la medida con un HackRF da exactamente el mismo espectro con el tono
encendido y apagado. La causa es que el tono del cert-test necesita el
estado completo de *cert test* del PHY, que prepara `rftest_init()` en
`wifi.o`; ese objeto arrastra `mac_common.o` con 45 KiB de `.bss` (los
buffers de IQ del rxtest), que no caben bajo el bank de captura.

En el upstream esp-sdr el C6 tampoco tiene TX: su único backend de
emisión es el DAC del S3.

Así que **el SDR es sólo de recepción** y el comando `rf_tx` se quitó de
la consola, junto con `c6_sdr_tx.c` y el enlace del archivo de
cert-test. El modo TX del protocolo osmoSDR se rechaza con
`-ENOTSUP` en vez de responder con silencio.

## 7. Verificación

```bash
make tests                        # incluye main/modules/esp_sdr/test
# en placa:
#   Applications > SDR -> burst, revisar /sdcard/sdrcap_0001.iq
```

Medido en placa (ESP32-C6 rev v0.2, firmware único con todo):

```
I (6918) c6_sdr_rx: Tuned to 2412 MHz (CH 1)
I (7102) esp_sdr_module: burst 1: 16380 words in 217 us (not saved: no SD)
```

16380 palabras (65 KiB) en 217 us son 76 MS/s, coherente con los 80 MS/s
del motor dump. Antes de reservar el bank, la misma secuencia terminaba en
un Guru Meditation.

Nota de la placa de pruebas: la microSD de ese equipo no responde
("R1 response: command not supported"), asi que el burst no se puede
guardar; la captura en si funciona. El montaje de la SD devuelve
`ESP_ERR_NO_MEM` cuando no la ve, que confunde: el error real es que la
tarjeta no contesta.

---

## 8. El modo host: hablar con el navegador de ESPARGOS

> **Añadido 2026-10-21.**

El navegador de <https://espargos.net/espsdr/app/> (ESP-WebSDR) es un
analizador de espectro y cascada que se conecta al ESP por USB y le pide
ráfagas de I/Q. Habla el **protocolo de línea** del firmware *burst* del
ESP-SDR, que es otro distinto del binario `ESRL` que implementa
`osmosdr_serial.c` (ese es el del driver SoapySDR para escritorio).

### 8.1 El protocolo

```
SYNC <nonce>   -> SYNC <nonce>
INFO           -> C6SDR 6 burst 16380
CAPS           -> CAPS VERSION RXLIMITS HWAGC IQ8 SERIALLEASE TUNEEXT RX40 RX16
VERSION?       -> VERSION {"revision":"unknown","build_date":"...","build_timestamp":"...","profile":"esp32c6"}
TRANSPORT?     -> TRANSPORT USB 0
RELEASE        -> OK
FREQ <MHz>     -> OK
LIMITS?        -> LIMITS {"gain":[0,0,1],"bandwidth":null,"rates":[80000000,40000000,16000000],"bits":[8,10]}
RANGE?         -> RANGE 2412 2472 1
CAP16 <n> <r>  -> DATA <n> <crc32hex> <us>  +  2*n bytes
CAP20 <n> <r>  -> DATA <n> <crc32hex> <us>  +  ceil(20*n/8) bytes
```

El navegador aplica expresiones regulares exactas sobre cada respuesta, así
que el formato está fijado:

```js
/^(ESP32|C2|C3|C5|C6|C61|H2|S2|S3|S31)SDR 6 burst (\d+)$/   // INFO
/^TRANSPORT (UART|USB) (\d+)$/                              // TRANSPORT?
/^DATA (\d+) ([0-9a-fA-F]{1,8}) (\d+)$/                      // la cabecera de la captura
```

El CRC32 es el de zlib, sobre la carga ya empaquetada.

### 8.2 Los formatos de muestra

| Formato | Cómo | Bytes por muestra |
|---|---|---|
| `CAP16` | I y luego Q, los 8 bits altos de cada campo de 10 | 2 |
| `CAP20` | 20 bits por par, el bit menos significativo primero: **I en los bits 0-9 y Q en los 10-19** | 2,5 |

El grupo de 20 bits sale directamente de la palabra cruda, que ya tiene I
en los bits 9-0 y Q en los 19-10.

### 8.3 La entrega del puerto

La web y la consola de Minino comparten el puerto USB Serial/JTAG, y la
consola hace eco de lo que lee (linenoise), así que el REPL no puede
escuchar a la vez que el navegador espera una respuesta limpia.

La solución es que **la entrega del puerto la haga la propia consola**:

```
minino> sdr_host
Serving the ESP-SDR protocol on the USB port.
Open https://espargos.net/espsdr/app/ and connect to this board.
The console comes back when the viewer sends RELEASE, or type MININO.
```

El comando corre dentro de la tarea de la consola, así que cuando la
sesión empieza la consola está entre dos lecturas: el puerto tiene un
solo lector y los bytes no se reparten entre dos.

Mientras dura la sesión:

- el **nivel maestro** de log baja a `ESP_LOG_NONE` (una línea de log en
  mitad de una captura binaria la corrompe);
- el driver de la radio toma una *lease* del Wi-Fi y sólo la devuelve
  cuando el último tenedor la suelta; sin tráfico durante 5 s la radio
  vuelve al grupo, y sin tráfico durante un minuto la sesión termina.

### 8.4 Lo que hace falta para que la captura traiga señal

La cadena de RX del C6 no es sólo "encender el receptor". El firmware del
ESPARGOS la monta así (`receiver.c:292-298`):

```c
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous(true));
```

**Sin power save y sin promiscuo, la captura sale con un valor fijo**: I =
−63 y Q = −62, idénticos en los 13 canales. El motor funciona (el canario
se sustituye, las palabras de guarda quedan intactas) pero la entrada del
conversor es un nivel fijo, porque con el driver en STA el radio gestiona su
propia vida: se duerme, escanea, se asocia.

Con las dos piezas puesta, la captura trae señal de verdad (medido en un
ESP32-C6 rev v0.2):

| Canal | Sin power save ni promiscuo | Con los dos |
|---|---|---|
| 2412 MHz | I = −63, varianza 0 | I = 130, varianza 9 090 |
| 2472 MHz | I = −63, varianza 0 | I = 25, varianza 13 901 |

Y el espectro pasa de una línea plana a 87 dB de rango dinámico.

En Minino el modo promiscuo se pide **con la lease**
(`radio_lease_prom_hold(RADIO_PROM_SDR_RX)`), para que las otras apps que lo
comparten no se lo quiten de debajo.

El firmware del ESPARGOS programa además el canal en el PHY con
`chip_v7_set_chan()`. **En Minino eso cuelga la placa**: el driver de Wi-Fi
tiene su propio estado del PHY, lo reinicializa al detectarlo y se queda
esperando. Con el driver en marcha no es viable; el canal se deja por el
driver, como ya se hacía.

### 8.5 La carga binaria va a trozos

El driver del USB Serial/JTAG mete la carga en un anillo de 256 bytes con
`xRingbufferSend()`, y esa llamada **no acepta un bloque mayor que el
anillo**: se queda esperando un hueco que no va a llegar y devuelve 0 sin
escribir nada. Una captura de 16 380 muestras son 40 950 bytes, así que la
carga tiene que ir a trozos del tamaño del FIFO del hardware:

```c
#define ESDR_TX_CHUNK 64
```

### 8.6 El modo de espectros: la FFT en la placa

El visor de ESPARGOS tiene dos modos: el de ráfagas (el de arriba) y el de
**espectros calculados en el chip** ("On-Chip Spectrum"), que es el que
refresca a decenas de cuadros por segundo. En ese modo la placa calcula la
FFT y manda una lista de códigos de potencia en vez de las muestras.

Minino lo implementa con `main/modules/esp_sdr/esdr_spectrum.c`, portado
del firmware del ESPARGOS (`main/common/spectrum.c` y compañía).

| Comando | Respuesta |
|---|---|
| `SPECINFO?` | `SPECINFO {"continuous":false,"transports":["USB"],"profiles":[[80000000,0,256,1,1,0],...]}` |
| `SPEC <ms> <paso> <uds> <det> <tasa> <bins>` | `SPEC <bins> <tasa_hz> <bins> <mhz>` y luego los marcos |
| `DC?` / `DC 0` / `DC 1` | `DC <modo>` / `OK` |
| `BAUD?` / `BAUD ...` | `ERR baud_transport` (esta sesión va por USB) |

Los perfiles cubren las tres tasas del C6 (80, 40 y 16 MS/s) con 256, 512,
1024 y 2048 bins, que es exactamente lo que el visor pide (su `MAX_FFT` es
también 2048).

**El marco.** Cada cuadro son 28 bytes de cabecera, las bins en orden
natural y 4 bytes de CRC32:

| Offset | Bytes | Contenido |
|---|---|---|
| 0 | 4 | ASCII `SPC1` |
| 4 | 4 | número de cuadro |
| 8 | 8 | índice de la primera muestra |
| 16 | 4 | muestras que abarca el cuadro |
| 20 | 2 | FFTs que representa |
| 22 | 1 | banderas (bit 0: detector de máximo, bit 3: hay huecos entre capturas) |
| 23 | 1 | metadatos de ganancia |
| 24 | 2 | cuadros perdidos acumulados |
| 26 | 1 | log2(bins) |
| 27 | 1 | multiplicador del código en dB (2) |
| 28 | N | códigos de potencia, en orden natural de la FFT |
| 28+N | 4 | CRC32 de la cabecera y las bins |

Un código representa `20*log10(potencia)` en el dominio Q15, acotado a
0..255.

**La FFT** es una radix-2 en tiempo decreciente con la entrada permutada
por inversión de bits, y **cada mariposa divide entre dos**: sin eso, un
tono fuerte saturaría el Q15 a la primera etapa. El test de host la
comprueba contra un DFT directo, porque un espectro con las bins en orden
equivocado parece plausible en pantalla y no es la señal.

**El watchdog.** El bucle de espectros no puede quedarse con la CPU: la
tarea idle es la que alimenta el watchdog de tareas, así que hay que cederle
el paso cada 20 ms.

### 8.7 Los ficheros

| Fichero | Qué es |
|---|---|
| `main/modules/esp_sdr/esdr_burst_proto.c/.h` | el protocolo: C puro, sin ESP-IDF, con 24 pruebas de host |
| `main/modules/esp_sdr/esdr_spectrum.c/.h` | los espectros en la placa: la FFT, el marco y el CRC |
| `main/modules/esp_sdr/esdr_transport.h` | el transporte que comparten los dos módulos |
| `main/modules/esp_sdr/esdr_host.c/.h` | la sesión sobre el USB |
| `main/modules/esp_sdr/test/test_esdr_burst_proto.c` | las pruebas del formato del cable |

El detalle completo está en la §23 de
[`changes-refactor-system-overhaul.md`](./changes-refactor-system-overhaul.md).

### 8.8 Capacidades negociadas y límites del hardware

| Capacidad | Estado en Minino |
|---|---|
| Ganancia RX (`GAIN`) | Soportada (`GAIN 0..60 dB`, paso de 3 dB, configurada en los registros LNA/AGC del receptor) |
| Decimación de tasa (`RATES`) | Soportada en tiempo real (`80 MS/s`, `40 MS/s` por decimación 2x y `16 MS/s` por decimación 5x) |
| Espectros en el chip (`SPEC`) | Soportado con FFT radix-2 Q15 en placa (`256`, `512`, `1024` y `2048` bins) y marcos `SPC1` con CRC32 |
| Ancho de banda analógico | No se anuncia `ALPF` (fijo por canal PHY) |
| GPIO | No se anuncia `GPIO` |
| 5 GHz | El ESP32-C6 opera únicamente en la banda de 2.4 GHz (`2412-2484 MHz`) |

El botón **"Flash Firmware…"** del navegador flashea el firmware del
ESPARGOS encima de Minino: hay que ignorarlo.

---

## 9. La prueba sobre el aire: dos placas

> **Añadido 2026-10-21.**

Una placa emite y la otra recibe con el SDR. La de recepción entra en el
modo host (el protocolo del navegador de ESPARGOS) y la de transmisión
radiando. Es la prueba que faltaba: hasta ahora el SDR se había probado
contra el ruido de fondo, nunca contra una señal que saliera de la otra
placa.

### 9.1 El reparto

| Placa | Papel | Cómo se pone |
|---|---|---|
| A | transmisión | `launch spam_all` (BLE), `launch ssid_spam` (beacons de Wi-Fi), `launch flock_sim` (simulador de Flock) |
| B | recepción del SDR | `sdr_host` en la consola, y a partir de ahí el protocolo del navegador |

Las dos con la variante del SDR (`make build-sdr`), que es la única que
reserva el banco de captura.

### 9.2 Dos fallos que la prueba destapó

**El spammer de SSID no emitía.** `launch ssid_spam` abría la app, pero la
app **no transmitía nada**: la lista de SSID se carga cuando el usuario
elige un elemento del menú, y desde la consola no hay ninguno elegido.
La tarea de spam se quedaba con la lista vacía (`ssid_spam.c:196`) y
salía sin_TX: la app parecía funcionar y el aire estaba en silencio.

Ahora la lista se carga al abrir la app (`ssid_spam_load_stored_list()`),
así que el `Start` del menú funciona siempre, y desde la consola:

```
minino> spam_save --name=minino MININO_SDR_1,MININO_SDR_2,MININO_SDR_3
minino> launch ssid_spam
minino> press down        # el menu: "List SSID" / "Start"
minino> press right       # Start
```

> `press` es el gancho de pruebas que ya existía (`cmd_control.c`): inyecta
> un evento de boton.

**Y reentrar en la app reiniciaba la placa.** `esp_netif_create_default_wifi_ap()`
hace un `assert()` si el netif del AP ya existe, así que arrancar el
spammer dos veces seguidas (o volver a entrar en la app) mataba la placa
con un *"duplicate key"*. Ahora solo se crea el netif si no existe
(`esp_netif_get_handle_from_ifkey("WIFI_AP_DEF")`).

### 9.3 Lo que se midio

Con la placa A emitiendo y la B en recepción:

| Prueba | Resultado |
|---|---|
| `tools/test_esdr_web.py` (el protocolo del navegador, byte a byte) | **56 de 56** |
| `tools/test_esdr_spec.py` (el modo de espectros) | **28 de 28** |
| `make tests` (las 20 suites de host) | **0 fallos** |
| La app nativa (`sdr_capture` + `press right`) | `burst 2: 16380 words in 218 us` |
| La sintonía (`press up`) | `Tuned to 2417 MHz (CH 2)` |
| La salida (`press left`) | `The Wi-Fi driver was started for the capture and is free again` |
| El espectro de una ráfaga | pico −5.6 dBFS, suelo −91.8 dBFS, **86 dB de rango dinámico** |
| La señal en las ráfagas | rms 0.227, pico 0.492 (no es ruido plano) |

Las dos suites se pasaron **con la otra placa radiando**, no en silencio.

### 9.4 La transmisión llega a la otra placa

El receptor de Wi-Fi de la propia placa B ve los beacons de la A. Con la
A spamming y la B en `wifi_scan`:

```
[-46] MININO_SDR_3       <- la placa A
[-46] MININO_SDR_1       <- la placa A
[-53] INFINITUM2843      <- el AP mas fuerte del entorno
[-81] Totalplay-77A5
[-88] Totalplay-77A5_EXT
...
```

−46 dBm: la placa A es **más fuerte que cualquier AP del entorno**. La
radiación de la A llega a la B sin ninguna duda.

### 9.5 Lo que ve el SDR

La diferencia entre el espectro con la transmision puesta y sin ella
cae donde tiene que caer: en la banda del transmisor.

Con el spammer de beacons de Wi-Fi (LO en 2412 MHz), la banda DSSS del
transmisor aparece entre +9 y +16 MHz, es decir **2421..2428 MHz**, con
+0.2 a +1.1 dB por encima del estado sin transmision. Con la antena en
2427 MHz (el canal 4) el mismo bandeado cae sobre el LO, lo que situa al
transmisor en ese canal.

> El spammer pone su AP en el canal 1, pero los beacons salen por el canal
> en el que esta la radio: el `esp_wifi_80211_tx()` usa el canal actual,
> no el de la configuracion del AP.

Con el spammer de BLE, la diferencia se concentra en los canales de
publicidad: **+13.5..+14.3 MHz** del LO (el canal 38 de BLE, 2426 MHz) y
alrededor de −10/−11 MHz (el canal 37, 2402 MHz).

### 9.6 Y el limite de la medida

La diferencia con transmision y sin transmision es de +0.2 a +1.1 dB, y el
**control negativo** (los dos estados haciendo lo mismo) da +1.0 a
+1.4 dB en las mismas bandas: en este entorno la variacion del ambiente es
del orden del efecto.

La razon es fisica, no un fallo del SDR: el entorno de 2.4 GHz esta
saturado (la AP mas cercana a −48 dBm), el AGC del receptor normaliza la
ganancia y los transmisores de Minino tienen un ciclo de trabajo bajo (el
spammer de beacons manda uno cada 100 ms). El pico narrowband de la
señal llega a verse (una rafaga con la portadora del anuncio de BLE
midio +10 dB sobre el suelo), pero la media no se separa del ambiente.

Para una atribucion inequivoca haria falta un entorno de RF en silencio
(o una jaula), no mas mediciones.

### 9.7 La herramienta

```bash
python3 tools/test_esdr_radio.py <puerto_rx> <puerto_tx> [opciones]
```

Mide la diferencia del espectro (o de la potencia estrecha en varias
frecuencias) con la transmision puesta y sin ella, alternando las
ventanas, y compara cada ventana con sus vecinas para que la deriva del
ambiente se cancele. Es la forma de repetir la medición de la §9.5.

Y para las dos aplicaciones del SDR en las dos placas:

```bash
python3 tools/test_esdr_app.py  <puerto>     # la app nativa contra la placa
python3 tools/test_esdr_pair.py app+spec <puerto_1> <puerto_2>
python3 tools/test_esdr_pair.py spec+spec <puerto_1> <puerto_2>
```

La §5 de [`sdr-guia.md`](./sdr-guia.md) tiene los resultados de esa
matriz.

### 9.8 Las dos placas no miden lo mismo si no estan en el mismo sitio

Con las dos placas midiendo el espectro a la vez, los dos perfiles tienen
la misma forma y hasta **14,7 dB de diferencia de nivel**. El `wifi_scan`
de cada una, en el mismo momento, lo explica: los mismos AP con +13 dB
en una placa que en la otra. Dos placas del SDR solo son comparables si
estan en el mismo sitio (o dentro de una jaula).


# El SDR de Minino: resultados de las pruebas y puesta en marcha

> Documento de uso y de puesta en marcha. El informe completo de las
> pruebas está en [`sdr-resultados.md`](./sdr-resultados.md) y el formato
> de los paquetes, en [`sdr-protocolo.md`](./sdr-protocolo.md); la
> referencia técnica del diseño
> (cómo funciona, por qué está así, qué se rompió por el camino) está en
> [`esp-sdr-integration.md`](./esp-sdr-integration.md); el historial del
> branch, en [`changes-refactor-system-overhaul.md`](./changes-refactor-system-overhaul.md).
>
> - **Fecha de las pruebas:** 2026-10-21
> - **Placas:** ESP32-C6 rev v0.2 `48:f6:ee:c2:a2:14` y rev v0.1
>   `7c:2c:67:42:84:a0`, las dos de 8 MB
> - **Toolchain:** ESP-IDF v5.5.1, target `esp32c6`

---

## 1. Qué es, en una página

El C6 no tiene conversor de salida: no puede generar señales de RF. Lo
que sí tiene es el **motor de recepción por dump**, un bloque de SRAM
(0x40840000) donde el hardware deja las muestras de I/Q que la radio
desmoduló, a 80, 40 o 16 MS/s. Con eso se puede:

| Modo | Qué es | Para qué |
|---|---|---|
| **App nativa** | `Applications > SDR` | ráfagas de I/Q a la microSD, con la pantalla del Minino |
| **Modo host** | el comando `sdr_host` | el analizador de espectro de ESPARGOS (<https://espargos.net/espsdr/app/>) habla con la placa por USB |
| **Modo espectros** | el comando `SPEC` del protocolo | la FFT se calcula **en la placa** y llegan los códigos de potencia: decenas de cuadros por segundo |

El SDR **solo recibe**. No hay TX, ni control de ganancia, ni ancho de
 banda analógico, ni 5 GHz. Todo lo que se ve en el aire sale de la
antena del C6 y de los 11 MHz del canal de Wi-Fi que esté sintonizado.

---

## 2. Puesta en marcha

### 2.1 La variante de build

El banco de captura se come un bloque entero de SRAM (128 KiB) y el
*linker script* recorta la imagen para que quepa por debajo de
`0x40840000`. Con la imagen completa de Minino (Bluetooth, Zigbee y
Thread dentro) no cabe: sobran 45 KB. Por eso hay **dos imágenes**:

```bash
cd firmware
make builds        # la de las placas: todo dentro, el SDR apagado
make build-sdr     # la del SDR: Zigbee y Thread fuera, el SDR encendido
```

| Variante | Tamaño | Margen de flash |
|---|---|---|
| Placas (`profiles/minino`) | 3.377 KiB | 8,7 % |
| SDR (`profiles/sdr`) | 2.561 KiB | 32 % |

### 2.2 Flashear

```bash
cd firmware
idf.py -B build-sdr -p /dev/ttyACM0 flash     # o ttyACM1
```

o, a pelo, con `esptool`:

```bash
esptool.py --chip esp32c6 -p /dev/ttyACM0 write_flash \
  0x0    build-sdr/bootloader/bootloader.bin \
  0x8000 build-sdr/partition_table/partition-table.bin \
  0x15000 build-sdr/ota_data_initial.bin \
  0xa0000 build-sdr/minino.bin
```

Requisitos: placa de **8 MB** con la tabla de particiones de 8 MB
(`partitions.csv`). En la imagen de las placas el SDR **no está**: el
código compila, pero no hay banco de captura detrás y el menú no enseña
la entrada.

### 2.3 Comprobarlo

```bash
python3 tools/test_esdr_web.py /dev/ttyACM0    # el protocolo del navegador
python3 tools/test_esdr_spec.py /dev/ttyACM0   # el modo de espectros
```

> Las dos pruebas entregan el puerto con `sdr_host` ellas mismas. Si se
> hace a mano, la consola de Minino cierra las líneas con **CR**, no con
> LF: `sdr_host` + CR.

---

## 3. Las tres formas de usarlo

### 3.1 La app nativa

`Applications > SDR` (o `sdr_capture` en la consola):

| Botón | Qué hace |
|---|---|
| UP / DOWN | sintonía ±5 MHz, de 2412 a 2472 MHz |
| RIGHT | captura una ráfaga de 16 380 muestras y la guarda en la microSD |
| LEFT | sale y devuelve la radio a Minino |

En la consola, el gancho de pruebas permite lo mismo sin botones:

```
minino> sdr_capture
minino> press right     # burst 1: 16380 words in 218 us
minino> press up        # Tuned to 2417 MHz (CH 2)
minino> press left      # sale y libera el driver de Wi-Fi
```

Los ficheros van a `/sdcard/sdrcap00.iq` (una palabra de 32 bits por
muestra, I en los bits 9-0 y Q en los 19-10). **Sin microSD la captura
funciona igual, pero no se guarda**: la app lo dice en pantalla y en el
log (`not saved: no SD`).

### 3.2 El modo host y el visor de ESPARGOS

```bash
minino> sdr_host
Serving the ESP-SDR protocol on the USB port.
Open https://espargos.net/espsdr/app/ and connect to this board.
The console comes back when the viewer sends RELEASE, or type MININO.
```

A partir de ahí el visor habla el protocolo de línea del firmware
*burst* del ESP-SDR (`SYNC`, `INFO`, `CAPS`, `FREQ`, `CAP16`, `CAP20`,
`SPEC`...). La consola deja de leer el puerto mientras dure la sesión
(la entrega la hace la propia consola, desde el interior de la tarea, y
el log baja a `ESP_LOG_NONE` para que una línea de log no corrompa una
captura binaria). El visor manda `RELEASE` al cerrar; `MININO` es el
escape propio de Minino.

> El botón **"Flash Firmware…"** del visor flashea el firmware del
> ESPARGOS encima de Minino: hay que ignorarlo.

### 3.3 Las pruebas automáticas

| Herramienta | Qué mide |
|---|---|
| `tools/test_esdr_web.py` | el protocolo del navegador byte a byte: el handshake, las expresiones regulares, el CRC32 y el desempaquetado de I/Q |
| `tools/test_esdr_spec.py` | el modo de espectros: los perfiles, los marcos `SPC1`, el CRC y el corte |
| `tools/test_esdr_app.py` | la app nativa contra la placa: la ráfaga, la sintonía y la salida |
| `tools/test_esdr_pair.py` | las dos aplicaciones del SDR a la vez en las dos placas |
| `tools/test_esdr_radio.py` | la prueba sobre el aire: una placa transmite y la otra recibe con el SDR |
| `tools/test_esdr_app.py` | la prueba de la app nativa contra la placa (ráfaga, sintonía, salida) |
| `tools/test_esdr_pair.py` | las dos aplicaciones del SDR a la vez en las dos placas, y la comparación de los dos perfiles |

---

## 4. Resultados

### 4.1 Las dos placas, las tres suites

Las dos placas llevan la variante del SDR y las tres suites pasan
**con la otra placa radiando**:

| Placa | `test_esdr_web.py` | `test_esdr_spec.py` | `test_esdr_app.py` |
|---|---|---|---|
| `48:f6:ee:c2:a2:14` (v0.2) | **56 de 56** | **28 de 28** | **16 de 16** |
| `7c:2c:67:42:84:a0` (v0.1) | **56 de 56** | **28 de 28** | **16 de 16** |

Y las 20 suites de host: `make tests` → **0 fallos**.

Lo que cubren las 84 comprobaciones de placa: la identidad (`C6SDR 6
burst 16380`), las capacidades que el visor espera y las que no debe ver,
el rango 2412-2472, los límites (tasas, bits, ganancia), la sintonía
(que acepta 2417 y rechaza 2000), las capturas de 16 380 muestras a 8 y
10 bits con su CRC, las tres tasas (80, 40 y 16 MS/s), el rango dinámico
del espectro, el `RELEASE`, el escape `MININO` y la vuelta de la
consola.

### 4.2 El espectro, en las dos placas

Instantánea del modo de espectros (LO en 2412 MHz, 40 MS/s, 1024 bins,
1 427 marcos promediados). Los dos perfiles son el mismo: la joroba
ancha del canal 1 de Wi-Fi (2401-2423 MHz) centrada en el LO, con el
suelo de ruido 10 MHz más lejos.

```
   placa 48:f6:ee           placa 7c:2c:67
   pico 113  mediana 58     pico 116  mediana 58
   min  8                   min  8
   LO (bin 0)  74.0 dB      LO (bin 0)  74.4 dB

   MHz del LO   dB          MHz del LO   dB
     -12      49.5            -12      50.2
     -10      56.6            -10      55.7
      -5      61.7             -5      60.5
      -2      68.2             -2      65.0
      +0      74.0             +0      74.4
      +2      71.1             +2      75.2
      +5      69.1             +5      71.6
     +10      65.0            +10      69.8
     +12      48.7            +12      54.6
     +15      22.5            +15      28.3
     +19      10.8            +19      12.9
```

El rango dinámico de una ráfaga calculada en el host es de **86 a 89
dB**, con el pico en el LO (que es donde debe estar: es la portadora del
canal que está sintonizado).

### 4.3 La app nativa, en placa

```
I (105901) esp_sdr_module: Entering native SDR I/Q capture app...
I (106154) c6_sdr_rx: Initializing C6 native SDR RX engine (buffer at 0x40840000)...
I (122537) esp_sdr_module: burst 2: 16380 words in 218 us
I (130544) c6_sdr_rx: Tuned to 2417 MHz (CH 2)
I (133649) c6_sdr_rx: The Wi-Fi driver was started for the capture and is free again
```

16 380 palabras (65 KiB) en 218 us son **75 MS/s**, coherente con los
80 MS/s del motor de dump.

### 4.4 La prueba sobre el aire (round aparte)

> Esta es la **única** medición que necesita una tercera aplicación: una
> app que radia. Todo lo demás de este documento es solo con las dos del
> SDR. Para la versión con las dos aplicaciones únicamente, mira la §5.

Una placa emite, la otra recibe con el SDR. Lo primero, la prueba de que
la señal **llega**: el receptor de Wi-Fi de la placa de recepción ve los
beacons de la otra, y son lo más fuerte que hay en el aire.

```
placa B, wifi_scan, con la placa A emitiendo:

[-46] MININO_SDR_3       <- la placa A
[-46] MININO_SDR_1       <- la placa A
[-53] INFINITUM2843      <- el AP más fuerte del entorno
[-81] Totalplay-77A5
...
```

Y lo segundo, lo que ve el SDR: la diferencia entre el espectro con la
transmisión puesta y sin ella cae **en la banda del emisor**.

| Emisor | Dónde aparece la diferencia (LO en 2412 MHz) | Tamaño |
|---|---|---|
| Spammer de beacons de Wi-Fi | **2421..2428 MHz** (+9 a +16 MHz del LO) | +0,2 a +1,1 dB |
| Spammer de BLE | **2426 MHz** (+13,5 a +14,3 MHz del LO) y 2402 MHz (−10/−11 MHz) | +1,1 a +1,5 dB |

La banda DSSS del spammer de beacons son 11 MHz, y el bloque medido
(+9..+16 MHz) son 7 MHz de ellos: el transmisor está en el canal 4
(2427 MHz). Con la antena puesta en 2427 MHz el mismo bandeado cae
sobre el LO, lo que confirma el canal.

> El spammer configura su AP en el canal 1, pero los beacons salen por
> el canal en el que está la radio: `esp_wifi_80211_tx()` usa el canal
> actual, no el de la configuración del AP.

### 4.5 Y el límite honesto de esa medida

La diferencia con transmisión (+0,2 a +1,1 dB) es **del orden del
control negativo** (los dos estados haciendo lo mismo: +1,0 a +1,4 dB en
las mismas bandas). En un entorno de 2,4 GHz saturado, con el AGC del
receptor normalizando la ganancia y con los transmisores de Minino en un
ciclo de trabajo bajo (un beacon cada 100 ms), la señal del emisor no se
separa del ambiente en la media.

Lo que sí queda por encima del ruido, medido, es el **pico** de una
ráfaga estrecha: con el spammer de BLE, una ráfaga que pilla un anuncio
mide **+10 dB sobre el suelo** en el canal de publicidad 37 (2402 MHz),
exactamente donde toca.

Para una atribución inequívoca hace falta un entorno de RF en silencio
o una jaula, no más mediciones. Queda escrito en
[`esp-sdr-integration.md` §9.6](./esp-sdr-integration.md).

---

## 5. Las dos aplicaciones en las dos placas

> Con las dos Minino en la mesa, cada una con **una** de las dos
> aplicaciones del SDR y **nada más**: la app nativa (la ráfaga de I/Q) en
> una placa y el modo de espectros (la FFT en la placa) en la otra, y al
> revés. Las dos placas miden el mismo ambiente de RF, así que sus
> resultados tienen que ser coherentes entre sí.

La herramienta que lo hace todo:

```bash
python3 tools/test_esdr_pair.py <modo> <puerto_1> <puerto_2>
```

| Modo | Qué hace |
|---|---|
| `app+spec` | la app nativa en la primera placa y el modo de espectros en la segunda, **a la vez** |
| `spec+spec` | el modo de espectros en las dos, a la vez, y la comparación de los dos perfiles |
| `app+app` | la app nativa en las dos, a la vez |

### 5.1 La matriz de pruebas

Las dos placas: `48:f6:ee:c2:a2:14` (rev v0.2) y `7c:2c:67:42:84:a0`
(rev v0.1), las dos con la variante del SDR.

| Asignación | Placa A | Placa B | Resultado |
|---|---|---|---|
| 1 | app nativa | modo de espectros | app: 3 ráfagas de 16 380 palabras en 217-218 us, sintonía 2412 ↔ 2417 MHz, salida limpia · espectro: 1 416 marcos con CRC bueno (pico 116) |
| 2 | modo de espectros | app nativa | espectro: 1 399 marcos (pico 112) · app: 3 ráfagas de 16 380 palabras en 217-218 us |
| 3 | app nativa | app nativa | las dos a la vez: 3 ráfagas cada una (una de 557 us, un tirón del driver) |
| 4 | modo de espectros | modo de espectros | 1 413 y 1 402 marcos, y los dos perfiles comparados banda a banda |

Y las suites sueltas, en las dos placas:

| Prueba | 48:f6:ee | 7c:2c:67 |
|---|---|---|
| `tools/test_esdr_app.py` (la app nativa) | **16 de 16** | **16 de 16** |
| `tools/test_esdr_spec.py` (el modo de espectros) | **28 de 28** | **28 de 28** |

La app nativa, comprobación a comprobación: que la app arranque, que el
motor de captura se levante, que sintonice 2412 MHz al entrar, que la
microSD responda, que la ráfaga sea de 16 380 palabras, que la tasa esté
cerca de los 80 MS/s (**75,1 y 75,5 MS/s** medidos), que el fichero se
guarde, que UP y DOWN sintonizen 2417 y 2412 MHz, que el contador de
ráfagas avance, que la app salga, que el motor se pare, que la radio
vuelva a Minino y que la consola siga viva.

### 5.2 Los dos perfiles, en el mismo ambiente

Las dos placas midiendo el espectro **a la vez** (LO en 2412 MHz,
40 MS/s, 1024 bins, ~1 400 marcos cada una):

```
        banda   placa 7C:2C:67   placa 48:F6:EE      dif
      -20 MHz            8.9            9.0    -0.1
      -16 MHz           19.6           19.0    +0.6
      -12 MHz           52.7           52.7    +0.0
       -8 MHz           59.5           60.0    -0.5
       -4 MHz           62.2           62.3    -0.0
        +0 MHz           69.9           67.8    +2.0
       +2 MHz           82.3           67.6   +14.7
       +6 MHz           80.7           67.7   +13.0
      +10 MHz           73.9           61.8   +12.2
      +14 MHz           40.6           27.3   +13.3
      +18 MHz           18.8           11.2    +7.6
```

**La forma es la misma** en las dos placas (la joroba del canal 1 de
Wi-Fi, el mismo pie, los mismos hombros) y **el nivel no**: hasta 14,7 dB
de diferencia por arriba del LO.

La diferencia no es de las placas: es de **dónde están**. El `wifi_scan`
de cada una, en el mismo momento, lo dice:

| Red | 48:f6:ee | 7c:2c:67 | diferencia |
|---|---|---|---|
| INFINITUM2843 | −45 dBm | −58 dBm | **+13 dB** |
| Totalplay-77A5 | −69 dBm | −87 dBm | **+18 dB** |
| Club_Totalplay_WiFi | −71 dBm | −87 dBm | +16 dB |
| Totalplay-77A5_EXT | −83 dBm | −95 dBm | +12 dB |

Los mismos AP, con 13 dB más nivel en una placa que en la otra: la
diferencia del espectro (+12 a +14 dB) es la del entorno, no la del
firmware. Por debajo del LO las dos placas coinciden dentro de ±1 dB.

> Moraleja para el que mida: **dos placas del SDR no son dos medidas
> comparables si no están en el mismo sitio**. O se mide el RSSI de
> referencia al mismo tiempo, o se pone la jaula.

### 5.3 Lo que no se ha usado

Ni una sola app de radiación (ni spammer, ni simulador de Flock, ni
analizador) en el aire: las dos aplicaciones de esta prueba son las dos
del SDR, la ráfaga y el espectro. El `wifi_scan` del punto 5.2 es la
*referencia* del ambiente (dice cuánta señal hay alrededor), no un
emisor.

## 6. Lo que las pruebas destaparon

| Fallo | Síntoma | Arreglo |
|---|---|---|
| El spammer de SSID no emitía | lanzado desde la consola, la app transmitía nada: la lista de SSID solo se carga al elegir un elemento del menú | `ssid_spam_load_stored_list()`: la lista se carga al abrir la app |
| Reentrar en el spammer reiniciaba la placa | `assert()` de `esp_netif_create_default_wifi_ap()`: *"duplicate key"* | el netif del AP solo se crea si no existe |
| La entrega del puerto fallaba a veces | las pruebas mandaban `sdr_host` con LF y la consola de Minino cierra las líneas con **CR** | las pruebas mandan CR |

---

## 7. Referencia rápida

### 7.1 Comandos de la consola

| Comando | Qué hace |
|---|---|
| `sdr_capture` | abre la app nativa del SDR |
| `sdr_host` | entrega el puerto al protocolo del visor de ESPARGOS |
| `launch ssid_spam` | abre el spammer de beacons de Wi-Fi (con `press down` + `press right` arranca el ataque) |
| `launch spam_all` | abre el spammer de BLE |
| `launch flock_sim` | abre el simulador de Flock |
| `wifi_scan` | lista las redes que ve (sirve para comprobar que una transmisión llega) |
| `press <up\|down\|left\|right>` | inyecta un evento de botón |
| `display` | informa del estado del panel y de la app |

### 7.2 El protocolo del visor

```
SYNC <nonce>   -> SYNC <nonce>
INFO           -> C6SDR 6 burst 16380
CAPS           -> CAPS VERSION RXLIMITS HWAGC IQ8 SERIALLEASE TUNEEXT RX40 RX16 ...
FREQ <MHz>     -> OK
LIMITS?        -> LIMITS {"gain":[0,0,1],...}
RANGE?         -> RANGE 2412 2472 1
CAP16 <n> <r>  -> DATA <n> <crc32hex> <us>  +  2*n bytes
CAP20 <n> <r>  -> DATA <n> <crc32hex> <us>  +  ceil(20*n/8) bytes
SPEC <ms> <paso> <uds> <det> <tasa> <bins> -> SPEC <bins> <tasa> <bins> <mhz> + marcos
RELEASE        -> OK      (la consola vuelve)
MININO         -> OK      (escape propio de Minino)
```

### 7.3 Limitaciones conocidas

| Limitación | Por qué |
|---|---|
| Solo recepción | el C6 no tiene DAC de TX; el motor de dump es un receptor |
| Sin ganancia manual | el hardware hace AGC; la capacidad `GAIN` no se anuncia |
| Sin ancho de banda analógico | no se implementa |
| Sin 5 GHz | el C6 no lo tiene |
| La entrega del puerto es manual | la consola hace eco de lo que lee y el visor espera una respuesta limpia |
| El "Flash Firmware…" del visor borra Minino | hay que ignorarlo |

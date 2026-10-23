# Resultados de las pruebas del SDR de Minino

> Informe de las pruebas hechas sobre dos Minino con la variante del SDR
> (la que reserva el banco de captura): una en recepción del SDR y la otra
> radiando, y al revés, en el mismo ambiente de RF.
>
> - **Fecha:** 2026-10-21
> - **Placas:** ESP32-C6 rev v0.2 `48:f6:ee:c2:a2:14` y rev v0.1
>   `7c:2c:67:42:84:a0`, las dos de 8 MB
> - **Toolchain:** ESP-IDF v5.5.1, target `esp32c6`
> - **Firmware:** `firmware/build-sdr` (`minino.bin`, 2.561.504 bytes, 32 %
>   libre en la partición de aplicación)
> - **Documentos hermanos:** el resumen de las dos aplicaciones
>   [`spectrum-SDR-results.md`](./spectrum-SDR-results.md), la guía de uso
>   [`sdr-guia.md`](./sdr-guia.md), el formato de los paquetes sobre el
>   cable [`sdr-protocolo.md`](./sdr-protocolo.md) y la referencia técnica
>   [`esp-sdr-integration.md`](./esp-sdr-integration.md)

---

## 1. Resumen

| # | Qué se probó | Dónde | Resultado |
|---|---|---|---|
| 1 | Las 15 suites de C (378 pruebas) y la de Python | host | **0 fallos** |
| 2 | El protocolo del visor de ESPARGOS, byte a byte | las dos placas | **56/56** y **56/56** |
| 3 | El modo de espectros (FFT en la placa) | las dos placas | **28/28** y **28/28** |
| 4 | La app nativa del SDR (ráfaga, sintonía, salida) | las dos placas | **16/16** y **16/16** |
| 5 | La app nativa y el modo de espectros **a la vez**, en las dos placas | las dos placas | correcto en las 4 asignaciones |
| 6 | Los dos perfiles de espectro medidos a la vez | las dos placas | misma forma, hasta **14,7 dB** de diferencia de nivel |
| 7 | *Round aparte:* la transmisión de una placa vista por la otra | las dos placas | los beacons a **−46 dBm** (§7) |

Lo que queda por encima de todo: **las dos placas miden el mismo
ambiente de RF y sus resultados son coherentes entre sí**, con la
única salvedad de que el nivel depende de dónde esté cada placa (§6).

---

## 2. Cómo se repitieron las pruebas

```bash
# el firmware
cd firmware
make build-sdr
idf.py -B build-sdr -p /dev/ttyACM0 flash
idf.py -B build-sdr -p /dev/ttyACM1 flash

# las suites
python3 tools/test_esdr_web.py  /dev/ttyACM0    # el protocolo del visor
python3 tools/test_esdr_spec.py /dev/ttyACM0    # el modo de espectros
python3 tools/test_esdr_app.py  /dev/ttyACM0    # la app nativa

# las dos aplicaciones a la vez en las dos placas
python3 tools/test_esdr_pair.py app+spec  /dev/ttyACM0 /dev/ttyACM1
python3 tools/test_esdr_pair.py app+spec  /dev/ttyACM1 /dev/ttyACM0
python3 tools/test_esdr_pair.py app+app   /dev/ttyACM0 /dev/ttyACM1
python3 tools/test_esdr_pair.py spec+spec /dev/ttyACM0 /dev/ttyACM1

# la prueba sobre el aire (una placa radia)
python3 tools/test_esdr_radio.py /dev/ttyACM1 /dev/ttyACM0 --mode spec
```

Las placas se identifican por su número de serie de USB, que no cambia al
reconectarlas:

```
/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_48:F6:EE:C2:A2:14-if00
/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_7C:2C:67:42:84:A0-if00
```

---

## 3. Las suites

### 3.1 El protocolo del visor de ESPARGOS

`tools/test_esdr_web.py` replica la lógica de `radio.js` del navegador:
el handshake, las expresiones regulares exactas sobre cada respuesta, la
comprobación del CRC32 y el desempaquetado de I/Q.

| Comprobación | Resultado |
|---|---|
| La consola aparece y el modo host arranca | PASA |
| El marcador de sincronización vuelve intacto | PASA |
| `INFO` → `C6SDR 6 burst 16380` | PASA |
| `CAPS` anuncia lo que el visor espera y **no** anuncia `GPIO`, `GAIN`, `ALPF`, `SPECSTAT` | PASA |
| `TRANSPORT?` → `TRANSPORT USB 0` | PASA |
| `RANGE?` → `RANGE 2412 2472 1` | PASA |
| `LIMITS?` con el JSON del visor (ganancia, tasas, bits) | PASA |
| `VERSION?` con el JSON del visor | PASA |
| `FREQ 2412` y `FREQ 2417` → `OK`; `FREQ 2000` → `ERR` | PASA |
| `CAP16` y `CAP20` de 16 380 muestras, con su CRC | PASA |
| Las tres tasas (80, 40 y 16 MS/s) | PASA |
| El espectro de la captura tiene rango dinámico (86 dB) y el pico en el LO | PASA |
| `RELEASE` responde `OK` y la consola vuelve | PASA |
| `MININO` devuelve la consola | PASA |

**56 de 56 en las dos placas.**

La captura, en números:

| | `48:f6:ee` | `7c:2c:67` |
|---|---|---|
| `CAP16` de 16 380 muestras | 32 760 bytes, 217 us de captura, 608 kB/s | 32 760 bytes, 217 us, 611 kB/s |
| `CAP16`: pico / rms de la I/Q | 0,477 / 0,247 | 0,367 / 0,120 |
| `CAP20` de 16 380 muestras | 40 950 bytes, 209 us, 618 kB/s | 40 950 bytes, 208 us, 618 kB/s |
| `CAP20`: pico / rms de la I/Q | 0,445 / 0,223 | 0,275 / 0,076 |
| Rango dinámico del espectro | 88,2 dB (pico −3,4 dBFS, suelo −91,7 dBFS) | 86,2 dB (pico −9,4 dBFS, suelo −95,6 dBFS) |
| El pico está en el LO | sí | sí |

### 3.2 El modo de espectros (la FFT en la placa)

`tools/test_esdr_spec.py`: los perfiles, la corriente de marcos, el CRC
y el corte.

| Comprobación | Resultado |
|---|---|
| Los 12 perfiles (3 tasas × 4 tamaños de FFT) | PASA |
| La cabecera `SPEC <bins> <tasa> <bins> <mhz>` | PASA |
| Los marcos `SPC1` llegan con el CRC bueno | PASA |
| Las bins son códigos de 0 a 255 y el espectro tiene estructura | PASA |
| La portadora del LO está en el bin 0 | PASA |
| `SPECEND` con los doce campos y el corte limpio | PASA |

**28 de 28 en las dos placas.**

### 3.3 La app nativa del SDR

`tools/test_esdr_app.py`: la app de la pantalla, sin pasar por el visor.

| Comprobación | `48:f6:ee` | `7c:2c:67` |
|---|---|---|
| La app arranca | PASA | PASA |
| El motor de captura se levanta (banco en 0x40840000) | PASA | PASA |
| Sintoniza 2412 MHz (CH 1) al entrar | PASA | PASA |
| La microSD responde | PASA | PASA |
| La ráfaga son 16 380 palabras | PASA | PASA |
| La tasa está cerca de los 80 MS/s | 75,5 MS/s | 75,1 MS/s |
| La ráfaga se guarda en la microSD | PASA | PASA |
| UP sintoniza 2417 MHz (CH 2) | PASA | PASA |
| DOWN vuelve a 2412 MHz (CH 1) | PASA | PASA |
| El contador de ráfagas avanza | PASA | PASA |
| La app sale y el motor se para | PASA | PASA |
| La radio vuelve a Minino | PASA | PASA |
| La consola sigue viva | PASA | PASA |

**16 de 16 en las dos placas.**

### 3.4 Las suites de host

```
make tests
```

15 suites de C (**378 pruebas**) más la de Python (`test_flash_tools.py`,
5 pruebas): **0 fallos**.

| Suite | Pruebas | Suite | Pruebas |
|---|---|---|---|
| `surveillance_detect` | 80 | `cmd_control` | 16 |
| `nmea_parser` | 22 | `esp_sdr` | 24 |
| `drone_id` | 27 | `menus_module` | 7 |
| `files_ops` | 23 | `hid_device` | 13 |
| `gap_dispatcher` | 9 | `rf_spectrum` | 13 |
| `radio_lease` | 12 | `general_scrolling_text` | 12 |
| `event_bus` | 11 | `bt_spam` | 13 |
| `resistor_detector` | 12 | `test_flash_tools.py` | 5 |

---

## 4. Las dos aplicaciones a la vez, en las dos placas

`tools/test_esdr_pair.py`. Cada hilo habla con su placa: la app nativa
por la consola (con `sdr_capture` y el gancho `press`) y el modo de
espectros por el protocolo (con `SPEC`).

| Asignación | Placa A | Placa B | Resultado |
|---|---|---|---|
| 1 | app nativa | modo de espectros | app: 3 ráfagas de 16 380 palabras (218, 217, 218 us), sintonía 2412 ↔ 2417, salida limpia, radio liberada · espectros: **1 416 marcos** con CRC bueno, pico 116, mediana 58 |
| 2 | modo de espectros | app nativa | espectros: **1 399 marcos**, pico 112, mediana 59 · app: 3 ráfagas de 16 380 palabras (217, 218, 218 us) |
| 3 | app nativa | app nativa | las dos a la vez: 3 ráfagas cada una (una de 557 us, un tirón del driver) |
| 4 | modo de espectros | modo de espectros | **1 413** y **1 402** marcos, y los dos perfiles comparados banda a banda (§6) |

En las cuatro asignaciones, el `SPECEND` sale limpio: estado 0, sin
marcos perdidos (`drops` 0), sin abandonos (`abandoned` 0) y todos los
FFTs entregados.

---

## 5. El espectro, en las dos placas

Instantánea del modo de espectros: LO en 2412 MHz, 40 MS/s, 1024 bins,
1 427 marcos promediados, una placa cada vez.

| | `48:f6:ee` | `7c:2c:67` |
|---|---|---|
| Pico | 113 | 116 |
| Mediana | 58 | 58 |
| Mínimo (suelo) | 8 | 8 |
| El LO (bin 0) | 74,0 dB | 74,4 dB |

Perfil banda a banda (dB, MHz relativos al LO):

| Banda | `48:f6:ee` | `7c:2c:67` |
|---|---|---|
| −12 MHz | 49,5 | 50,2 |
| −10 MHz | 56,6 | 55,7 |
| −5 MHz | 61,7 | 60,5 |
| −2 MHz | 68,2 | 65,0 |
| **0 MHz (LO)** | **74,0** | **74,4** |
| +2 MHz | 71,1 | 75,2 |
| +5 MHz | 69,1 | 71,6 |
| +10 MHz | 65,0 | 69,8 |
| +12 MHz | 48,7 | 54,6 |
| +15 MHz | 22,5 | 28,3 |
| +19 MHz | 10,8 | 12,9 |

Las dos ven lo mismo: una joroba ancha de unos 20 MHz centrada en el LO
(la banda del canal 1 de Wi-Fi, 2401-2423 MHz), con el suelo de ruido
10 MHz más allá.

---

## 6. Las dos placas a la vez: el mismo ambiente, niveles distintos

Midiendo las dos a la vez (LO 2412 MHz, 40 MS/s, 1024 bins):

| Banda | `7c:2c:67` | `48:f6:ee` | dif |
|---|---|---|---|
| −20 MHz | 8,9 | 9,0 | −0,1 |
| −18 MHz | 12,4 | 11,5 | +0,9 |
| −16 MHz | 19,6 | 19,0 | +0,6 |
| −14 MHz | 34,0 | 33,4 | +0,6 |
| −12 MHz | 52,7 | 52,7 | +0,0 |
| −10 MHz | 57,8 | 58,2 | −0,4 |
| −8 MHz | 59,5 | 60,0 | −0,5 |
| −6 MHz | 59,9 | 60,7 | −0,7 |
| −4 MHz | 62,2 | 62,3 | −0,0 |
| 0 MHz | 69,9 | 67,8 | +2,0 |
| +2 MHz | 82,3 | 67,6 | **+14,7** |
| +4 MHz | 81,5 | 68,3 | +13,1 |
| +6 MHz | 80,7 | 67,7 | +13,0 |
| +8 MHz | 80,4 | 68,1 | +12,4 |
| +10 MHz | 73,9 | 61,8 | +12,2 |
| +12 MHz | 57,4 | 44,4 | +13,0 |
| +14 MHz | 40,6 | 27,3 | +13,3 |
| +16 MHz | 28,1 | 16,6 | +11,6 |
| +18 MHz | 18,8 | 11,2 | +7,6 |

**La forma es la misma; el nivel no.** Hasta 14,7 dB por encima del LO.

### 6.1 La explicación: dónde está cada placa

El `wifi_scan` de las dos, en el mismo momento:

| Red | `48:f6:ee` | `7c:2c:67` | diferencia |
|---|---|---|---|
| INFINITUM2843 | −45 dBm | −58 dBm | **+13 dB** |
| Totalplay-77A5 | −69 dBm | −87 dBm | **+18 dB** |
| Club_Totalplay_WiFi | −71 dBm | −87 dBm | +16 dB |
| Totalplay-77A5_EXT | −83 dBm | −95 dBm | +12 dB |
| Totalplay-B6B8 | −90 dBm | −90 dBm | 0 dB |
| IZZI-8BDC | −90 dBm | −95 dBm | +5 dB |
| IZZI-F50C | −91 dBm | −89 dBm | −2 dB |
| Totalplay-2.4G-F9C0 | −93 dBm | −92 dBm | −1 dB |

Los mismos AP, con hasta 18 dB más nivel en una placa que en la otra: la
diferencia del espectro es la del entorno, no la del firmware. Por debajo
del LO (donde la señal es más débil y el ruido de la propia placa pesa
más) las dos coinciden dentro de ±1 dB.

> **Para comparar dos placas del SDR hay que medir el RSSI de referencia
> al mismo tiempo.** Sin eso, dos placas a medio metro pueden dar 14 dB
> de diferencia sin que haya nada raro.

---

## 7. La transmisión de una placa vista por la otra

> **Este round es el único que necesita una tercera aplicación** (una app
> que radia). El resto del informe es solo con las dos del SDR. Se deja
> aquí como la medición del límite de la §7.1.

Con la placa A radando (spammer de beacons de Wi-Fi) y la B en recepción
del SDR:

**Que la señal llega** (el receptor de Wi-Fi de la propia placa B):

```
[-46] MININO_SDR_3       <- la placa A
[-46] MININO_SDR_1       <- la placa A
[-53] INFINITUM2843      <- el AP más fuerte del entorno
[-81] Totalplay-77A5
[-88] Totalplay-77A5_EXT
```

**Que el SDR la ve**, la diferencia del espectro con y sin transmisión
(LO en 2412 MHz), cayendo en la banda del emisor:

| Emisor | Dónde sale la diferencia | Tamaño |
|---|---|---|
| Spammer de beacons de Wi-Fi | **2421..2428 MHz** (+9 a +16 MHz del LO) | +0,2 a +1,1 dB |
| Spammer de BLE | **2426 MHz** (+13,5 a +14,3 MHz del LO) y 2402 MHz (−10/−11 MHz) | +1,1 a +1,5 dB |

La banda DSSS del spammer de beacons son 11 MHz y el bloque medido son 7
de ellos: el emisor está en el canal 4 (2427 MHz). Con la antena en
2427 MHz el mismo bandeado cae sobre el LO, lo que confirma el canal.

> El spammer configura su AP en el canal 1, pero los beacons salen por el
> canal en el que está la radio: `esp_wifi_80211_tx()` usa el canal
> actual, no el de la configuración del AP.

**El límite de esta medida**: el control negativo (los dos estados
haciendo lo mismo) da +1,0 a +1,4 dB en las mismas bandas, así que el
efecto está al nivel del ruido del ambiente. En un entorno de 2,4 GHz
saturado, con el AGC normalizando la ganancia y con los transmisores de
Minino en un ciclo de trabajo bajo, la media no separa al emisor del
fondo.

Y el motivo por el que el SDR, con la misma antena y el mismo preamplificador
del C6, no lo ve mejor que el resto: **las dos placas tienen la misma
banda de 2,4 GHz que el Bluetooth** (los canales de publicidad de BLE
son 2402, 2426 y 2480 MHz), pero el SDR solo llega a los canales de
Wi-Fi (2412-2472). Los canales de BLE caen en los bordes: el 37 (2402)
queda a −10 MHz del LO en 2412 y el 39 (2480) a +8 MHz del LO en 2472.

---

## 8. Los fallos que encontraron las pruebas

| Fallo | Síntoma | Arreglo |
|---|---|---|
| El spammer de SSID no emitía | desde la consola la app no transmitía: la lista de SSID solo se carga al elegir un elemento del menú | `ssid_spam_load_stored_list()`: la lista se carga al abrir la app |
| Reentrar en el spammer reiniciaba la placa | `assert()` de `esp_netif_create_default_wifi_ap()`: *"duplicate key"* | el netif del AP solo se crea si no existe |
| La entrega del puerto fallaba a veces | las pruebas mandaban `sdr_host` con LF y la consola de Minino cierra las líneas con **CR** | las pruebas mandan CR |

Ninguno de los tres era del SDR en sí, pero los tres impedían medirlo.

---

## 9. Conclusión

- **La cadena de recepción funciona**: el protocolo del visor, el modo
  de espectros y la app nativa pasan en las dos placas, con y sin la
  otra placa radiando.
- **Las dos placas miden el mismo ambiente**: misma forma de espectro,
  mismo juego de redes, y el nivel explica exactamente la distancia a
  los AP.
- **La captura es de verdad**: 16 380 muestras en 217-218 us (75 MS/s),
  con 86 dB de rango dinámico y el pico en el LO.
- **Lo que no se ha podido demostrar**: que la señal de *otra* Minino se
  separe del ambiente en la media del espectro. Hace falta un entorno de
  RF en silencio, o una jaula.

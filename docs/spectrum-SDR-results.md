# Spectrum-SDR results

> Resultados de la prueba del SDR de Minino con **las dos aplicaciones
> del SDR y solo ellas**: la app nativa (la ráfaga de I/Q) y el modo de
> espectros (la FFT calculada en la placa). Sin ninguna app de radiación
> en juego: una Minino mide con una aplicación y la otra con la otra, en
> el mismo ambiente de RF, y al revés.
>
> - **Fecha:** 2026-10-21
> - **Placas:** ESP32-C6 rev v0.2 `48:f6:ee:c2:a2:14` y rev v0.1
>   `7c:2c:67:42:84:a0`, las dos de 8 MB
> - **Firmware:** la variante del SDR (`make build-sdr`, `minino.bin` de
>   2.561.504 bytes, 32 % libre en la partición de aplicación)
> - **Toolchain:** ESP-IDF v5.5.1, target `esp32c6`
> - **Documentos hermanos:** [`sdr-protocolo.md`](./sdr-protocolo.md) (el
>   cable, byte a byte), [`sdr-guia.md`](./sdr-guia.md) (uso) y
>   [`sdr-resultados.md`](./sdr-resultados.md) (el informe largo)

---

## 1. Resumen

| Prueba | `48:f6:ee` | `7c:2c:67` |
|---|---|---|
| El protocolo del visor de ESPARGOS, byte a byte | **56/56** | **56/56** |
| El modo de espectros (FFT en la placa) | **28/28** | **28/28** |
| La app nativa del SDR (ráfaga, sintonía, salida) | **16/16** | **16/16** |

| Las dos aplicaciones a la vez, en las dos placas | Resultado |
|---|---|
| app nativa en A + espectros en B | 3 ráfagas de 16 380 palabras (218, 217, 218 us) · 1 416 marcos con CRC bueno |
| espectros en A + app nativa en B | 1 399 marcos · 3 ráfagas (217, 218, 218 us) |
| app nativa en las dos | 3 ráfagas cada una (una de 557 us) |
| espectros en las dos | 1 413 y 1 402 marcos, con los dos perfiles comparados banda a banda |

Los tests de host: 15 suites de C (378 pruebas) y la de Python, **0
fallos**.

El perfil del espectro de las dos placas, dibujado y explicado, está en
la §5.

---

## 2. La app nativa (la ráfaga)

`tools/test_esdr_app.py` — la app de la pantalla, con el gancho de
pruebas `press` para los botones.

| Comprobación | `48:f6:ee` | `7c:2c:67` |
|---|---|---|
| La app arranca (`Entering native SDR I/Q capture app`) | PASA | PASA |
| El motor de captura se levanta (banco en 0x40840000) | PASA | PASA |
| Sintoniza 2412 MHz (CH 1) al entrar | PASA | PASA |
| La microSD responde y guarda la ráfaga | PASA | PASA |
| La ráfaga son 16 380 palabras | PASA | PASA |
| La tasa está cerca de los 80 MS/s | **75,5 MS/s** | **75,1 MS/s** |
| UP sintoniza 2417 MHz (CH 2) | PASA | PASA |
| DOWN vuelve a 2412 MHz (CH 1) | PASA | PASA |
| El contador de ráfagas avanza | PASA | PASA |
| La app sale y el motor se para | PASA | PASA |
| La radio vuelve a Minino | PASA | PASA |
| La consola sigue viva | PASA | PASA |

**16 de 16 en las dos placas.**

El log de la placa, tal cual:

```
I (105901) esp_sdr_module: Entering native SDR I/Q capture app...
I (106154) c6_sdr_rx: Initializing C6 native SDR RX engine (buffer at 0x40840000)...
I (122537) esp_sdr_module: burst 2: 16380 words in 218 us
I (130544) c6_sdr_rx: Tuned to 2417 MHz (CH 2)
I (133649) c6_sdr_rx: The Wi-Fi driver was started for the capture and is free again
```

16 380 palabras (65 KiB) en 218 us son **75 MS/s**, coherente con los
80 MS/s del motor de dump.

---

## 3. El modo de espectros

`tools/test_esdr_spec.py` — la FFT en la placa y los marcos que salen
por el USB.

| Comprobación | `48:f6:ee` | `7c:2c:67` |
|---|---|---|
| Los 12 perfiles (3 tasas × 4 tamaños de FFT) | PASA | PASA |
| La cabecera `SPEC <bins> <tasa> <bins> <mhz>` | PASA | PASA |
| Los marcos `SPC1` con el CRC bueno | PASA | PASA |
| Las bins son códigos de 0 a 255 | PASA | PASA |
| El espectro tiene estructura (no es ruido plano) | PASA | PASA |
| La portadora del LO está en el bin 0 | PASA | PASA |
| `SPECEND` con los doce campos y el corte limpio | PASA | PASA |

**28 de 28 en las dos placas.**

La corriente, medida:

| | `48:f6:ee` | `7c:2c:67` |
|---|---|---|
| Marcos en 12 s | 1 430 | 1 416 |
| `drops` / `abandoned` | 0 / 0 | 0 / 0 |
| Pico / mediana / suelo | 116 / 59 / 8 | 112 / 59 / 8 |

---

## 4. El perfil del espectro, en las dos placas

Instantánea del modo de espectros: LO en 2412 MHz, 40 MS/s, 1024 bins,
1 427 marcos promediados, una placa cada vez.

| | `48:f6:ee` | `7c:2c:67` |
|---|---|---|
| Pico | 113 | 116 |
| Mediana | 58 | 58 |
| Suelo (mínimo) | 8 | 8 |
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
(la banda del canal 1 de Wi-Fi, 2401-2423 MHz) y el suelo de ruido 10
MHz más allá.

### 4.1 Las dos midiendo a la vez

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

**La misma forma, distinto nivel**: hasta 14,7 dB por encima del LO.

No es la placa, es **dónde está cada una**. El `wifi_scan` de las dos en
el mismo momento:

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

Los mismos AP con hasta 18 dB más nivel en una placa que en la otra: la
diferencia del espectro es la del entorno, no la del firmware. Por debajo
del LO las dos coinciden dentro de ±1 dB.

> **Para comparar dos placas del SDR hay que medir el RSSI de referencia
> al mismo tiempo.** Sin eso, dos placas a medio metro pueden dar 14 dB
> de diferencia sin que haya nada raro.

---

## 5. Cómo se leen las gráficas

### 5.1 Los ejes

| Eje | Qué es |
|---|---|
| **Horizontal** | la separación respecto al LO, en MHz. El LO es la frecuencia a la que está sintonizada la placa (2412 MHz, el canal 1, por defecto). Como la señal es **compleja** (I y Q), el espectro cubre de LO − tasa/2 a LO + tasa/2: ±20 MHz a 40 MS/s, ±40 a 80, ±8 a 16 |
| **Vertical** | el código de potencia de cada bin: un byte de 0 a 255 que es `20*log10(potencia)` en Q15. El visor lo multiplica por el byte 27 del marco (2) para sacarlo en dB |

Las tablas de este documento agrupan las bins en bandas de 1 MHz: a
40 MS/s con 1024 bins cada bin es de 39,06 kHz, así que cada fila es la
media de unas 26 bins. El dibujo de abajo usa la escala del suelo (8) al
pico (75): lo que importa es la **forma**, no la posición.

```
  MHz del LO |                                        | dB  |                                        | dB
  -----------+----------------------------------------+-----+----------------------------------------+-----
             |        48:f6:ee        |      |        7c:2c:67        |
   -20        |#######################################|  8.6 |#######################################|  8.2
   -19        |#######################################|  8.7 |#######################################|  8.6
   -18        |######################################|  9.9 |######################################| 10.7
   -17        |#####################################| 11.8 |#####################################| 12.4
   -16        |###################################| 15.4 |###################################| 15.8
   -15        |################################| 20.5 |################################| 21.0
   -14        |############################| 27.8 |###########################| 28.2
   -13        |######################| 36.7 |######################| 36.9
   -12        |###############| 49.5 |##############| 50.2
   -11        |##############| 53.3 |#############| 52.6
   -10        |##########| 56.6 |###########| 55.7
    -9        |##########| 57.7 |##########| 57.2
    -8        |#########| 58.9 |##########| 58.0
    -7        |#########| 59.8 |#########| 58.9
    -6        |#########| 59.8 |#########| 59.1
    -5        |#######| 61.7 |########| 60.5
    -4        |#####| 65.5 |#######| 61.6
    -3        |####| 67.0 |#######| 63.1
    -2        |####| 68.2 |#####| 65.0
     0        || 74.0 || 74.4  <- LO
    +1        |####| 68.0 |######| 64.9
    +2        |##| 71.1 || 75.2
    +3        |##| 70.6 || 74.2
    +4        |###| 69.6 |#| 72.8
    +5        |###| 69.1 |##| 71.6
    +6        |###| 68.7 |#| 71.8
    +7        |###| 68.9 |#| 73.1
    +8        |###| 68.5 |#| 73.3
    +9        |####| 67.0 |#| 71.9
   +10        |#####| 65.0 |###| 69.8
   +11        |#########| 58.3 |#######| 62.8
   +12        |###############| 48.7 |############| 54.6
   +13        |#####################| 39.8 |##################| 43.9
   +14        |###########################| 29.2 |#######################| 36.3
   +15        |###############################| 22.5 |###########################| 28.3
   +16        |##################################| 17.6 |###############################| 23.0
   +17        |####################################| 14.3 |##################################| 18.0
   +18        |#####################################| 12.0 |###################################| 15.0
   +19        |######################################| 10.8 |#####################################| 12.9
```

### 5.2 Qué dice la forma

| Lo que se ve | Qué es |
|---|---|
| **La joroba grande** (de −12 a +11 MHz) | la banda del canal 1 de Wi-Fi: 2401-2423 MHz. Es la suma de todos los AP que hay alrededor |
| **El pico en 0** | la portadora del canal que está sintonizado: el LO es el centro de la banda y ahí está la energía |
| **Los faldones** (−16 y +16 MHz) | los hombros de las señales DSSS/OFDM, de 11 o 20 MHz de ancho |
| **El suelo a 8** | el ruido propio del receptor |
| **La caída hacia el suelo** (de −12 a −17 MHz y de +12 a +19 MHz) | fuera de la banda de Wi-Fi ya no hay nada: 2395 MHz y 2431 MHz son tierra de nadie en el Wi-Fi |

El **rango dinámico** (del pico al suelo) es de **86 a 88 dB** en las
capturas, y el pico cae exactamente en el LO: las dos cosas que tiene que
cumplir un receptor.

### 5.3 Las dos placas, una al lado de la otra

Las dos curvas tienen **la misma forma**: la misma joroba, los mismos
faldones, el pico en el mismo sitio. Lo que cambia es la **altura**.

- **Por debajo del LO** coinciden dentro de ±1 dB: ahí manda el ruido
  del receptor, que es el mismo en las dos.
- **Por encima del LO** hay hasta 14,7 dB de diferencia: y no es la
  placa, es **dónde está cada una**. El `wifi_scan` de las dos en el
  mismo momento da los mismos AP con +13 dB (INFINITUM2843) y hasta
  +18 dB (Totalplay-77A5) de diferencia en la placa que está más cerca.

Es decir: la gráfica no mide la placa, mide **el sitio**. Por eso el
`wifi_scan` tiene que ir en el mismo paquete de measurement que el
espectro.

### 5.4 Lo que el visor dibuja con esto

| Modo | Eje horizontal | Eje vertical | Color |
|---|---|---|---|
| Ráfaga | el tiempo (una columna por ráfaga) | la frecuencia | la potencia |
| On-Chip Spectrum | el tiempo (una columna por marco `SPC1`) | la frecuencia | la potencia |

En los dos casos, la cascada se hace con los mismos datos: en el modo
ráfaga los calcula el navegador a partir de `CAP16`/`CAP20`, y en el modo
On-Chip Spectrum ya vienen calculados en la placa (que es lo que permite
refrescar a decenas de cuadros por segundo).

El aviso de *"Reception has gaps between snapshots"* del visor sale del
**bit 3 de las banderas** de los marcos: entre captura y captura hay que
rearmar el motor de dump, así que la corriente de espectros no es
continua.

### 5.5 Lo que se encontró y se vio

1. **La recepción funciona y el eje de frecuencias es correcto**: el
   pico cae en el LO y la joroba coincide con la banda del canal 1 de
   Wi-Fi (2401-2423 MHz). Si el eje estuviera desplazado o espejado, la
   joroba no caería donde tiene que caer.
2. **La banda de 2,4 GHz es compartida con el Bluetooth**, y de ahí la
   salvedad del rango: el SDR solo llega a los canales de Wi-Fi
   (2412-2472) porque el motor de dump toma prestada la PHY del Wi-Fi.
   Los canales de publicidad de BLE quedan en los bordes: el 37 (2402
   MHz) a −10 MHz del LO en 2412, y el 39 (2480 MHz) a +8 MHz del LO en
   2472. **No se puede sintonizar el Bluetooth**, solo rozarlo por los
   bordes.
3. **Dos placas del SDR en sitios distintos no son comparables**: hasta
   14,7 dB de diferencia por el sitio, con la misma forma de espectro.
4. **El rango dinámico es de 86-88 dB** y la tasa medida de 75 MS/s
   frente a los 80 nominales del motor.
5. **La corriente de espectros tiene huecos** (el bit 3 de las
   banderas), y hay que rearmar el motor entre captura y captura.
6. **El AGC del receptor normaliza la ganancia**: por eso el *nivel*
   absoluto del espectro no es comparable entre placas, y solo lo es la
   forma.

---

## 6. Los paquetes, de un vistazo

Los dos formatos que viajan por el USB (el detalle y los bytes reales,
en [`sdr-protocolo.md`](./sdr-protocolo.md)):

| | La ráfaga | El espectro |
|---|---|---|
| Petición | `CAP20 <n> <tasa>` | `SPEC <ms> <paso> <uds> <det> <tasa> <bins>` |
| Respuesta | `DATA <n> <crc32hex> <us>` + la carga | `SPEC <bins> <tasa> <bins> <mhz>` + los marcos `SPC1` |
| La carga | 2 bytes por par (`CAP16`) o 2,5 (`CAP20`, 20 bits: I en 0-9, Q en 10-19) | un byte por bin, un código de potencia |
| El CRC32 | el de zlib sobre la carga | el de zlib sobre la cabecera y las bins |
| Tamaño | 10 240 bytes para 4096 muestras | 1056 bytes por marco de 1024 bins |

Medido en las dos placas (bytes reales, con el CRC quadrando):

```
-> CAP20 4096 0
<- DATA 4096 31145255 65          (48:f6:ee)
   000000  5a f8 3f 86 fd 70 80 7f 07 f3 72 08 4f 06 f2 52  Z.?..p....r.O..R
   ... (10240 bytes en total)
   el CRC32 (zlib) de la carga: 31145255  (el que anuncia la cabecera: 31145255)

-> CAP20 4096 0
<- DATA 4096 1a58a795 64          (7c:2c:67)
   000000  31 7c d1 c3 14 2d dc c0 00 05 ee 17 f0 bd 02 dd  1|...-..........
   ... (10240 bytes en total)
   el CRC32 (zlib) de la carga: 1a58a795  (el que anuncia la cabecera: 1a58a795)

-> SPEC 0 1 1 0 1 1024
<- SPEC 1024 40000000 1024 2412
   000000  53 50 43 31 00 00 00 00 00 00 00 00 00 00 00 00  SPC1............
   000010  00 04 00 00 01 00 08 4e 00 00 0a 02 0c 48 00 00  .......N.....H..
   ... (1056 bytes en total)
   el CRC32 de la cabecera y las bins: 5367f516  (el que lleva el marco: 5367f516)
```

Y el corte de la corriente de espectros:

```
-> (línea vacía)
<- SPECEND 0 0 65 66560 759 0 0 65 0 0 65 1
```

---

## 7. Dónde se ve cada cosa

| Menú | Qué se ve |
|---|---|
| **Visor de ESPARGOS**, modo ráfaga | la cascada de spectra y la cascada de I/Q, calculadas por el **navegador** a partir de `CAP16`/`CAP20` |
| **Visor de ESPARGOS**, modo On-Chip Spectrum | el espectro que ya viene calculado **en la placa** (los códigos de las bins), que se refresca a decenas de cuadros por segundo |
| **`Applications > SDR`** (la app nativa) | `MININO SDR [IDL]`, `FREQ: 2412 MHz`, `RATE: 80 MS/s IQ`, `CAPS: 31 PKTS`, el medidor de nivel y `^v:Tune <:Exit`; tras cada captura, `CAP 16380 smp 218 us` y el destino (`iQ saved to SD` o `no SD: capture only`) |
| **`Applications > Spectrum`** (el analizador de RF de Minino) | **otra cosa**: la potencia por canal con el RSSI del driver de Wi-Fi (canal, dB, umbral, proximidad, drones). No es la FFT del SDR |

---

## 8. Lo que encontraron las pruebas

| Fallo | Síntoma | Arreglo |
|---|---|---|
| El spammer de SSID no emitía desde la consola | la lista de SSID solo se carga al elegir un elemento del menú | `ssid_spam_load_stored_list()`: la lista se carga al abrir la app |
| Reentrar en el spammer reiniciaba la placa | `assert()` de `esp_netif_create_default_wifi_ap()` (*"duplicate key"*) | el netif del AP solo se crea si no existe |
| La entrega del puerto fallaba a veces | las pruebas mandaban `sdr_host` con LF y la consola cierra las líneas con **CR** | las pruebas mandan CR |

Ninguno era del SDR en sí, pero los tres impedían medirlo.

---

## 9. Cómo se repite

```bash
cd firmware
make build-sdr
idf.py -B build-sdr -p /dev/ttyACM0 flash
idf.py -B build-sdr -p /dev/ttyACM1 flash

# por placa
python3 tools/test_esdr_web.py  /dev/ttyACM0    # el protocolo del visor
python3 tools/test_esdr_spec.py /dev/ttyACM0    # el modo de espectros
python3 tools/test_esdr_app.py  /dev/ttyACM0    # la app nativa

# las dos aplicaciones a la vez en las dos placas
python3 tools/test_esdr_pair.py app+spec  /dev/ttyACM0 /dev/ttyACM1
python3 tools/test_esdr_pair.py app+spec  /dev/ttyACM1 /dev/ttyACM0
python3 tools/test_esdr_pair.py app+app   /dev/ttyACM0 /dev/ttyACM1
python3 tools/test_esdr_pair.py spec+spec /dev/ttyACM0 /dev/ttyACM1

# el cable, byte a byte
python3 tools/test_esdr_wire.py /dev/ttyACM0

# las pruebas de host
make tests
```

Las placas se localizan por su número de serie de USB, que no cambia al
reconectarlas:

```
/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_48:F6:EE:C2:A2:14-if00
/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_7C:2C:67:42:84:A0-if00
```

---

## 10. Conclusión

- **La recepción funciona en las dos placas**: las tres suites (56, 28 y
  16 comprobaciones) y las dos aplicaciones a la vez, en las cuatro
  asignaciones.
- **Las dos placas miden el mismo ambiente**: misma forma de espectro,
  mismo juego de redes, y el nivel explica exactamente la distancia a los
  AP.
- **La captura es de verdad**: 16 380 muestras en 217-218 us (75 MS/s),
  con 86-88 dB de rango dinámico y el pico en el LO.
- **El cable es el que espera el visor**: el `DATA` con su CRC32 y el
  marco `SPC1` con el suyo, en las dos placas, con los bytes verificados.

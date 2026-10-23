# El cable del SDR: qué se envía, cómo lo recibe Spectrum y dónde se ve

> Los dos paquetes que viajan entre el host y la placa, con los bytes
> reales que se han medido en las dos Minino, y dónde acaba cada cosa en
> las pantallas.
>
> - **Fecha:** 2026-10-21
> - **Placas:** ESP32-C6 rev v0.2 `48:f6:ee:c2:a2:14` y rev v0.1
>   `7c:2c:67:42:84:a0`
> - **Aplicaciones:** solo las dos del SDR, la app nativa (la ráfaga) y el
>   modo de espectros. Ninguna app de radiación.
> - **Documentos hermanos:** los resultados de las dos aplicaciones
>   [`spectrum-SDR-results.md`](./spectrum-SDR-results.md),
>   [`sdr-resultados.md`](./sdr-resultados.md) (el informe completo),
>   [`sdr-guia.md`](./sdr-guia.md) (uso) y
>   [`esp-sdr-integration.md`](./esp-sdr-integration.md) (el diseño)

Para sacar el intercambio real:

```bash
python3 tools/test_esdr_wire.py /dev/ttyACM0
```

---

## 1. El transporte

El canal es el **USB Serial/JTAG** del C6, que el driver de Minino
presenta como un puerto serie (CDC-ACM). Tres cosas hay que tener en
cuenta:

1. **La entrega del puerto la hace la consola.** El REPL hace eco de lo
   que lee (linenoise) y el protocolo necesita respuestas limpias, así
   que el comando `sdr_host` se ejecuta *dentro* de la tarea de la
   consola: cuando empieza la sesión, la consola está entre dos lecturas
   y el puerto tiene un solo lector.

   ```
   minino> sdr_host
   Serving the ESP-SDR protocol on the USB port.
   Open https://espargos.net/espsdr/app/ and connect to this board.
   The console comes back when the viewer sends RELEASE, or type MININO.
   ```

2. **Durante la sesión no sale ni un log.** El nivel maestro baja a
   `ESP_LOG_NONE`: una línea de log en mitad de una carga binaria la
   corrompe. El radio se sube *antes* de silenciar el log, porque el
   driver de Wi-Fi imprime desde su propio ROM y no pasa por `esp_log`.

3. **La carga va a trozos de 64 bytes.** El anillo del USB Serial/JTAG
   son 256 bytes y `xRingbufferSend()` no acepta un bloque mayor: una
   carga de 40 950 bytes en un solo trozo se queda esperando un hueco que
   no llega y no escribe nada. Por eso `port_write()` trocea
   (`ESDR_TX_CHUNK 64`).

La línea de protocolo termina en **LF**; la línea de **consola** (el
`sdr_host`, el `sdr_capture`) termina en **CR**.

---

## 2. El paquete de la ráfaga (el modo "burst" del visor)

### 2.1 El handshake, en texto

Todo lo de esta fase son líneas ASCII terminadas en LF:

```
-> SYNC 1792629170881
<- SYNC 1792629170881
-> INFO
<- C6SDR 6 burst 16380
-> CAPS
<- CAPS VERSION RXLIMITS HWAGC IQ8 SERIALLEASE TUNEEXT RX40 RX16 UARTBAUD SPEC SPECN SPECCAPS DCT
-> RANGE?
<- RANGE 2412 2472 1
-> FREQ 2412
<- OK
```

| Respuesta | Qué le dice al visor |
|---|---|
| `SYNC <nonce>` | el eco del nonce: la placa está viva y el puerto no tiene basura |
| `C6SDR 6 burst 16380` | la familia del chip y el tamaño máximo de la ráfaga (el visor exige entre 4096 y 16384) |
| `CAPS ...` | las capacidades. El visor esconde los controles que **no** aparecen aquí: `GAIN`, `ALPF` y `GPIO` no se anuncian, así que sus controles no seVEN |
| `RANGE 2412 2472 1` | el rango de sintonía y el paso (los canales de Wi-Fi: el motor de dump toma prestado la PHY del Wi-Fi) |
| `OK` | la sintonía ha hecho |

### 2.2 La cabecera de la captura

```
-> CAP20 4096 0
<- DATA 4096 31145255 65
```

`DATA <muestras> <crc32> <micros>`:

| Campo | Valor | Qué es |
|---|---|---|
| muestras | 4096 | los pares I/Q de la ráfaga |
| crc32 | `31145255` | el CRC32 (zlib) de la carga, en hexadecimal |
| micros | 65 | lo que tardó la captura en el chip |

El visor lo comprueba con una expresión regular exacta
(`/^DATA (\d+) ([0-9a-fA-F]{1,8}) (\d+)$/`) y luego verifica el CRC.

### 2.3 La carga

| Formato | Bytes por par | Cómo va |
|---|---|---|
| `CAP16` | 2 | I en 8 bits con signo y luego Q en 8 bits con signo |
| `CAP20` | 2,5 | 20 bits por par, el bit menos significativo primero: **I en los bits 0-9 y Q en los 10-19** |

La palabra cruda del motor de dump lleva I en los bits 9-0 y Q en los
19-10, así que el `CAP20` es la palabra tal cual:

```
   31        28 27        20 19        10 9         0
  +------------+------------+------------+------------+
  |  (usado)  | índice de |     Q      |     I      |
  |            | ganancia  | (10 bits)  | (10 bits)  |
  +------------+------------+------------+------------+
```

Los primeros bytes de una carga `CAP20` de 4096 muestras (10 240 bytes),
tal cual los manda la placa:

```
000000  5a f8 3f 86 fd 70 80 7f 07 f3 72 08 4f 06 f2 52  Z.?..p....r.O..R
000010  58 af 84 f9 55 b8 bf 06 fe 80 08 00 c9 01 8d f4  X...U...........
000020  ef 47 fa 6b 54 ef c5 f3 5d 7c 3f 06 fe 68 40 60  .G.kT...]|?..h@`
... (10240 bytes en total)
```

Y el CRC32 que anuncia la cabecera es el de esa carga:

```
el CRC32 (zlib) de la carga: 31145255   (el que anuncia la cabecera: 31145255)
```

La otra placa (`7c:2c:67`), con la misma petición:

```
-> CAP20 4096 0
<- DATA 4096 1a58a795 64
000000  31 7c d1 c3 14 2d dc c0 00 05 ee 17 f0 bd 02 dd  1|...-..........
000010  b7 70 be 15 f5 bf 01 c0 1b 09 68 01 41 10 10 dc  .p........h.A...
000020  00 81 0e 11 08 41 c1 11 24 20 d1 43 11 57 f8 50  .....A..$ .C.W.P
... (10240 bytes en total)
el CRC32 (zlib) de la carga: 1a58a795  (el que anuncia la cabecera: 1a58a795)
```

Mismo formato, mismo ancho, otro contenido (el del aire que había en
ese momento) y el CRC cuadra en las dos.

> El CRC32 es el de zlib (polinomio y convenciones iguales), pero
> calculado con una **tabla de 16 entradas** (nibble a nibble): 64 bytes
> de flash en vez de los 1024 de la tabla completa, que en una build que
> pelea por cada byte de heap es la diferencia.

---

## 3. El paquete del espectro (el modo "On-Chip Spectrum")

Aquí la placa no manda muestras: **calcula la FFT** y manda códigos de
potencia. Es lo que permite al visor refrescar a decenas de cuadros por
segundo en vez de a diez.

### 3.1 La cabecera

```
-> SPECINFO?
<- SPECINFO {"continuous":false,"transports":["USB"],"profiles":[[80000000,0,256,1,1,0],[80000000,0,512,1,1,0],[80000000,0,1024,1,1,0],[80000000,0,2048,1,1,0],[40000000,1,256,1,1,0],...
-> SPEC 0 1 1 0 1 1024
<- SPEC 1024 40000000 1024 2412
```

`SPECINFO` da los **12 perfiles**: las tres tasas del C6 (80, 40 y
16 MS/s) × los cuatro tamaños de FFT (256, 512, 1024 y 2048 bins), con
el código de tasa, el paso, las unidades, el detector y el tamaño.

`SPEC <ms> <paso> <uds> <det> <tasa> <bins>` arranca la corriente y la
placa contesta con `SPEC <bins> <tasa> <bins> <mhz>`: los bins, la tasa
en Hz, los bins otra vez y la frecuencia a la que está sintonizada.

### 3.2 El marco `SPC1`

Cada cuadro son 28 bytes de cabecera, las bins en orden natural y 4
bytes de CRC32. Bytes reales de un marco de 1024 bins (1056 bytes en
total):

```
000000  53 50 43 31 00 00 00 00 00 6d d6 3e 00 00 00 00  SPC1.....m.>....
000010  00 04 00 00 01 00 08 4e 00 00 0a 02 0c 50 4f 4f  .......N.....POO
00001c  0c 50 4f 4f 4d 3a 44 48 4b 4f 4a 26 4a 40 3a 4e  .POOM:DHKOJ&J@:N
00002c  50 50 49 3e 48 45 47 47 3b 46 4a 4a 4e 47 37 39  PPI>HEGG;FJJNG79
```

| Offset | Bytes | Campo |
|---|---|---|
| 0 | 4 | la firma `SPC1` |
| 4 | 4 | número de cuadro |
| 8 | 8 | índice de la primera muestra |
| 16 | 4 | muestras que abarca el cuadro (1024) |
| 20 | 2 | FFTs que representa (1) |
| 22 | 1 | banderas: bit 0 detector de máximo, **bit 3 hay huecos entre capturas** (`08`) |
| 23 | 1 | metadatos de ganancia (`4e` = 78) |
| 24 | 2 | cuadros perdidos acumulados (`0000`) |
| 26 | 1 | log2 de las bins (`0a` = 10 → 1024) |
| 27 | 1 | multiplicador del código en dB (`02`) |
| 28 | N | las bins, en orden natural, un byte cada una |
| 28+N | 4 | CRC32 de la cabecera y las bins |

El CRC32, comprobado:

```
el CRC32 de la cabecera y las bins: ce794789  (el que lleva el marco: ce794789)
```

El mismo marco en la otra placa (`7c:2c:67`):

```
000000  53 50 43 31 00 00 00 00 00 00 00 00 00 00 00 00  SPC1............
000010  00 04 00 00 01 00 08 4e 00 00 0a 02 0c 48 00 00  .......N.....H..
00001c  0c 48 00 00 00 57 4d c0 d7 4d 4d 3e ff 00 43 00  .H...WM..MM>..C.
00002c  4d 4a 9a ff ff 00 25 45 45 40 4a 39 3f ff 40 00  MJ....%EE@J9?.@.
el CRC32 de la cabecera y las bins: 5367f516  (el que lleva el marco: 5367f516)
```

La firma, los tamaños, el log2 de las bins (`0a`), el multiplicador
(`02`) y el bit de los huecos (`08`) son los mismos en las dos placas.

**El código de cada bin** es `20*log10(potencia)` en el dominio Q15,
acotado a 0..255, y el visor lo multiplica por el byte 27 (2) para
sacarlo en dB.

### 3.3 El corte

Una línea vacía corta la corriente (es lo que hace el visor al cerrar) y
la placa responde con el resumen:

```
-> (línea vacía)
<- SPECEND 0 0 61 62464 27109 0 0 61 0 0 61 1
```

| Campo | Valor | Qué es |
|---|---|---|
| status | 0 | parada normal |
| detail | 0 | sin error |
| units | 61 | las unidades de captura entregadas |
| pairs | 62 464 | los pares de muestras entregados |
| elapsed_us | 27 109 | el tiempo de la corriente |
| late_max | 0 | el retraso máximo |
| work_max_cycles | 0 | el trabajo máximo por ciclo |
| frames | 61 | los marcos enviados |
| drops | 0 | los cuadros perdidos |
| abandoned | 0 | las capturas abandonadas |
| ffts | 61 | las FFTs calculadas |
| stopped_by_host | 1 | el host paró la corriente |

---

## 4. Cómo los recibe Spectrum

### 4.1 El parser

El comando llega como una línea (LF) y se parsea con las reglas del
firmware del ESPARGOS: un comando con basura detrás se rechaza, una línea
vacía no es un comando. Cada comando tiene su respuesta; lo que no se
reconoce contesta `ERR command`.

El `SPEC` es la excepción: se pasa la línea entera al módulo de
espectros, que valida sus argumentos:

```
SPEC <ms> <paso> <uds> <det> <tasa> <bins> [stats]
```

| Regla | Por qué |
|---|---|
| el paso tiene que ser 1 | el motor de dump no admite otro |
| las unidades, de 1 a 8 | cuántos pares de muestras por FFT |
| el detector, 0 o 1 | máximo o media |
| la tasa, uno de los códigos del C6 | 0 = 80 MS/s, 1 = 40 MS/s, 6 = 16 MS/s |
| las bins, de 256 a 2048 y potencia de dos | el máximo de la FFT del chip |

Un `SPEC` con argumentos malos contesta `ERR spec_args`, y si no hay
memoria para el banco de trabajo, `ERR spectrum_memory`.

### 4.2 La FFT de la placa

- **Radix-2 en tiempo decreciente** (DIF) con la entrada permutada por
  inversión de bits.
- **Cada mariposa divide entre dos**: sin eso un tono fuerte saturaría
  el Q15 a la primera etapa. La ganancia total de la transformada es 1/n.
- La salida va en **orden natural** (no bit-reversed): un espectro con
  las bins en orden equivocado parece plausible en pantalla y no es la
  señal. Lo comprueba el test de host, comparando con un DFT directo.

### 4.3 La cadencia

La corriente no es continua: entre captura y captura hay que rearmar el
motor de dump, y por eso los marcos llevan el **bit 3 de las banderas**
puesto (hay huecos entre capturas). Es lo que el visor enseña como
*"Reception has gaps between snapshots"*.

El bucle cede el paso a la tarea idle cada 20 ms: la tarea idle es la
que alimenta el watchdog de tareas, así que sin cederle el paso un bucle
de espectros largo lo dispara y la placa se reinicia.

Medido en las dos placas (40 MS/s, 1024 bins): **1 399 a 1 430 marcos en
12 s**, con el CRC de todos ellos bueno, `drops` 0 y `abandoned` 0.

---

## 5. Dónde se ve cada cosa

### 5.1 El visor de ESPARGOS (<https://espargos.net/espsdr/app/>)

Dos modos, y cada uno consume un paquete distinto:

| Modo del visor | Qué pide | Qué ve |
|---|---|---|
| **Ráfaga** (el de las cascadas) | `CAP16` / `CAP20` | el navegador calcula la FFT de las muestras y las dibuja: la cascada de spectra y la vista de la I/Q |
| **On-Chip Spectrum** | `SPEC` | el espectro que ya viene calculado en la placa, con sus códigos de potencia: se refresca a decenas de cuadros por segundo |

El handshake es el de la §2.1 y el visor elige el perfil del `SPECINFO`
que coincide con las bins que quiere. Cuando cierra, manda `RELEASE` y
la consola de Minino vuelve.

### 5.2 La app nativa de Minino (`Applications > SDR`)

La pantalla OLED (128x64) tiene esto:

| Página | Qué pone |
|---|---|
| 0 (invertida) | `MININO SDR [IDL]` (o `[RX]` mientras corre) |
| 2 | `FREQ: 2412 MHz` |
| 3 | `RATE: 80 MS/s IQ` |
| 4 | `CAPS: 31     PKTS` (las ráfagas capturadas) |
| y=42..52 | el medidor de nivel: un marco con la barra de RSSI dentro |
| 7 | `^v:Tune <:Exit` |

Y después de cada captura, la línea 6 pone el resultado:

```
CAP 16380 smp 218 us
```

y la 7, el destino: `iQ saved to SD` o `no SD: capture only`.

Los botones: **UP/DOWN** sintonían ±5 MHz (de 2412 a 2472), **RIGHT**
captura y **LEFT** sale (y devuelve la radio a Minino).

### 5.3 El analizador de RF de Minino (`Applications > Spectrum`)

Ojo, esta **no** es la del SDR: es la app de siempre de Minino, que
pinta la potencia **por canal** con el RSSI que da el driver de Wi-Fi
(la pantalla lista canal, dB y umbral, con el gráfico de la escala
−90/−70/−50/−30, la proximidad y los drones). La del SDR es la de
§5.2: la FFT de verdad, hecha con el motor de dump.

---

## 6. Los resultados, solo con estas dos aplicaciones

| Prueba | `48:f6:ee` | `7c:2c:67` |
|---|---|---|
| `tools/test_esdr_web.py` (el protocolo del visor) | 56/56 | 56/56 |
| `tools/test_esdr_spec.py` (el modo de espectros) | 28/28 | 28/28 |
| `tools/test_esdr_app.py` (la app nativa) | 16/16 | 16/16 |

Y las dos aplicaciones a la vez en las dos placas
(`tools/test_esdr_pair.py`):

| Asignación | Placa A | Placa B | Resultado |
|---|---|---|---|
| 1 | app nativa | espectros | app: 3 ráfagas de 16 380 palabras (218, 217, 218 us) · espectros: 1 416 marcos con CRC bueno |
| 2 | espectros | app nativa | espectros: 1 399 marcos · app: 3 ráfagas (217, 218, 218 us) |
| 3 | app nativa | app nativa | 3 ráfagas cada una (una de 557 us) |
| 4 | espectros | espectros | 1 413 y 1 402 marcos, con los dos perfiles comparados banda a banda |

El detalle de todo esto, con las tablas completas, está en
[`sdr-resultados.md`](./sdr-resultados.md).

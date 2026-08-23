# Detector de vigilancia para Minino — diseño

Fecha: 2026-08-23
Estado: aprobado para plan de implementación

## 1. Objetivo

Dotar a Minino de un detector pasivo de infraestructura de vigilancia: cámaras
Flock Safety y lectores de placas (ALPR), body cams, rastreadores personales,
cámaras IP, drones y skimmers. La alerta es local (OLED, buzzer, LEDs) y cada
detección se registra con coordenadas GPS en microSD para mapear después.

El trabajo sintetiza dos proyectos existentes:

- [flock-you](https://github.com/colonelpanichacks/flock-you) (MIT) — el motor de
  detección de Flock con niveles de confianza y el fingerprint de Information
  Elements.
- [eye-spy](https://github.com/simeononsecurity/eye-spy) (Apache-2.0) — el
  alcance amplio de motores de detección y el modelo de score acumulativo.

No es un port: Minino es ESP32-C6 con ESP-IDF en C, y ambos proyectos son
Arduino/PlatformIO sobre ESP32-S3 y ESP32-PICO-D4. Se reimplementa el algoritmo,
no el código.

## 2. Decisiones tomadas

| Decisión | Valor |
|---|---|
| Alcance de detección | Vigilancia amplia (modelo eye-spy), con el motor Flock de flock-you dentro |
| Salida | Alerta local (OLED + buzzer + LEDs) y log GPS a microSD. Sin streaming USB ni dashboard web |
| OPSEC | Pasivo por defecto; scan activo de WiFi como opción en settings |
| Firmas | Tabla base compilada en flash + overlay opcional desde microSD |
| Solape con `trackers_scanner` | Se extrae el disector BLE al componente compartido; no se duplica |
| Pruebas de radio | Con dos placas Minino (emisor + detector). No se depende de auto-loopback |

## 3. Alcance

### Dentro

- Componente `surveillance_detect` con motores BLE y WiFi.
- App de menú con pantallas, buzzer y LEDs.
- Registro CSV con GPS a microSD, compatible con subida a WiGLE.
- Exportación GPX de detecciones tier 4 con coordenada.
- Volcado pcap de la evidencia de tier 3 y 4.
- Overlay de firmas desde microSD.
- Refactor de `trackers_scanner` para usar el disector compartido.
- Tests en host de los matchers y del motor de score.

### Fuera

- Dashboard Flask o cualquier herramienta de host.
- Streaming JSON por USB.
- Detección en 5 GHz (el ESP32-C6 es 2.4 GHz).
- Contribución automatizada a DeFlock (es OpenStreetMap; el GPX es una ayuda,
  no una integración).

## 4. Arquitectura

### 4.1 Componente `firmware/components/surveillance_detect/`

Prefijo `surv_` para no colisionar con `sd_card`. Sin dependencias de OLED,
menús ni pantallas.

```
surveillance_detect/
├── CMakeLists.txt
├── include/
│   ├── surv_types.h            surv_event_t, surv_class_t, surv_proto_t
│   ├── surv_match.h            matchers puros
│   ├── surv_engine.h           score, decay, cooldown, tabla por MAC
│   └── surveillance_detect.h   API pública
├── surv_signatures.c           tablas base en flash
├── surv_overlay.c              parser del overlay de microSD
├── surv_match.c                matchers (sin hardware, testeables en host)
├── surv_engine.c               motor de score y tiers
├── surv_radio.c                planificador de fases BLE/WiFi
└── test_apps/                  Unity en host
```

`surv_match.c` no incluye ninguna cabecera de ESP-IDF fuera de tipos básicos:
es condición para que los tests corran en host.

### 4.2 App `firmware/main/apps/surveillance/`

- `surveillance_module.c/.h` — ciclo de vida, callbacks de botones, escritura
  CSV/GPX/pcap.
- `surveillance_screens.c/.h` — pantallas OLED, siguiendo el patrón de
  `main/apps/ble/trackers/trackers_screens.c`.

Entrada de menú nueva bajo `MENU_APPLICATIONS` (no bajo `MENU_WIFI_APPS`, porque
usa las dos radios), declarada en `main/modules/menus_module/menus_include/menus.h`
y gated por Kconfig como el resto de apps.

### 4.3 Flujo de datos

```
callback promiscuo WiFi ─┐
                         ├─→ matcher rápido ─→ ring buffer (32) ─→ task surv_engine
callback GAP de BLE     ─┘   sin printf/malloc                          │
                                                                        ▼
                                          score + dedupe por MAC ──→ callback de la app
                                                                        │
                                                            ┌───────────┼───────────┐
                                                          OLED       buzzer    CSV/GPX/pcap
```

El callback promiscuo corre en la task de WiFi con restricciones de tiempo real:
solo compara y encola. La escritura a microSD nunca ocurre en la task del motor
—un `sd_card_append_to_file` de decenas de ms detendría el drenaje del ring
buffer y se perderían tramas—; se bufferea en RAM y se vuelca por lotes, igual
que hace `main/modules/gps/wardriving/wardriving_module.c`.

### 4.4 Tasks

| Task | Prioridad | Stack | Responsabilidad |
|---|---|---|---|
| `surv_radio` | 10 | 3 KB | Fases BLE/WiFi y salto de canal |
| `surv_engine` | 5 | 4 KB | Drena el ring buffer, score, dedupe, callbacks |
| (task de WiFi) | — | — | Callback promiscuo: compara y encola |
| (task de la app) | — | — | UI y volcado por lotes a microSD |

## 5. Modelo de detección

Los dos proyectos usan modelos distintos que responden preguntas distintas. Se
implementan ambos.

- **Tier (0–4)**, de flock-you — *"¿qué tan seguro estoy de este dispositivo?"*.
  Propiedad por MAC. Un tier alto nunca se sobrescribe con uno bajo y puede
  saltarse el cooldown de dedupe: sin esa excepción, un tier 1 (que dispara con
  cualquier trama) gana la carrera al cooldown de 5 s y esconde la confirmación
  tier 4 del mismo dispositivo. Determina el tono del buzzer y la etiqueta que
  se guarda.
- **Score**, de eye-spy — *"¿qué tan vigilado está este lugar?"*. Acumulador
  global: suma puntos por clase, decae −1 cada 60 s, con cooldown de 120 s por
  clase para que un dispositivo persistente no acumule sin límite. Determina el
  semáforo (0–2 CLEAR, 3–5 CAUTION, 6+ ALERT).

### 5.1 Evento

```c
typedef struct {
  uint8_t      mac[6];
  surv_class_t klass;    // FLOCK, ALPR, AXON, AIRTAG, CAM, ODID, SKIMMER, ...
  uint8_t      tier;     // 0-4, confianza del método que disparó
  int8_t       rssi;
  uint8_t      channel;  // 0 si es BLE
  surv_proto_t proto;    // BLE | WIFI
} surv_event_t;
```

El motor mantiene una tabla por MAC (cap 200, como flock-you) con `best_tier` y
`methods_seen`, más el acumulador global con su decay.

`tier` y `points` son ejes independientes y no deben derivarse uno del otro. Un
SSID que contiene "flock" es evidencia débil de que *ese* dispositivo sea una
cámara —un SSID es trivial de falsificar— pero es evidencia fuerte de que el
lugar tiene infraestructura Flock. Por eso puede ser tier 0 y a la vez sumar 5
puntos al score. El matching por keyword de SSID va **habilitado** por defecto
(flock-you lo trae apagado), precisamente porque a tier 0 nunca etiqueta un
dispositivo como confirmado.

### 5.2 Tiers de WiFi (de flock-you)

| Tier | Método | Condición |
|---|---|---|
| 4 | `wildcard_probe_ie_sig` | Probe Request + SSID IE de longitud 0 + OUI en `addr2` + firma IE |
| 3 | `wildcard_probe` | Igual, sin coincidencia de firma IE |
| 2 | `oui_addr2` | OUI en `addr2` de cualquier trama |
| 1 | `oui_addr1` / `oui_addr3` | OUI en `addr1` o BSSID; propenso a falsos positivos |
| 0 | `ssid_kw` | Keyword en SSID |

Los tiers 1 y 2 son ecos: un AP cercano respondiendo al probe de una cámara pone
la MAC de la cámara en `addr1`. Se conservan porque cubren estaciones que no
transmiten durante la ventana, pero degradados.

### 5.3 Motores

| Origen | Motores |
|---|---|
| flock-you | Los cinco tiers de la tabla anterior |
| eye-spy | Axon (OUI `00:25:df`), Ray-Ban Meta (UUID `0xFD5F`), Flock BLE por nombre, Flock BLE por mfr ID `0x09C8`, Raven (UUIDs GATT), skimmers HC-03/05/06, ODID BLE (`0xFFFA`), ODID WiFi (NaN a `51:6f:9a:01:00:00`), iBeacon, MeshCore, SoundThinking/ShotSpotter, OUI y SSID de cámaras y ALPR, persistencia de MAC desconocida |
| `trackers_scanner` (ya en Minino) | AirTag, SmartTag, Tile, Apple Nearby |
| Nuevo en Minino | Beacons en promiscuo → BSSID y SSID **sin transmitir** |

El último es una mejora sobre ambos: eye-spy necesita `esp_wifi_scan` (que emite
probe requests y delata al dispositivo) para obtener BSSID/SSID; sniffando
beacons en promiscuo se obtiene lo mismo sin transmitir. flock-you no mira
beacons.

`SOUNDTHINKING_OUIS`, `RAVEN_UUIDS` y el mfr ID `0x09C8` están en el código de
eye-spy pero no en su README; se incluyen.

## 6. Firmas

### 6.1 Tabla base

Una sola tabla con la clase y el peso como datos, en lugar de los cinco arrays
sueltos de eye-spy (`FLOCK_OUIS` 35, `FLOCK_MFR_OUIS` 6, `SOUNDTHINKING_OUIS` 1,
`ALPR_OUIS` 1, `CAM_OUIS` 31 = 74 OUIs):

```c
typedef struct {
  uint8_t      oui[3];
  surv_class_t klass;
  uint8_t      points;   // peso en el score global
  uint8_t      tier;     // techo de confianza para esta firma
} surv_oui_entry_t;
```

El tier del evento lo determina el **método** que disparó (posición de la
dirección, probe wildcard, firma IE), no la tabla. El campo `tier` de la entrada
es un **techo**: las OUIs de fabricante contratista (Liteon, USI), que van en
hardware que no es de Flock, nunca superan tier 1 aunque el método diera más.

74 entradas × 5 B = 370 bytes. Ordenada, con **prefiltro de bitmap de 32 bytes
indexado por el primer octeto**: si el bit no está puesto, la trama se descarta
en una operación. Importa porque el matcher corre en el callback promiscuo con
cada trama recibida.

No se debe añadir un filtro de "saltar MAC localmente administrada": `82:6b:f2`
tiene ese bit puesto y es una cámara Flock confirmada en campo por DeFlockJoplin.

### 6.2 Firma IE, en binario

flock-you construye la firma como string y compara con `strcmp`, dentro del
callback promiscuo, violando su propia regla de no formatear en el hot path. La
firma drive-testeada es:

```
2,12,127,221:506f9a16030103,45,191,221:0050f208000000
```

Aquí se guarda precompilada como tokens y se compara recorriendo los IE de la
trama y el patrón en paralelo:

```c
typedef struct { uint8_t tag; uint8_t vlen; uint8_t vendor[7]; } surv_ie_tok_t;
// {2} {12} {127} {221,7,50 6f 9a 16 03 01 03} {45} {191} {221,7,00 50 f2 08 00 00 00}
```

Mismo resultado, sin formateo de strings en el callback.

Se conservan tal cual los parches de campo de flock-you, que son hallazgos
reales y no ruido:

- *Phantom overflow*: TLVs con longitud imposible, se saltan hasta 16 por trama.
- Resync de TLV cuando el recorrido se desalinea.
- Reintento con la trama recortada en 4 bytes (FCS).
- Canonicalización anclada al prefijo vendor LiteON `221:506f9a16030103`.

### 6.3 Overlay desde microSD

Archivo `/surveil/signatures.csv`, texto plano. `+` añade, `-` quita:

```
+oui,70:c9:4e,flock,5,2
-oui,f8:a2:d6
+ssid,alpr,alpr,4,0
+blename,pigvision,flock,5,0
+uuid,fd5f,glasses,5,0
+iesig,2,12,127,221:506f9a16030103,45,191,221:0050f208000000
```

Se carga una sola vez en `surv_begin()`, nunca desde un callback. Líneas
malformadas se ignoran y se cuentan. Topes: 256 OUIs, 64 keywords, 8 firmas IE.
Sin microSD o sin archivo, funciona con la tabla base. La pantalla de arranque
muestra `sigs: 74 base +12 SD` o `SD: none`.

Que sea texto y no binario permite subir una lista actualizada por WiFi con el
`web_file_browser` que Minino ya tiene, sin sacar la tarjeta ni recompilar.
Ninguno de los dos proyectos de origen puede hacerlo.

Que la firma IE sea actualizable por overlay no es lujo: es lo que va a cambiar
cuando Flock actualice el firmware de sus cámaras por OTA.

## 7. Planificador de radio

El ESP32-C6 tiene una sola antena de 2.4 GHz compartida por WiFi, BLE y 802.15.4.
Con alcance amplio no se puede cubrir todo a la vez, así que el reparto es una
elección explícita del usuario, en settings:

| Perfil | Reparto | Uso |
|---|---|---|
| `Flock/ALPR` | 100% WiFi promiscuo | Wardriving en coche; máxima probabilidad de cazar el probe |
| `Vigilancia` | Ciclo de 20 s: BLE pasivo 6 s → WiFi promiscuo 14 s | Uso general |
| `Trackers` | 100% BLE pasivo | A pie: body cams, AirTags, gafas |

Las cámaras emiten probes wildcard cada ~125 ms saltando canales en ascendente,
así que cada segundo sin escuchar es una cámara potencialmente perdida. El perfil
mixto cede aproximadamente un 30% del airtime de WiFi.

**Transición entre ventanas.** Ambos stacks quedan inicializados; lo exclusivo es
el escaneo: `esp_ble_gap_stop_scanning()` antes de `esp_wifi_set_promiscuous(true)`
y a la inversa. No se hace `esp_bluedroid_deinit`/`esp_bt_controller_deinit` por
ciclo como en eye-spy: fragmenta el heap y cuesta cientos de ms cada 20 s.

**BLE pasivo**: parámetros propios con `BLE_SCAN_TYPE_PASSIVE` y `window ==
interval` (escucha continua). No se usa el default de `bt_gattc`, que hoy es
`BLE_SCAN_TYPE_ACTIVE` (`components/bt_gattc/bt_gattc.c:61`).

**Salto de canal**: 11/6/1 descendente con dwell de 250 ms, el de flock-you,
calibrado contra el salto ascendente de la cámara. En perfil `Vigilancia`, cada
cuatro vueltas se extiende a 13/8/3 para cubrir ODID, como eye-spy.

**Recorte por región**: el set de canales se recorta al `nchan` de la región
configurada. Minino resuelve región en `main/modules/settings/wifi/wifi_regions.c`
y su default es `GLOBAL` = canales 1–11; saltar al 13 ahí no escucha nada.
eye-spy tiene este defecto (hopea a 13 sin comprobar región).

**Scan activo opcional**: cuando el usuario lo habilita en settings, se añade una
ventana de 3 s de `esp_wifi_scan` al final del ciclo. Apagado por defecto. Solo
está disponible en los perfiles `Flock/ALPR` y `Vigilancia`; en `Trackers` la
opción se ignora. Habilitarlo hace que el dispositivo transmita probe requests y
deje de ser indetectable: la pantalla lo indica con un `!` junto al perfil.

**Integración con `radio_selector`**: hoy son dos flags (`platform_configured`,
`stack_initialized`) que solo consultan Zigbee y Thread. Se añade
`RADIO_SELECT_SURVEILLANCE` al enum y, al entrar a la app, si
`radio_selector_is_stack_initialized()` está en true, se avisa y no se arranca,
en lugar de disputar la antena con 802.15.4.

**Parada limpia** al salir: promiscuo off, `esp_wifi_stop()`, scan BLE parado,
tasks borradas y volcado final del buffer CSV. Es el patrón de
`deauth_detector_stop()`.

## 8. Persistencia y exportación

### 8.1 CSV

Se reutiliza el header de procedencia de `wardriving_module` (`ElecCats-1.0`,
formato WigleWifi) y se añaden las columnas propias al final:

```
MAC,SSID,AuthMode,FirstSeen,Channel,Frequency,RSSI,CurrentLatitude,
CurrentLongitude,AltitudeMeters,AccuracyMeters,RCOIs,MfgrId,Type,
Class,Tier,Method,Score
```

Un `cut -d, -f1-14` deja un archivo WiGLE válido para subir; el archivo completo
conserva clase, tier y método. Directorio `surveil/`, junto a `warfi/`, `warbee/`
y `thread/`. Línea de hasta 200 bytes (las 14 columnas WigleWifi más las cuatro
propias no caben en los 150 B de `CSV_LINE_SIZE` que usa wardriving), buffer de
200 líneas y volcado por lotes.

`Type` toma los valores `WIFI` o `BLE`, como WiGLE.

**Sin fix GPS la detección se guarda igual**, con latitud y longitud vacías.
Perder una cámara confirmada porque el GPS aún no fijó sería el peor intercambio
posible; filtrar las filas sin coordenada antes de subir es trivial.

### 8.2 GPX

Archivo `surveil/flock_<fecha>.gpx` con un waypoint por detección tier 4 que
tenga coordenada. Se carga como capa en el editor iD de OpenStreetMap para
colocar nodos en DeFlock.

DeFlock **no** consume CSV: es OpenStreetMap, con nodos etiquetados
(`man_made=surveillance`, tipo, orientación) creados desde su app o desde iD. El
GPX es una ayuda para no repetir el recorrido, no una integración.

### 8.3 Evidencia en pcap

Para tier 3 y 4 se vuelca la trama cruda a `surveil/evidence_<fecha>.pcap`
usando `managed_components/espressif__pcap`, que ya está en el proyecto y que
`wifi_sniffer` ya usa. La copia solo ocurre cuando el match ya disparó, así que
no pesa en el hot path. El archivo se corta a 4 MB por sesión; alcanzado el tope
se dejan de escribir tramas y se avisa en pantalla, sin detener la detección ni
el CSV.

Esto permite abrir la captura en Wireshark y verificar la firma IE a mano en vez
de confiar en el firmware. Ninguno de los dos proyectos de origen guarda la
evidencia. Los mismos volcados sirven como vectores para los tests de host.

## 9. UX

### 9.1 Pantalla principal (SH1106 128×64)

```
SURVEIL  [FLOCK]
████████░░ ALERT
Flock  x3   T4
-52dBm  ch 6
GPS 12sat  SD ok
sigs 74+12
```

Más una lista navegable de detecciones con botones, patrón `trackers_screens`.

### 9.2 Buzzer por tier

Tomado de flock-you, para distinguir el método de oído mientras se maneja:

| Tier | Tono |
|---|---|
| 4 | Chirp ascendente 2000 → 2800 Hz |
| 3 | Chirp ascendente 1400 → 1800 Hz |
| 2 | Blip 1200 Hz |
| 1 | Blip 800 Hz |
| 0 | Blip 600 Hz |

Máscara de silencio por tier en settings, persistida con
`preferences_put_uchar("surv_beep", mask)`. Silenciar afecta solo al buzzer: la
detección se registra igual.

### 9.3 LEDs

eye-spy pinta el semáforo en un NeoPixel RGB. Minino no tiene RGB:
`components/leds/include/leds.h` expone `LED_LEFT` y `LED_RIGHT` con brillo, sin
color. El semáforo va en pantalla y los LEDs codifican el nivel por cadencia:

| Nivel | LEDs |
|---|---|
| CLEAR (0–2) | Apagados |
| CAUTION (3–5) | `led_start_breath` lento |
| ALERT (6+) | `led_start_blink` rápido |

### 9.4 Aviso regional

Flock Safety se despliega en Estados Unidos y, en menor medida, Canadá. Con la
tabla base, en México la app puede correr una hora sin una sola detección, y eso
es el resultado correcto, no un fallo. La pantalla de ayuda de la app lo dice
explícitamente y remite al overlay de microSD para cargar firmas locales.

## 10. Cambios en código existente

| Archivo | Cambio |
|---|---|
| `components/trackers_scanner/trackers_scanner.c` | El disector de advertising BLE se mueve a `surv_match.c`; el componente lo llama y conserva su lista y su UI. Pasa a scan pasivo |
| `components/bt_gattc/bt_gattc.c` | Permitir parámetros de scan pasivo sin cambiar el default de los demás consumidores |
| `components/radio_selector/` | Nuevo `RADIO_SELECT_SURVEILLANCE` y su setter |
| `main/modules/menus_module/menus_include/menus.h` | Entrada de menú y `menu_idx` nuevos |
| `main/modules/settings/` | Perfil de radio, toggle de scan activo, máscara de buzzer |

El refactor de `trackers_scanner` (219 líneas más su app) es la parte con riesgo
de regresión y debe ir acompañada de una verificación manual de la app de
trackers antes de continuar.

## 11. Pruebas y criterios de aceptación

### 11.1 Tests en host (Unity)

Con el patrón `test_apps` que ya usa `components/console`, enganchados al CI
existente (`.github/workflows/builds.yml`):

- Cada OUI de la tabla base matchea su clase.
- Exclusión mutua: ningún OUI aparece en dos clases.
- Keywords de SSID case-insensitive; nombres BLE por substring.
- Parser del overlay: línea válida, malformada, `-` que quita, tope de entradas,
  archivo ausente, archivo vacío.
- Fingerprint IE contra vectores de trama reales, incluidos phantom overflow,
  resync de TLV, y trama con y sin FCS.
- Motor: decay de −1/60 s, cooldown de 120 s por clase, un tier alto no se
  degrada, un tier alto salta el dedupe.

Los vectores de trama salen de los volcados pcap de la sección 8.3.

### 11.2 Pruebas de radio con dos placas

El parser se prueba en host inyectando el buffer directamente. La radio, los
canales y el timing requieren **dos Minino**: una emitiendo tramas sintéticas con
`esp_wifi_80211_tx` (ya se usa en `ssid_spam` y `drone_id/spoofer`) y otra
detectando. No se depende de que una sola placa se reciba a sí misma en modo
promiscuo: la radio es half-duplex y no está previsto que funcione.

### 11.3 Validación sin cámaras Flock cerca

El overlay lo resuelve: se añade el OUI de un dispositivo propio a
`/surveil/signatures.csv` y se comprueba que dispara el tier correcto y que la
línea CSV sale bien formada. El tier 4 se valida reproduciendo con la segunda
placa una trama del dataset. Sin esta ruta, "no detecta nada" es indistinguible
de "está roto".

### 11.4 Criterios de aceptación

| Criterio | Umbral |
|---|---|
| Falsos positivos con tabla base, 30 min en zona urbana de México | 0 |
| Detección de trama tier-4 sintética con hop activo | < 2 s |
| Overflow del ring buffer en sesión de 1 h | 0 (contador visible en pantalla de debug) |
| Bloqueo del drenaje del ring buffer por escritura a microSD | < 50 ms |
| App de trackers tras el refactor | Sin regresión, verificada a mano |

## 11.5 Orden de implementación

El trabajo es grande y conviene ejecutarlo en este orden, cada etapa verificable
por sí sola:

1. `surv_match` + `surv_signatures` + tests en host. Sin radio, sin UI.
2. Refactor de `trackers_scanner` sobre el disector compartido, con verificación
   manual de la app de trackers. Riesgo de regresión aislado y resuelto pronto.
3. `surv_engine` (score, tiers, dedupe) + tests en host.
4. `surv_radio` y los motores WiFi/BLE sobre hardware, perfil `Flock/ALPR`
   primero.
5. App, pantallas, buzzer y LEDs.
6. Persistencia: CSV, luego GPX, luego pcap.
7. Overlay de microSD.
8. Pruebas de radio con dos placas y validación de los criterios de aceptación.

## 12. Licencias y créditos

Minino es GPL-3.0. MIT (flock-you) y Apache-2.0 (eye-spy) son compatibles hacia
GPLv3, conservando los avisos de copyright. Los créditos se preservan en las
cabeceras de los archivos que derivan de cada investigación y en la pantalla de
About:

- **OrdoOuroboros / @NitekryDPaul** — lista de OUIs de Flock y la técnica de
  detección por `addr1`.
- **DeFlockJoplin** — firma de probe request wildcard, fingerprint de IE y el
  OUI `82:6b:f2`.
- **Lucia Pintor y Luigi Atzori** — *Analysis of Wi-Fi Probe Requests Towards
  Information Element Fingerprinting*, GLOBECOM 2022,
  doi:10.1109/GLOBECOM48099.2022.10001618.
- **colonelpanichacks** — flock-you.
- **SimeonOnSecurity** — eye-spy.
- **DeFlock (FoggedLens)** — datos y metodología de localización de ALPR.

## 13. Riesgos

| Riesgo | Mitigación |
|---|---|
| El refactor de `trackers_scanner` rompe una app que hoy funciona | Verificación manual antes de seguir; el refactor va primero y aislado |
| Randomización de MAC deja ciegos los tiers 1–2 | Documentado; el tier 4 no depende del OUI solo |
| La firma IE deja de coincidir tras una OTA de Flock | Firma actualizable por overlay de microSD |
| Contención de radio con 802.15.4 | Comprobación de `radio_selector_is_stack_initialized()` al entrar |
| Desgaste de la microSD | Volcado por lotes de 200 líneas, como wardriving |
| Expectativa de detecciones en México | Aviso regional en la pantalla de ayuda |

## 14. Fuera de este diseño, posible después

- Streaming JSON por USB para un dashboard de host.
- Detección de Flock por BLE (dejó de funcionar en primavera de 2026 según
  flock-you; se reevalúa si vuelve).
- Importación directa del formato de export web de WiGLE como overlay.

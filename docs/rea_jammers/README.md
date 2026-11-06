# REA de Jammers 2.4 GHz y Port a Minino

Documentación del análisis de ingeniería inversa (REA) sobre arquitecturas de jammers en 2.4 GHz y diseño de contramedidas / pruebas de laboratorio para Minino.

> [!danger] Sobre el uso de radiofrecuencia
> La interferencia activa (jamming) en 2.4 GHz es ilegal en operación en la mayoría de jurisdicciones. Este análisis se realiza exclusivamente para:
> 1. Detección pasiva y análisis forense de firmas espectrales anti-jammer.
> 2. Pruebas de estrés y resiliencia en laboratorio controlado sobre equipos propios.

## Resumen del análisis de arquitectura

Del análisis de implementaciones basadas en ESP32 clásico (como BlueJammer) y transceptores SPI:
1. **Doble transceptor nRF24L01+** dedicados en buses HSPI y VSPI para cubrir barrido rápido de canales en 2.4 GHz.
2. **Uso de primitivas raw en 802.11** (`net80211` con tramas beacon y deauth) emitidas directamente por la interfaz Wi-Fi.
3. El tráfico BLE/BT en dispositivos dedicados opera comúnmente a nivel de radio física sin stack completo.

## Port e Implementación en Minino

El ESP32-C6 de Minino no dispone de hardware nRF24 ni soporte Bluetooth clásico. La técnica aplicable dentro del marco legal y de laboratorio propio es la **saturación de canales BLE publicitarios** (`Saturate Lab`):

- **Modo en stack**: Añadido en `firmware/components/bt_spam/bt_spam.{h,c}` como `BT_SPAM_MODE_SATURATE`.
- **Parámetros de saturación**: Dwell de 100 ms (dos slices de 50 ms), rotación de MAC cada 20 ciclos e intervalo publicitario fijado al piso mínimo normativo de 20 ms (`0x20`).
- **Control de interfaz**: Accesible desde el menú OLED (`Bluetooth > Spam > Saturate`) y terminal serie (`spam_saturate`).
- **Objetivo técnico**: Evaluación de resiliencia del scanner del receptor en laboratorio y prueba de detección de `Tracker Scanner` ante alta densidad publicitaria en los canales 37, 38 y 39.

## Pruebas

El build por defecto incluye Zigbee/Thread y deja ~20 KB libres después de
inicializar BLE. `bt_spam` rechaza arrancar por debajo de 18 KB para evitar que
el controlador falle al habilitar advertising; después de usar Thread
Sniffer/Broadcast el heap puede quedar por debajo de ese umbral. La prueba
mínima es lanzar `spam_saturate`, comprobar que aparece `start_adv task started`,
verificar anuncios BLE con un scanner y salir con `exit_app` comprobando que
WiFi vuelve a inicializarse.

## Licencias

- Implementación `BT_SPAM_MODE_SATURATE` y adaptaciones en firmware: GPLv3 (Minino / Electronic Cats).

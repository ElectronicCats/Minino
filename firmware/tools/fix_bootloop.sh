#!/usr/bin/env bash
set -e

PORT="${1:-/dev/ttyACM0}"

echo "=== Minino Bootloop Fix & Dual-Slot Restore ==="
echo "Buscando puerto serial..."

for i in {1..30}; do
  if [ -e "$PORT" ]; then
    echo "Puerto $PORT detectado!"
    break
  fi
  # Probar alternativas
  if [ -e "/dev/ttyACM1" ]; then
    PORT="/dev/ttyACM1"
    echo "Puerto $PORT detectado!"
    break
  fi
  sleep 0.5
done

if [ ! -e "$PORT" ]; then
  echo "ERROR: No se detectó la placa en $PORT."
  echo "Por favor mantén presionado el botón BOOT y reconecta el cable USB."
  exit 1
fi

echo "1. Reseteando otadata a slot 0 (0x15000)..."
python3 -m esptool --chip esp32c6 -p "$PORT" -b 460800 write_flash 0x15000 firmware/build/ota_data_initial.bin

echo "2. Flasheando Minino funcional en slot 1 (0x440000) para eliminar la imagen vieja..."
python3 -m esptool --chip esp32c6 -p "$PORT" -b 460800 write_flash 0x440000 firmware/build/minino.bin

echo "3. Asegurando Minino funcional en slot 0 (0xa0000)..."
python3 -m esptool --chip esp32c6 -p "$PORT" -b 460800 write_flash 0xa0000 firmware/build/minino.bin

echo "=== ¡RESTAURACIÓN COMPLETADA CON ÉXITO! ==="

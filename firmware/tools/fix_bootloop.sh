#!/usr/bin/env bash
set -e

PORT="${1:-/dev/ttyACM0}"

echo "=== Minino Bootloop Fix & Dual-Slot Restore ==="
echo "Searching for serial port..."

for i in {1..30}; do
  if [ -e "$PORT" ]; then
    echo "Port $PORT detected!"
    break
  fi
  # Try alternatives
  if [ -e "/dev/ttyACM1" ]; then
    PORT="/dev/ttyACM1"
    echo "Port $PORT detected!"
    break
  fi
  sleep 0.5
done

if [ ! -e "$PORT" ]; then
  echo "ERROR: Board not detected on $PORT."
  echo "Please hold down the BOOT button and reconnect the USB cable."
  exit 1
fi

echo "1. Resetting otadata to slot 0 (0x15000)..."
python3 -m esptool --chip esp32c6 -p "$PORT" -b 460800 write_flash 0x15000 firmware/build/ota_data_initial.bin

echo "2. Flashing working Minino to slot 1 (0x440000) to clear the old image..."
python3 -m esptool --chip esp32c6 -p "$PORT" -b 460800 write_flash 0x440000 firmware/build/minino.bin

echo "3. Ensuring working Minino in slot 0 (0xa0000)..."
python3 -m esptool --chip esp32c6 -p "$PORT" -b 460800 write_flash 0xa0000 firmware/build/minino.bin

echo "=== RESTORE COMPLETED SUCCESSFULLY! ==="

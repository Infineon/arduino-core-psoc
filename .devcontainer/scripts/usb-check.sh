#!/usr/bin/env bash
# Diagnose USB device visibility (serial ports and USB bus access)
set -euo pipefail

echo "[devcontainer] Checking USB access..."

# List USB devices if lsusb is available
if command -v lsusb >/dev/null 2>&1; then
  echo "[devcontainer] USB devices visible to the container:"
  lsusb || true
else
  echo "[devcontainer] NOTICE: lsusb is unavailable; install usbutils for USB diagnostics."
fi

# Check for serial device files (ACM and USB serial adapters)
serial_devices=()
for device_pattern in /dev/ttyACM* /dev/ttyUSB*; do
  [[ -e "$device_pattern" ]] && serial_devices+=("$device_pattern")
done

if ((${#serial_devices[@]} > 0)); then
  echo "[devcontainer] Serial devices visible: ${serial_devices[*]}"
else
  echo "[devcontainer] NOTICE: No /dev/ttyACM* or /dev/ttyUSB* devices are visible."
fi

# Check if USB bus filesystem is mounted (required for raw USB access)
if [[ -d /dev/bus/usb ]]; then
  echo "[devcontainer] USB bus is mounted at /dev/bus/usb."
else
  echo "[devcontainer] NOTICE: /dev/bus/usb is unavailable; use hardware mode for USB passthrough."
fi
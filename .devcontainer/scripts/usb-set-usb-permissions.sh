#!/usr/bin/env bash
# Configure USB permissions for supported programmers and Arduino boards
set -euo pipefail

# Find supported USB devices by vendor:product ID.
# 04b4:f155: KitProg3 programmer
# 2341:1002: Arduino Uno
kitprog_devices=()
while read -r device_path; do
  [[ -e "$device_path" ]] && kitprog_devices+=("$device_path")
done < <(
  lsusb | awk '$6 == "04b4:f155" || $6 == "2341:1002" {
    printf "/dev/bus/usb/%s/%s\n", $2, substr($4, 1, 3)
  }'
)

if ((${#kitprog_devices[@]} == 0)); then
  echo "[devcontainer] No supported USB devices found."
  exit 0
fi

# Find serial device files associated with the programmer
serial_devices=()
for device_path in /dev/ttyACM*; do
  [[ -e "$device_path" ]] && serial_devices+=("$device_path")
done

# Grant read/write access to the container user
sudo chmod a+rw "${kitprog_devices[@]}" "${serial_devices[@]}"
echo "[devcontainer] USB access configured for supported devices."
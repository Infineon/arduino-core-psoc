#!/usr/bin/env bash
# Post-create hook: runs all devcontainer setup scripts in order
set -euo pipefail

script_dir="$(cd "$(dirname "$0")" && pwd)"

# Initialize USB, install repo tooling, and configure local development environment.
bash "$script_dir/scripts/usb-set-usb-permissions.sh"
bash "$script_dir/scripts/usb-check.sh"
bash "$script_dir/scripts/arduino-user-data-dir.sh"
bash "$script_dir/scripts/dev-tools.sh"
bash "$script_dir/scripts/dev-setup.sh"

echo "[devcontainer] Setup complete."

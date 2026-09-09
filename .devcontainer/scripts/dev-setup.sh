#!/usr/bin/env bash
# Set up the development environment: run repo setup, configure Arduino, and link PSoC6 core
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$repo_root"

# Run the main repository setup script
echo "[devcontainer] Running core setup..."
bash tools/dev-setup.sh

arduino-cli version

# Replace the installed core with this checkout while retaining its installed version.
psoc6_version="$(arduino-cli core list | awk '$1 == "infineon:psoc6" { print $2; exit }')"
if [[ -z "$psoc6_version" ]]; then
	echo "[devcontainer] infineon:psoc6 is not installed" >&2
	exit 1
fi

arduino_data_dir="$(arduino-cli config get directories.data)"
installed_core_dir="${arduino_data_dir}/packages/infineon/hardware/psoc6/${psoc6_version}"
if [[ ! -d "$installed_core_dir" ]]; then
	echo "[devcontainer] Installed core directory not found: $installed_core_dir" >&2
	exit 1
fi

sudo rm -rf "$installed_core_dir"
sudo ln -s "$repo_root" "$installed_core_dir"
echo "[devcontainer] Linked $installed_core_dir -> $repo_root"
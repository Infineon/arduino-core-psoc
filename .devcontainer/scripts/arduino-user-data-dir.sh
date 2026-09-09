#!/usr/bin/env bash
# Verify that Arduino CLI's configured data directory is accessible, so
# existing and later-installed cores/tools share one supported location.
set -euo pipefail

data_dir="${ARDUINO_DIRECTORIES_DATA:?ARDUINO_DIRECTORIES_DATA must be set}"

mkdir -p "$data_dir"

# Ensure the active user can actually read/write the shared Arduino data area,
# including any subdirs (e.g. staging/libraries) created earlier as root.
user_name="$(id -un)"
user_group="$(id -gn)"
current_owner="$(stat -c '%U:%G' "$data_dir" 2>/dev/null || true)"

if [[ "$current_owner" != "$user_name:$user_group" ]]; then
	if ! chown -R "$user_name:$user_group" "$data_dir" 2>/dev/null; then
		if command -v sudo >/dev/null 2>&1; then
			sudo chown -R "$user_name:$user_group" "$data_dir"
		else
			echo "[devcontainer] Could not grant ownership of Arduino data directory to $user_name: $data_dir" >&2
			exit 1
		fi
	fi
	chmod -R u+rwX "$data_dir" 2>/dev/null || {
		if command -v sudo >/dev/null 2>&1; then
			sudo chmod -R u+rwX "$data_dir"
		fi
	}
fi

if [[ ! -r "$data_dir" || ! -w "$data_dir" || ! -x "$data_dir" ]]; then
	echo "[devcontainer] Arduino CLI data directory is not accessible: $data_dir" >&2
	exit 1
fi

echo "[devcontainer] Arduino CLI data directory ready at $data_dir."
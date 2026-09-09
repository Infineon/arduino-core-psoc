#!/usr/bin/env bash
# Install the repo's required tooling for git hooks and local formatting.
set -euo pipefail

export PATH="$HOME/.local/bin:$PATH"
repo_root="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$repo_root"

if ! command -v uncrustify >/dev/null 2>&1; then
    echo "[devcontainer] Installing uncrustify..."
    sudo apt-get update
    sudo DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends uncrustify
fi

if ! command -v pre-commit >/dev/null 2>&1; then
    echo "[devcontainer] Installing pre-commit..."
    sudo python3 -m pip install --break-system-packages pre-commit
fi

pre-commit install --hook-type pre-commit --hook-type commit-msg

echo "[devcontainer] Git hooks and formatter dependencies are ready."

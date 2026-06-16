#!/usr/bin/env sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$SCRIPT_DIR"

echo "[NovaOS] Cleaning old build..."
make clean >/dev/null 2>&1 || true

echo "[NovaOS] Building ISO..."
make iso

echo "[NovaOS] Done. ISO generated: $SCRIPT_DIR/NovaOS.iso"

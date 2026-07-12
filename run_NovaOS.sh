#!/usr/bin/env sh
set -eu
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ISO="$SCRIPT_DIR/NovaOS.iso"
if [ ! -f "$ISO" ]; then
    echo "[!] NovaOS.iso not found. Run: make iso"
    exit 1
fi
qemu-system-i386 \
    -m 256M \
    -cdrom "$ISO" \
    -boot d \
    -audiodev pipewire,id=snd0 \
    -machine pcspk-audiodev=snd0

#!/bin/bash
set -eu
PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
QEMU_IMG_SOURCE="$PROJECT_ROOT/scripts/utm-qemu-img.c"
QEMU_IMG_BINARY="$PROJECT_ROOT/build/tools/qemu-img"
mkdir -p "$PROJECT_ROOT/build/tools"
if [ ! -x "$QEMU_IMG_BINARY" ] || [ "$QEMU_IMG_SOURCE" -nt "$QEMU_IMG_BINARY" ]; then
    clang -O2 -Wl,-rpath,/Applications/UTM.app/Contents/Frameworks \
        "$QEMU_IMG_SOURCE" -o "$QEMU_IMG_BINARY"
fi
exec "$QEMU_IMG_BINARY" "$@"

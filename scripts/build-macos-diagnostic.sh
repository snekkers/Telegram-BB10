#!/bin/bash
set -eu
PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WORK="$PROJECT_ROOT/build/macos-development"
QMAKE="$WORK/prefix/qt/bin/qmake"
if [ ! -x "$QMAKE" ]; then
    echo "First prepare native dependencies with scripts/build-macos-development.sh" >&2
    exit 1
fi
mkdir -p "$WORK/client"
bash "$PROJECT_ROOT/scripts/build-telegram-host.sh" macos
cd "$WORK/client"
"$QMAKE" "$PROJECT_ROOT/diagnostic/core.pro" QMAKE_MACOSX_DEPLOYMENT_TARGET=11.0
make -j2

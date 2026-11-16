#!/bin/bash
set -eu
PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SOURCE="$PROJECT_ROOT/build/porting/source/td-4c6377aa7386c39217878a9a56d581922ad37ca4"
WORK="$PROJECT_ROOT/build/porting/work"
PREFIX="$WORK/native-prefix"
"$PROJECT_ROOT/porting/native/bb10-cxx" -std=c++17 -O2 -fno-rtti -fno-exceptions \
    -I"$SOURCE/tdutils" -I"$WORK/td-native-arm/tdutils" -I"$PREFIX/include" \
    "$PROJECT_ROOT/tests/qnx-poll-tests.cpp" "$WORK/td-native-arm/tdutils/libtdutils.a" \
    "$PREFIX/lib/libcrypto.a" "$PREFIX/lib/libz.a" -pthread -lsocket -ldl \
    -o "$WORK/qnx-poll-tests"

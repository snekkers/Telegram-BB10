#!/bin/bash
set -eu
PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$PROJECT_ROOT/build/session-tests"
python3 "$PROJECT_ROOT/scripts/prepare-tdlib-linux-tests.py"
docker build --platform linux/arm64 -t berrygram/qt-tests:bookworm -f "$PROJECT_ROOT/tests/Dockerfile.qt" "$PROJECT_ROOT"
docker run --rm --platform linux/arm64 --network none \
    --mount "type=bind,source=$PROJECT_ROOT,target=/source,readonly" \
    --mount "type=bind,source=$PROJECT_ROOT/build/session-tests,target=/work" \
    berrygram/qt-tests:bookworm sh -c \
    'cc -std=c99 -Wall -Wextra -Werror /source/engine/tdjson-host.c -ldl -o berrygram-tdjson && qmake /source/tests/session-tests.pro && make -j2 && ./session-tests -o results.xml,junitxml -o results.qt.xml,xml -o -,txt'

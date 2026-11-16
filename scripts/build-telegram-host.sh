#!/bin/bash
set -eu

PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TARGET_PLATFORM="${1:-macos}"
case "$TARGET_PLATFORM" in
    macos)
        mkdir -p "$PROJECT_ROOT/build/telegram/macos"
        clang -std=c99 -O2 -Wall -Wextra -Werror \
            "$PROJECT_ROOT/engine/tdjson-host.c" \
            -o "$PROJECT_ROOT/build/telegram/macos/berrygram-tdjson"
        ;;
    simulator|device)
        SDK_IMAGE="${BBNDK_IMAGE:-delaya73/bbndk@sha256:26334b9fe9fe3e8d5984a814e2ebadbef94838994dbb768efd78f7b3da418bc4}"
        mkdir -p "$PROJECT_ROOT/build/telegram/$TARGET_PLATFORM"
        docker run --rm --platform linux/amd64 --network none \
            --entrypoint /bin/bash \
            --mount "type=bind,source=$PROJECT_ROOT,target=/source,readonly" \
            --mount "type=bind,source=$PROJECT_ROOT/build,target=/output" \
            "$SDK_IMAGE" /source/scripts/container-build-telegram-host.sh "$TARGET_PLATFORM"
        ;;
    *) echo "Usage: $0 [macos|simulator|device]" >&2; exit 2 ;;
esac
echo "Built: build/telegram/$TARGET_PLATFORM/berrygram-tdjson"

#!/bin/bash
set -eo pipefail
TARGET_PLATFORM="$1"
source /home/admin/bin/bbndk/bbndk-env_10_3_1_995.sh
case "$TARGET_PLATFORM" in
    simulator) COMPILER_VARIANT=gcc_ntox86 ;;
    device) COMPILER_VARIANT=gcc_ntoarmv7le ;;
    *) exit 2 ;;
esac
qcc -V"$COMPILER_VARIANT" -std=c99 -O2 -Wall -Wextra -Werror \
    -c /source/engine/tdjson-host.c -o "/output/telegram/$TARGET_PLATFORM/tdjson-host.o"
qcc -V"$COMPILER_VARIANT" \
    "/output/telegram/$TARGET_PLATFORM/tdjson-host.o" \
    -o "/output/telegram/$TARGET_PLATFORM/berrygram-tdjson"

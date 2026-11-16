#!/bin/bash
set -eu
PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SDK_IMAGE="${BBNDK_IMAGE:-delaya73/bbndk@sha256:26334b9fe9fe3e8d5984a814e2ebadbef94838994dbb768efd78f7b3da418bc4}"
docker run --rm --platform linux/amd64 --network none --entrypoint /bin/bash \
    --mount "type=bind,source=$PROJECT_ROOT,target=/source" "$SDK_IMAGE" -c \
    'source /home/admin/bin/bbndk/bbndk-env_10_3_1_995.sh; rcc -binary /source/resources/emoji.qrc -o /source/assets/emoji.rcc'

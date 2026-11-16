#!/bin/bash
set -eu

TARGET_PLATFORM="${1:-simulator}"
case "$TARGET_PLATFORM" in
    simulator|device) ;;
    *) echo "Usage: $0 [simulator|device]" >&2; exit 2 ;;
esac

PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SDK_IMAGE="${BBNDK_IMAGE:-delaya73/bbndk@sha256:26334b9fe9fe3e8d5984a814e2ebadbef94838994dbb768efd78f7b3da418bc4}"

if ! command -v docker >/dev/null 2>&1; then
    echo "Docker is required. Install Docker Desktop for Apple silicon first." >&2
    exit 1
fi
if ! docker info >/dev/null 2>&1; then
    echo "Docker is not running. Start Docker Desktop and try again." >&2
    exit 1
fi

mkdir -p "$PROJECT_ROOT/build/$TARGET_PLATFORM"
if [ "$TARGET_PLATFORM" = device ]; then
    python3 "$PROJECT_ROOT/scripts/check-qnx-runtime.py"
fi

# Only the build directory is writable. Compilation and packaging need no network.
# The initial image pull, if needed, is handled by Docker before the container runs.
docker run --rm --platform linux/amd64 --network none \
    --entrypoint /bin/bash \
    --mount "type=bind,source=$PROJECT_ROOT,target=/source,readonly" \
    --mount "type=bind,source=$PROJECT_ROOT/build,target=/output" \
    --workdir /source \
    "$SDK_IMAGE" /source/scripts/container-build.sh "$TARGET_PLATFORM"

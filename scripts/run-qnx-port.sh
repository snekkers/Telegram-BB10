#!/bin/bash
set -eu
PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
STAGE="${1:-probe}"
case "$STAGE" in
    probe|generate|dependencies|platform|tdutils|tdjson) ;;
    *) echo "Usage: $0 [probe|generate|dependencies|platform|tdutils|tdjson]" >&2; exit 2 ;;
esac
PORT_ROOT="$PROJECT_ROOT/build/porting"
test -d "$PORT_ROOT/gcc9/gcc-9.3.0-berrycore" || {
    echo "Run python3 scripts/prepare-qnx-port.py first." >&2; exit 1;
}
mkdir -p "$PORT_ROOT/work" "$PORT_ROOT/logs"
python3 "$PROJECT_ROOT/porting/prepare-clang-headers.py"
docker run --rm --platform linux/amd64 --network none \
    --mount "type=bind,source=$PROJECT_ROOT,target=/project,readonly" \
    --mount "type=bind,source=$PORT_ROOT/gcc9/gcc-9.3.0-berrycore,target=/root/qnx800,readonly" \
    --mount "type=bind,source=$PORT_ROOT/source,target=/sources" \
    --mount "type=bind,source=$PORT_ROOT/work,target=/work" \
    --entrypoint /bin/bash berrygram/porting:bookworm \
    /project/porting/build-stage.sh "$STAGE"

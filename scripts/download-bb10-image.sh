#!/bin/bash
set -eu

PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
IMAGE_DIRECTORY="$PROJECT_ROOT/build/emulation"
IMAGE_NAME=BlackBerry10SimulatorBB10_3_2.qcow2
mkdir -p "$IMAGE_DIRECTORY"

if [ ! -f "$IMAGE_DIRECTORY/$IMAGE_NAME" ]; then
    echo "Downloading/resuming the archived BB10 10.3.2 simulator disk (2.14 GB)."
    curl --fail --location --retry 5 --retry-all-errors --continue-at - \
        --connect-timeout 20 --speed-limit 1024 --speed-time 60 \
        --silent --show-error \
        --output "$IMAGE_DIRECTORY/$IMAGE_NAME.download" \
        "https://archive.org/download/black-berry-10-simulator-bb-10-3-2/$IMAGE_NAME"
fi

python3 - "$IMAGE_DIRECTORY/$IMAGE_NAME" <<'PY'
import hashlib
import sys
from pathlib import Path

destination = Path(sys.argv[1])
source = destination if destination.is_file() else Path(str(destination) + ".download")
if source.stat().st_size != 2135883776:
    raise SystemExit("Unexpected image size; the incomplete download has been retained.")
digest = hashlib.sha1()
with source.open("rb") as stream:
    for block in iter(lambda: stream.read(1024 * 1024), b""):
        digest.update(block)
if digest.hexdigest() != "a58392e402ae6de197faa428cb172a998c2f6046":
    raise SystemExit("Image SHA-1 differs from the archive metadata.")
if source != destination:
    source.rename(destination)
print("Verified archive image:", destination)
PY

#!/usr/bin/env python3
"""Fetch a pinned community TDLib binary for account-free local M1 tests only."""

import hashlib
import json
import platform
import urllib.request
import zipfile
from pathlib import Path


VERSION = "1.8.69"
FILENAME = "tdjson-1.8.69-cp310-cp310-macosx_11_0_arm64.whl"
SHA256 = "85772d8a34ac96c3925896af66514846a70208cd81e2eedac8baf290d28a9ee0"
URL = "https://files.pythonhosted.org/packages/05/78/4c0d127299d22ea2e5cce3ba9e0d12028597eba1e1ba57a89221118a8a40/" + FILENAME


def main():
    if platform.system() != "Darwin" or platform.machine() != "arm64":
        raise SystemExit("This development binary is for Apple silicon macOS only.")
    root = Path(__file__).resolve().parent.parent / "build/tdlib/macos"
    root.mkdir(parents=True, exist_ok=True)
    wheel = root / FILENAME
    if not wheel.exists():
        temporary = root / (FILENAME + ".download")
        urllib.request.urlretrieve(URL, temporary)
        if hashlib.sha256(temporary.read_bytes()).hexdigest() != SHA256:
            raise SystemExit("Downloaded wheel SHA-256 does not match the pinned value.")
        temporary.rename(wheel)
    if hashlib.sha256(wheel.read_bytes()).hexdigest() != SHA256:
        raise SystemExit("Existing wheel SHA-256 does not match the pinned value.")
    with zipfile.ZipFile(wheel) as archive:
        for entry in archive.infolist():
            if entry.filename.startswith("tdjson/.dylibs/") and entry.filename.endswith(".dylib"):
                target = root / "lib" / Path(entry.filename).name
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(archive.read(entry))
            elif ".dist-info/licenses/" in entry.filename and not entry.is_dir():
                target = root / "licenses" / Path(entry.filename).name
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(archive.read(entry))
    report = {
        "pypi": f"https://pypi.org/pypi/tdjson/{VERSION}/json",
        "source": "https://github.com/AYMENJD/tdjson",
        "filename": FILENAME,
        "url": URL,
        "sha256": SHA256,
        "purpose": "Local offline engine tests; not a QNX build or production dependency.",
    }
    (root / "wheel-source.json").write_text(json.dumps(report, indent=2) + "\n")
    print(root / "lib" / f"libtdjson.{VERSION}.dylib")


if __name__ == "__main__":
    main()

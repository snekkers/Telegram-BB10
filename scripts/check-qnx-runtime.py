#!/usr/bin/env python3
"""Verify the prebuilt ARM payload before copying it into the application BAR."""
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
RUNTIME = ROOT / "build/device/telegram-runtime"


def verify():
    manifest = json.loads((RUNTIME / "runtime-manifest.json").read_text())
    assert manifest["platform"] == "BlackBerry 10 ARMv7 softfp"
    assert manifest["tdlib_version"] == "1.8.69"
    assert manifest.get("poll_patch_sha256") == hashlib.sha256((ROOT / "porting/tdlib-qnx-poll.patch").read_bytes()).hexdigest()
    assert manifest["link_undefined_symbols_rejected"] is True
    required = {"bin/berrygram-tdjson", "lib/libtdjson.so", "lib/libstdc++.so.6", "lib/libgcc_s.so.1"}
    found = set()
    for entry in manifest["files"]:
        name = entry["path"]
        assert not Path(name).is_absolute() and ".." not in Path(name).parts
        data = (RUNTIME / name).read_bytes()
        assert len(data) == entry["bytes"], name
        assert hashlib.sha256(data).hexdigest() == entry["sha256"], name
        found.add(name)
    assert required <= found
    # Only the three runtime libraries may enter the app, never stale extras.
    assert {"lib/" + p.name for p in (RUNTIME / "lib").iterdir()} == required - {"bin/berrygram-tdjson"}
    assert {p.name for p in (RUNTIME / "licenses").iterdir()} == {
        "TDLib-Boost.txt", "SQLCipher.txt", "OpenSSL-Apache.txt", "zlib.txt",
        "GCC-GPL3.txt", "GCC-Runtime-Exception.txt", "jsmn-MIT.txt",
    }
    print("ARM runtime manifest: hashes and packaging inputs verified")


if __name__ == "__main__":
    verify()

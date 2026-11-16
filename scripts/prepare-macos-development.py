#!/usr/bin/env python3
"""Prepare isolated, hash-pinned native macOS build sources (no QNX mutation)."""
import hashlib
import importlib.util
import json
import tarfile
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
WORK = ROOT / "build/macos-development"
QT = {
    "version": "5.15.19",
    "filename": "qtbase-everywhere-opensource-src-5.15.19.tar.xz",
    "url": "https://download.qt.io/archive/qt/5.15/5.15.19/submodules/qtbase-everywhere-opensource-src-5.15.19.tar.xz",
    "sha256": "51e91c73abacab81e64efd01bf95794e79c0e605ad80947a024769f0dd620a32",
    "root": "qtbase-everywhere-src-5.15.19",
}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    spec = importlib.util.spec_from_file_location("prepare_qnx", ROOT / "scripts/prepare-qnx-port.py")
    helper = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(helper)
    WORK.mkdir(parents=True, exist_ok=True)
    sources = json.loads((ROOT / "porting/sources.json").read_text())
    sources = {name: sources[name] for name in ("tdlib", "openssl", "zlib")}
    sources["qt"] = QT
    for name, item in sources.items():
        existing = ROOT / "build/porting" / item["filename"]
        archive = existing if existing.exists() else WORK / item["filename"]
        if not archive.exists():
            temporary = archive.with_suffix(".download")
            urllib.request.urlretrieve(item["url"], temporary)
            if digest(temporary) != item["sha256"]:
                raise ValueError("SHA256 mismatch: " + name)
            temporary.rename(archive)
        if digest(archive) != item["sha256"]:
            raise ValueError("SHA256 mismatch: " + name)
        destination = WORK / "source" / item["root"]
        if not destination.exists():
            helper.extract(archive, WORK / "source")
        print(name + ": " + str(destination), flush=True)
    (WORK / "sources.json").write_text(json.dumps(sources, indent=2) + "\n")


if __name__ == "__main__":
    main()

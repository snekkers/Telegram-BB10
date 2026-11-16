#!/usr/bin/env python3
"""Pinned community Linux ARM64 TDLib for offline QProcess tests on an M1."""
import hashlib
import json
import urllib.request
import zipfile
from pathlib import Path

FILENAME = "tdjson-1.8.69-cp310-cp310-manylinux2014_aarch64.manylinux_2_17_aarch64.whl"
URL = "https://files.pythonhosted.org/packages/3f/50/8bdf01d9f225f0cd7773ae7729f105d2774075449f7d6a5da773df223ef5/" + FILENAME
SHA256 = "bbf04c8a9038f816bc52b2e34b1d8432e9797968dfb45244c0dacf0d88822530"
ROOT = Path(__file__).resolve().parent.parent / "build/session-tests"


def main():
    ROOT.mkdir(parents=True, exist_ok=True)
    wheel = ROOT / FILENAME
    if not wheel.exists():
        download = wheel.with_suffix(".download")
        urllib.request.urlretrieve(URL, download)
        if hashlib.sha256(download.read_bytes()).hexdigest() != SHA256:
            raise ValueError("Downloaded wheel SHA-256 mismatch")
        download.rename(wheel)
    if hashlib.sha256(wheel.read_bytes()).hexdigest() != SHA256:
        raise ValueError("Cached wheel SHA-256 mismatch")
    with zipfile.ZipFile(wheel) as archive:
        for entry in archive.infolist():
            name = Path(entry.filename).name
            if (entry.filename.startswith("tdjson/") or entry.filename.startswith("tdjson.libs/")) and ".so" in name and name.startswith("lib"):
                path = ROOT / "real-tdlib" / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(archive.read(entry))
            elif ".dist-info/licenses/" in entry.filename and not entry.is_dir():
                path = ROOT / "real-tdlib/licenses" / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(archive.read(entry))
    libraries = list((ROOT / "real-tdlib").glob("libtdjson-*.so.*"))
    if len(libraries) != 1:
        raise ValueError("Expected exactly one wheel TDLib shared library")
    link = ROOT / "real-tdlib/libtdjson.so"
    if link.is_symlink():
        link.unlink()
    link.symlink_to(libraries[0].name)
    report = {"filename": FILENAME, "url": URL, "sha256": SHA256,
              "source": "https://github.com/AYMENJD/tdjson", "purpose": "Offline Linux ARM64 Qt adapter tests only; never packaged for QNX"}
    (ROOT / "wheel-source.json").write_text(json.dumps(report, indent=2) + "\n")
    print("Linux ARM64 test TDLib prepared")


if __name__ == "__main__":
    main()

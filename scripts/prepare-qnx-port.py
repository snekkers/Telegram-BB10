#!/usr/bin/env python3
"""Download pinned porting inputs and extract them under ignored build/porting."""

import hashlib
import json
import subprocess
import tarfile
from pathlib import Path, PurePosixPath


ROOT = Path(__file__).resolve().parent.parent


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def extract(archive, destination):
    destination.mkdir(parents=True, exist_ok=True)
    boundary = destination.resolve()
    with tarfile.open(archive) as source:
        members = source.getmembers()
        for member in members:
            name = PurePosixPath(member.name)
            path = (destination / member.name).resolve()
            if name.is_absolute() or ".." in name.parts or not path.is_relative_to(boundary):
                raise RuntimeError("Archive member escapes destination")
            if not (member.isfile() or member.isdir() or member.issym() or member.islnk()):
                raise RuntimeError("Unsupported special file in archive")
            if member.issym() or member.islnk():
                base = path.parent if member.issym() else destination
                link = (base / member.linkname).resolve()
                if not link.is_relative_to(boundary):
                    raise RuntimeError("Archive link escapes destination")
        source.extractall(destination, members=members)


def main():
    sources = json.loads((ROOT / "porting/sources.json").read_text())
    workspace = ROOT / "build/porting"
    workspace.mkdir(parents=True, exist_ok=True)
    for name, item in sources.items():
        archive = workspace / item["filename"]
        if not archive.exists():
            temporary = archive.with_name(archive.name + ".download")
            subprocess.run([
                "curl", "--fail", "--location", "--silent", "--show-error",
                "--retry", "4", "--retry-all-errors", "--connect-timeout", "20",
                "--continue-at", "-", "--output", str(temporary), item["url"],
            ], check=True)
            if sha256(temporary) != item["sha256"]:
                raise RuntimeError("SHA-256 mismatch for " + name)
            temporary.rename(archive)
        if sha256(archive) != item["sha256"]:
            raise RuntimeError("SHA-256 mismatch for " + name)
        destination = workspace / item["destination"]
        expected = destination / item["root"]
        if not expected.exists():
            extract(archive, destination)
        print(name + ": verified " + str(expected), flush=True)
    tdlib = sources["tdlib"]
    td_source = workspace / tdlib["destination"] / tdlib["root"]
    patch = ROOT / "porting/tdlib-qnx.patch"
    command = ["patch", "-p1", "-i", str(patch)]
    check = subprocess.run(command + ["--forward", "--dry-run"], cwd=td_source, capture_output=True)
    if check.returncode == 0:
        subprocess.run(command + ["--forward"], cwd=td_source, check=True)
    else:
        reverse = subprocess.run(command + ["--reverse", "--dry-run"], cwd=td_source, capture_output=True)
        if reverse.returncode != 0:
            raise RuntimeError("TDLib source no longer matches the QNX patch")
        print("tdlib: QNX patch already applied", flush=True)
    (workspace / "verified-sources.json").write_text(json.dumps(sources, indent=2) + "\n")


if __name__ == "__main__":
    main()

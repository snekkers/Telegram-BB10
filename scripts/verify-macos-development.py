#!/usr/bin/env python3
"""Record native toolchain evidence separately from account/network acceptance."""
import hashlib
import json
import platform
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
WORK = ROOT / "build/macos-development"


def main():
    tests = json.loads((WORK / "diagnostic-tests.json").read_text())
    assert all(test["passed"] for test in tests["tests"])
    tested_at = (WORK / "diagnostic-tests.json").stat().st_mtime
    assert tested_at >= (WORK / "client/berrygram-diagnostic-core").stat().st_mtime, "Rerun native integration tests after rebuilding the client"
    assert tested_at >= (WORK / "tdlib/libtdjson.1.8.69.dylib").stat().st_mtime, "Rerun native integration tests after rebuilding TDLib"
    recording_report = WORK / "recording-tests.json"
    recording = json.loads(recording_report.read_text())
    assert recording["passed"] and not recording["hardware_microphone_used"]
    assert recording_report.stat().st_mtime >= max((ROOT / name).stat().st_mtime for name in
        ("diagnostic/web/app.js", "tests/diagnostic-recording-tests.js")), "Rerun recording tests after changing the UI"
    inputs = {
        "tdlib": WORK / "tdlib/libtdjson.1.8.69.dylib",
        "qtcore": WORK / "prefix/qt/lib/libQt5Core.5.15.19.dylib",
        "diagnostic_core": WORK / "client/berrygram-diagnostic-core",
        "engine": ROOT / "build/telegram/macos/berrygram-tdjson",
    }
    outputs = []
    for name, path in inputs.items():
        architecture = subprocess.check_output(["file", str(path.resolve())], text=True).strip()
        assert "arm64" in architecture, architecture
        dependencies = subprocess.check_output(["otool", "-L", str(path)], text=True)
        assert "libssl.1.1" not in dependencies and "libcrypto.1.1" not in dependencies
        outputs.append({"name": name, "path": str(path.relative_to(ROOT)), "bytes": path.stat().st_size,
                        "sha256": hashlib.sha256(path.read_bytes()).hexdigest(), "dependencies": dependencies.splitlines()[1:]})
    report = {"platform": "macOS " + platform.mac_ver()[0] + " ARM64", "sources": json.loads((WORK / "sources.json").read_text()),
              "files": outputs, "tests": tests["tests"], "recording_tests": recording,
              "not_checked": ["real account login", "real Telegram chat traffic", "BB10 runtime"],
              "visual_qa": {"browser": "Codex in-app browser", "synthetic_data": True,
                            "checks": ["initialization", "chat list", "keyboard send", "media download/display",
                                       "draft restored after chat switch", "page reload", "narrow and desktop layout", "attachment preview and manual recovery", "synthetic M4A preview/player metadata", "persistent selection after full server/core restart", "protected draft after cache cleanup"],
                            "screenshot": "build/macos-development/screenshots/fixture-durable.jpg"}}
    (WORK / "verification.json").write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n")
    print("Native macOS build, dependencies, integration tests and visual QA recorded")


if __name__ == "__main__":
    main()

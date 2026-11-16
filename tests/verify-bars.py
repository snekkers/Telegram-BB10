#!/usr/bin/env python3
"""Inspect packaged artifacts; does not claim BB10 execution or account login."""
import hashlib
import json
import struct
import re
import sys
import time
import zipfile
from pathlib import Path
from xml.etree import ElementTree

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "porting"))
from elf32 import inspect


def main():
    artifacts = []
    for target, arch, machine in [("simulator", "x86", 3), ("device", "armle-v7", 40)]:
        path = ROOT / "build" / target / ("berrygram-" + target + ".bar")
        with zipfile.ZipFile(path) as archive:
            assert archive.testzip() is None
            names = archive.namelist()
            assert len(names) == len(set(names))
            manifest = archive.read("META-INF/MANIFEST.MF").decode()
            assert "Package-Architecture: " + arch + "\n" in manifest
            assert "Application-Development-Mode: true" in manifest
            assert "Package-Version: 0.15.0.20" in manifest
            assert "Entry-Point-Icon: native/assets/icon.png" in manifest
            assert "Entry-Point-User-Actions: access_internet" in manifest
            assert manifest.count("\nEntry-Point: ") == 1
            entry = next(line for line in manifest.splitlines() if line.startswith("Entry-Point: "))
            assert "telegram/lib" not in entry
            for source in (ROOT / "assets").glob("*.qml"):
                assert archive.read("native/assets/" + source.name) == source.read_bytes()
                for reference in re.findall(r'asset:///([^"\s]+)',source.read_text()):
                    assert "native/assets/"+reference in names, reference
            assert archive.read("native/assets/icon.png") == (ROOT / "assets/icon.png").read_bytes()
            for source in (ROOT / "assets").rglob("*"):
                if source.is_file() and source.suffix in {".js", ".png", ".amd", ".txt", ".ogg"}:
                    assert archive.read("native/assets/" + source.relative_to(ROOT / "assets").as_posix()) == source.read_bytes()
            assert archive.read("native/assets/emoji.rcc") == (ROOT / "assets/emoji.rcc").read_bytes()
            binary = archive.read("native/berrygram")
            assert binary[:6] == b"\x7fELF\x01\x01"
            assert struct.unpack_from("<H", binary, 18)[0] == machine
            if target == "device":
                ui = inspect(binary)
                assert "libcpp.so.4" in ui["needed"]
                assert "libbbdata.so.1" in ui["needed"]
                assert "libbbmultimedia.so.1" in ui["needed"]
                assert "libbbcascadespickers.so.1" in ui["needed"]
                assert not {"libstdc++.so.6", "libgcc_s.so.1", "libtdjson.so"} & set(ui["needed"])
                runtime = json.loads(archive.read("native/telegram/runtime-manifest.json"))
                required = {"bin/berrygram-tdjson", "lib/libtdjson.so", "lib/libstdc++.so.6", "lib/libgcc_s.so.1"}
                for item in runtime["files"]:
                    if item["path"] in required:
                        content = archive.read("native/telegram/" + item["path"])
                        assert hashlib.sha256(content).hexdigest() == item["sha256"]
                        inspect(content)
                assert {"native/telegram/" + name for name in required} <= set(names)
                worker_block = manifest.split("Archive-Asset-Name: native/telegram/bin/berrygram-tdjson\n", 1)[1].split("\n\n", 1)[0]
                assert "Archive-Asset-Type: Qnx/Elf" in worker_block
                for license_file in (ROOT / "build/device/telegram-runtime/licenses").iterdir():
                    assert archive.read("native/telegram/licenses/" + license_file.name) == license_file.read_bytes()
            else:
                assert not any(name.startswith("native/telegram/") for name in names)
        artifacts.append({"target": target, "path": str(path.relative_to(ROOT)), "architecture": arch,
                          "elf_machine": machine, "size_bytes": path.stat().st_size,
                          "sha256": hashlib.sha256(path.read_bytes()).hexdigest()})
    tests = ElementTree.parse(ROOT / "build/session-tests/results.xml").getroot()
    assert tests.tag == "testsuite"
    assert tests.get("failures") == "0" and tests.get("errors") == "0"
    qt_tests = ElementTree.parse(ROOT / "build/session-tests/results.qt.xml").getroot()
    incidents = qt_tests.findall(".//Incident")
    assert incidents and all(item.get("type") == "pass" for item in incidents)
    store_tests = ElementTree.parse(ROOT / "build/session-tests/store/results.xml").getroot()
    assert store_tests.get("failures") == "0" and store_tests.get("errors") == "0"
    store_incidents = ElementTree.parse(ROOT / "build/session-tests/store/results.qt.xml").getroot().findall(".//Incident")
    assert store_incidents and all(item.get("type") == "pass" for item in store_incidents)
    avatar_incidents=ElementTree.parse(ROOT / "build/session-tests/avatars/results.qt.xml").getroot().findall(".//Incident")
    assert avatar_incidents and all(item.get("type")=="pass" for item in avatar_incidents)
    voice_incidents=ElementTree.parse(ROOT / "build/session-tests/voice/results.qt.xml").getroot().findall(".//Incident")
    assert voice_incidents and all(item.get("type")=="pass" for item in voice_incidents)
    report = {
        "checked_date": time.strftime("%Y-%m-%d"), "host": "Apple M1 / macOS arm64",
        "scope": "Build/package and Linux Qt checks; device execution is recorded separately in build/porting/device-tests.json",
        "sdk": "BlackBerry Native SDK 10.3.1.995 / Linux amd64 in Docker",
        "checks": ["Qt 4/C++98 compilation and BAR packaging", "ZIP integrity and ELF architecture",
                   "packaged QML matches source", "ARM engine and library hashes match runtime manifest",
                   "single UI entry point and isolated child GNU runtime", "runtime license texts included"],
        "session_tests": {"platform": "Linux ARM64 / Qt 5.15.8 in Docker", "results": "build/session-tests/results.xml",
                          "functions_including_setup_cleanup": int(tests.get("tests")),
                          "passed_rows_including_setup_cleanup": len(incidents), "failures": 0,
                          "real_tdlib": "1.8.69 offline: version, auth state, close; no parameters or account"},
        "avatar_tests": {"passed_rows_including_setup_cleanup": len(avatar_incidents), "failures": 0, "scope": "headless Qt image crop, cache, alpha and invalid/symlink input"},
        "voice_tests": {"passed_rows_including_setup_cleanup": len(voice_incidents), "failures": 0, "scope": "Opus to private bounded WAV; malformed, cancellation and overlong input"},
        "chat_tests": {"platform": "Linux ARM64 / Qt 5.15.8 in Docker; synthetic process",
                       "results": "build/session-tests/store/results.xml",
                       "passed_rows_including_setup_cleanup": len(store_incidents), "failures": 0,
                       "checks": ["exact int53 IDs and int64 positions", "list and history pagination",
                                  "stale history and edit/delete races", "incoming messages and previews",
                                  "pending, acknowledged, sent, read and failed states", "send update/response races",
                                  "timeouts, late replies and no automatic duplicate send", "permissions and data reset", "attachment paths, formats, permissions and recovery", "durable account-scoped outbox, restart and failed write", "cache cleanup protects active and uncertain shared files"]},
        "not_checked_by_this_script": ["Qt 4 JsonDataAccess execution", "BB10 runtime and QML rendering",
                        "Telegram network login", "Telegram chat loading, sending and reconnection",
                        "physical Classic installation"],
        "artifacts": artifacts,
    }
    (ROOT / "build/verification.json").write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n")
    print(json.dumps(report, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()

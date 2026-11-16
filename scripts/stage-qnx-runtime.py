#!/usr/bin/env python3
"""Stage the experimental ARMv7 runtime and offline diagnostics for a BB10 phone."""
import hashlib
import json
import shutil
import sys
import time
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "porting"))
from elf32 import inspect


def main():
    work = ROOT / "build/porting/work"
    library = work / "td-native-arm/libtdjson.so.1.8.69"
    link_command = work / "td-native-arm/CMakeFiles/tdjson.dir/link.txt"
    flags = link_command.read_text()
    if "--no-undefined" not in flags or "--no-allow-shlib-undefined" not in flags:
        raise SystemExit("Rebuild tdjson with the strict shared-library linker flags first.")
    if library.stat().st_mtime < link_command.stat().st_mtime:
        raise SystemExit("TDLib has not been relinked after the latest CMake configuration.")
    checked_headers = 0
    for depfile in (work / "td-native-arm").rglob("*.o.d"):
        dependencies = depfile.read_text()
        if any(old in dependencies for old in (
            "gcc-9.3.0-berrycore/include/openssl/", "gcc-9.3.0-berrycore/include/zlib.h",
            "gcc-9.3.0-berrycore/include/zconf.h",
        )):
            raise ValueError("An object still uses obsolete SDK dependency headers: " + str(depfile))
        checked_headers += 1
    compiler_runtime = ROOT / "build/porting/gcc9/gcc-9.3.0-berrycore/x86_64-linux/arm-blackberry-qnx8eabi/lib64/gcc/arm-blackberry-qnx8eabi/9.3.0"
    inputs = {
        "bin/berrygram-tdjson": ROOT / "build/telegram/device/berrygram-tdjson",
        "bin/openssl-probe": work / "openssl-probe",
        "bin/cxx17-probe": work / "native-probe",
        "lib/libtdjson.so": library,
        "lib/libstdc++.so.6": compiler_runtime / "libstdc++.so.6",
        "lib/libgcc_s.so.1": compiler_runtime / "libgcc_s.so.1",
    }
    entries = []
    system = {"libc.so.3", "libm.so.2", "libsocket.so.3"}
    bundled = {Path(name).name for name in inputs if name.startswith("lib/")}
    required = {"td_create_client_id", "td_send", "td_receive", "td_execute"}
    for name, path in inputs.items():
        data = path.read_bytes()
        elf = inspect(data)
        if name.startswith("bin/") and elf["interpreter"] != "/usr/lib/ldqnx.so.2":
            raise ValueError("Unexpected executable loader: " + name)
        missing = set(elf["needed"]) - system - bundled
        if missing:
            raise ValueError("Unstaged dependencies for " + name + ": " + repr(missing))
        if name == "lib/libtdjson.so" and not required.issubset(elf["exports"]):
            raise ValueError("Missing modern TDLib JSON C API exports")
        del elf["exports"]
        entries.append({"path": name, "bytes": len(data), "sha256": hashlib.sha256(data).hexdigest(), **elf})

    output = ROOT / "build/device/telegram-runtime"
    output.mkdir(parents=True, exist_ok=True)
    for name, path in inputs.items():
        destination = output / name
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, destination)
        destination.chmod(0o755 if name.startswith("bin/") else 0o644)
    td_source = ROOT / "build/porting/source/td-4c6377aa7386c39217878a9a56d581922ad37ca4"
    licenses = {
        "TDLib-Boost.txt": td_source / "LICENSE_1_0.txt",
        "SQLCipher.txt": td_source / "sqlite/sqlite/LICENSE",
        "OpenSSL-Apache.txt": ROOT / "build/porting/source/openssl-3.5.9/LICENSE.txt",
        "zlib.txt": ROOT / "build/porting/source/zlib-1.3.2/LICENSE",
        "GCC-GPL3.txt": ROOT / "porting/licenses/COPYING3",
        "GCC-Runtime-Exception.txt": ROOT / "porting/licenses/COPYING.RUNTIME",
        "jsmn-MIT.txt": ROOT / "engine/vendor/jsmn-LICENSE",
    }
    (output / "licenses").mkdir(exist_ok=True)
    for name, path in licenses.items():
        shutil.copy2(path, output / "licenses" / name)
    shutil.copy2(ROOT / "porting/run-device-probe.sh", output / "run-probe.sh")
    (output / "run-probe.sh").chmod(0o755)
    report = {
        "created_at": time.strftime("%Y-%m-%dT%H:%M:%S%z"),
        "platform": "BlackBerry 10 ARMv7 softfp", "tdlib_version": "1.8.69",
        "upstream_commit": "4c6377aa7386c39217878a9a56d581922ad37ca4",
        "local_patch_sha256": hashlib.sha256((ROOT / "porting/tdlib-qnx.patch").read_bytes()).hexdigest(),
        "link_undefined_symbols_rejected": True, "json_c_api_exports_verified": True,
        "dependency_files_checked_for_header_shadowing": checked_headers,
        "device_execution_tested": False, "telegram_login_tested": False, "files": entries,
    }
    (output / "runtime-manifest.json").write_text(json.dumps(report, indent=2) + "\n")
    (output / "README.txt").write_text(
        "Experimental BB10 ARMv7 runtime. Not an installer or a verified phone build.\n"
        "Copy the whole folder to a BB10 ARM phone with an available native execution transport.\n"
        "Run: sh run-probe.sh\n"
        "The probe checks C++ threads, OpenSSL entropy/SHA-256/TLS context and TDLib startup/close.\n"
        "It does not log in, send account data or establish a Telegram session.\n"
        "Keep stdout, stderr and the exit status when testing. See runtime-manifest.json.\n"
        "The x86 simulator cannot run this ARMv7 bundle.\n"
    )
    archive = output.with_suffix(".zip")
    with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=6) as destination:
        for path in sorted(output.rglob("*")):
            if path.is_file():
                destination.write(path, str(Path(output.name) / path.relative_to(output)))
    with zipfile.ZipFile(archive) as check:
        if check.testzip() is not None:
            raise ValueError("Runtime ZIP integrity check failed")
    report["archive"] = {"path": str(archive.relative_to(ROOT)), "bytes": archive.stat().st_size,
                         "sha256": hashlib.sha256(archive.read_bytes()).hexdigest()}
    (ROOT / "build/porting/verification.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()

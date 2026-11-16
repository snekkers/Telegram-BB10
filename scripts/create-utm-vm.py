#!/usr/bin/env python3
"""Create an isolated experimental UTM VM; BB10 GUI boot is not working yet."""

import argparse
import hashlib
import json
import platform
import plistlib
import shutil
import subprocess
import uuid
from pathlib import Path


ARCHIVE_SHA1 = "a58392e402ae6de197faa428cb172a998c2f6046"
ARCHIVE_SIZE = 2135883776


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image", type=Path)
    parser.add_argument("--bundle", type=Path, default=Path("build/emulation/BB10 Classic.utm"))
    args = parser.parse_args()
    image = args.image.resolve(strict=True)
    bundle = args.bundle.resolve()
    if bundle.exists():
        parser.error("The VM bundle already exists; refusing to overwrite its disk.")
    if image.stat().st_size != ARCHIVE_SIZE:
        parser.error("The simulator download is incomplete or has an unexpected size.")

    checksum = hashlib.sha1()
    with image.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            checksum.update(block)
    if checksum.hexdigest() != ARCHIVE_SHA1:
        parser.error("The simulator SHA-1 does not match the archive metadata.")

    data_dir = bundle / "Data"
    data_dir.mkdir(parents=True)
    disk = data_dir / "bb10.qcow2"
    # APFS clone keeps the original download intact without allocating another
    # full disk immediately. A normal copy is the fallback for other filesystems.
    if platform.system() == "Darwin":
        result = subprocess.run(["/bin/cp", "-c", str(image), str(disk)])
        if result.returncode != 0:
            shutil.copyfile(image, disk)
    else:
        shutil.copyfile(image, disk)

    vm_uuid = str(uuid.uuid4()).upper()
    configuration = {
        "Backend": "QEMU",
        "ConfigurationVersion": 4,
        "Information": {
            "Name": "BB10 Classic",
            "IconCustom": False,
            "Notes": "Experimental BB10 10.3.2 simulator on Apple silicon; archive disk clone.",
            "UUID": vm_uuid,
        },
        "System": {
            "Architecture": "x86_64",
            "Target": "pc",
            "CPU": "qemu64",
            "CPUFlagsAdd": [],
            "CPUFlagsRemove": [],
            "CPUCount": 1,
            "ForceMulticore": False,
            "MemorySize": 2048,
            "JITCacheSize": 256,
        },
        "QEMU": {
            "DebugLog": True,
            "UEFIBoot": False,
            "RNGDevice": False,
            "BalloonDevice": False,
            "TPMDevice": False,
            "Hypervisor": False,
            "TSO": False,
            "RTCLocalTime": False,
            "PS2Controller": True,
            "MachinePropertyOverride": "vmport=on",
            "AdditionalArguments": [],
        },
        "Input": {"UsbBusSupport": "2.0", "UsbSharing": False, "MaximumUsbShare": 0},
        "Sharing": {
            "ClipboardSharing": False,
            "DirectoryShareMode": "None",
            "DirectoryShareReadOnly": True,
        },
        "Display": [{
            "Hardware": "vmware-svga",
            "DynamicResolution": False,
            "NativeResolution": False,
            "UpscalingFilter": "Nearest",
            "DownscalingFilter": "Linear",
        }],
        "Drive": [{
            "Identifier": str(uuid.uuid4()).upper(),
            "ImageName": "bb10.qcow2",
            "ImageType": "Disk",
            "Interface": "IDE",
            "InterfaceVersion": 0,
            "ReadOnly": False,
        }],
        "Network": [{
            "Mode": "Emulated",
            "Hardware": "e1000",
            "MacAddress": "52:54:00:10:03:20",
            "IsolateFromHost": False,
            "PortForward": [
                {"Protocol": "TCP", "HostAddress": "127.0.0.1", "HostPort": host,
                 "GuestPort": guest}
                for host, guest in [(8443, 443), (2222, 22), (2323, 23)]
            ],
        }],
        "Serial": [],
        "Sound": [],
    }
    with (bundle / "config.plist").open("wb") as output:
        plistlib.dump(configuration, output, sort_keys=False)
    report = {
        "source_image": str(image),
        "archive_sha1": ARCHIVE_SHA1,
        "bundle": str(bundle),
        "uuid": vm_uuid,
        "boot_verified": False,
        "status": "Experimental configuration; tested variants stall at START SERVICE: screen.",
    }
    (bundle.parent / "vm-created.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Check the ARM EABI memory-helper contracts using host libc semantics."""
import ctypes
import json
import subprocess
from pathlib import Path

root = Path(__file__).resolve().parent.parent
output = root / "build/porting/work/aeabi-memory-test.dylib"
subprocess.run(["clang", "-dynamiclib", "-fno-builtin", "-O2",
                str(root / "porting/aeabi-memory.c"), "-o", str(output)], check=True)
library = ctypes.CDLL(str(output))
passed = []
for name in ("__aeabi_memcpy", "__aeabi_memcpy4", "__aeabi_memcpy8"):
    method = getattr(library, name)
    method.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t]
    method.restype = None
    source = ctypes.create_string_buffer(bytes(range(32)))
    dest = ctypes.create_string_buffer(b"x" * 32)
    method(ctypes.addressof(dest) + 8, ctypes.addressof(source) + 8, 16)
    assert dest.raw[:32] == b"x" * 8 + bytes(range(8, 24)) + b"x" * 8
    method(dest, source, 0)
    assert dest.raw[:8] == b"x" * 8
    passed.append(name)

for name, offset in (("__aeabi_memmove", 3), ("__aeabi_memmove4", 4), ("__aeabi_memmove8", 8)):
    method = getattr(library, name)
    method.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t]
    method.restype = None
    buffer = ctypes.create_string_buffer(bytes(range(32)))
    method(ctypes.addressof(buffer) + offset, buffer, 20)
    assert buffer.raw[:32] == bytes(range(offset)) + bytes(range(20)) + bytes(range(offset + 20, 32))
    passed.append(name)

for name in ("__aeabi_memset", "__aeabi_memset4", "__aeabi_memset8"):
    method = getattr(library, name)
    method.argtypes = [ctypes.c_void_p, ctypes.c_size_t, ctypes.c_int]
    method.restype = None
    buffer = ctypes.create_string_buffer(b"x" * 32)
    method(ctypes.addressof(buffer) + 8, 9, 0x37)
    assert buffer.raw[:32] == b"x" * 8 + b"7" * 9 + b"x" * 15
    passed.append(name)

for name in ("__aeabi_memclr", "__aeabi_memclr4", "__aeabi_memclr8"):
    method = getattr(library, name)
    method.argtypes = [ctypes.c_void_p, ctypes.c_size_t]
    method.restype = None
    buffer = ctypes.create_string_buffer(b"x" * 32)
    method(ctypes.addressof(buffer) + 8, 9)
    assert buffer.raw[:32] == b"x" * 8 + b"\0" * 9 + b"x" * 15
    passed.append(name)

report = {"host_contract_tests_passed": passed, "qnx_execution_tested": False}
(root / "build/porting/aeabi-memory-results.json").write_text(json.dumps(report, indent=2) + "\n")
print(json.dumps(report, indent=2))

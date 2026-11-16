#!/usr/bin/env python3
"""Compile QNX ARM objects on Apple silicon; use GNU QNX tools for ELF linking."""

import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BUILD = ROOT / "build"
SDK = BUILD / "porting/gcc9/gcc-9.3.0-berrycore"


def native_flags(cxx):
    flags = [
        "--target=armv7-none-eabi", "-mthumb", "-mcpu=cortex-a9",
        "-mfpu=neon-vfpv3", "-mfloat-abi=softfp", "-fno-short-enums", "-fPIC", "-femulated-tls",
        "-D__ARM__", "-D__QNX__", "-D__QNXNTO__", "-D__unix__", "-D__unix",
        "-D__ELF__", "-D__LITTLEENDIAN__", "-D_QNX_SOURCE", "-D_REENTRANT",
        "-D_POSIX_C_SOURCE=200809L", "-D_LARGEFILE64_SOURCE", "-D_NO_CPP_INLINES",
        "-D_NTO_HDR_DIR_=" + str(SDK / "include") + "/",
        "-Wno-unused-command-line-argument",
    ]
    if cxx:
        flags += ["-nostdinc++"]
        for directory in (
            "include/libstdc++/9.3.0", "include/libstdc++/9.3.0/arm-blackberry-qnx8eabi",
            "include/libstdc++/9.3.0/backward",
        ):
            flags += ["-isystem", str(SDK / directory)]
    flags += ["-isystem", str(BUILD / "porting/clang-include")]
    return flags


def compile_native(compiler, cxx, args):
    # Target dependency include paths supplied by CMake must precede the SDK's
    # bundled, obsolete OpenSSL/zlib headers. Keep only the C sysroot last.
    return subprocess.call([compiler] + native_flags(cxx) + args + ["-isystem", str(SDK / "include")])


def target_tool(tool, args):
    cwd = Path.cwd().resolve()
    if not cwd.is_relative_to(ROOT):
        raise RuntimeError("The cross-link working directory must be inside this project")
    command = [
        "docker", "run", "--rm", "--platform", "linux/amd64", "--network", "none",
        "--mount", f"type=bind,source={ROOT},target={ROOT},readonly",
        "--mount", f"type=bind,source={BUILD},target={BUILD}",
        "--mount", f"type=bind,source={SDK},target=/root/qnx800,readonly",
        "--workdir", str(cwd), "--entrypoint", "/bin/bash", "berrygram/porting:bookworm",
        "-c", 'source /root/qnx800/env.sh; exec "$@"', "bb10-tool",
        "/root/qnx800/bin/arm-blackberry-qnx8eabi-" + tool,
    ]
    return subprocess.call(command + args)


def compile_flags(args):
    # Combined compile/link invocations occur in CMake's compiler and feature
    # checks. Compile their sources with Clang, then pass only objects to GNU.
    result = []
    skip = False
    for arg in args:
        if skip:
            skip = False
            continue
        if arg == "-o" or arg in ("-L", "-l", "-Xlinker"):
            skip = True
        elif arg.startswith(("-Wl,", "-l", "-L")) or arg in (
            "-shared", "-rdynamic", "-static", "-pie", "-no-pie",
        ):
            continue
        elif Path(arg).suffix not in (".o", ".a", ".so"):
            result.append(arg)
    return result


def main():
    mode, args = sys.argv[1], sys.argv[2:]
    runtime = BUILD / "porting/work/libbb10-clang-memory.a"
    if mode == "prepare-runtime":
        obj = BUILD / "porting/work/aeabi-memory.o"
        result = target_tool("gcc", ["-O2", "-fPIC", "-fno-builtin", "-c",
                             str(ROOT / "porting/aeabi-memory.c"), "-o", str(obj)])
        return result or target_tool("ar", ["rcs", str(runtime), str(obj)])
    if mode in ("ar", "ranlib", "strip"):
        return target_tool(mode, args)
    cxx = mode == "cxx"
    compiler = "clang++" if cxx else "clang"
    if any(arg in args for arg in ("--version", "-dumpmachine", "-dumpversion")):
        return compile_native(compiler, cxx, args)
    if any(arg in args for arg in ("-c", "-S", "-E", "-fsyntax-only")):
        return compile_native(compiler, cxx, args)

    sources = [arg for arg in args if Path(arg).suffix in (".c", ".cc", ".cpp", ".cxx", ".C")]
    # Clang warning options are compile-only; the GNU linker driver need not
    # understand them. Keep -Wl options, which belong to the linker itself.
    link_options = [arg for arg in args if not arg.startswith("-W") or arg.startswith("-Wl,")]
    if not sources:
        if "-o" in args:
            link_options.append(str(runtime))
        return target_tool("g++" if cxx else "gcc", link_options)
    temporary_root = BUILD / "porting/work/native-link"
    temporary_root.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=temporary_root) as directory:
        link_args = link_options + [str(runtime)]
        common = [arg for arg in compile_flags(args) if arg not in sources]
        for index, source in enumerate(sources):
            obj = str(Path(directory) / (str(index) + ".o"))
            result = compile_native(compiler, cxx, common + ["-c", source, "-o", obj])
            if result:
                return result
            link_args[link_args.index(source)] = obj
        return target_tool("g++" if cxx else "gcc", link_args)


if __name__ == "__main__":
    raise SystemExit(main())

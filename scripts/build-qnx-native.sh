#!/bin/bash
set -eo pipefail
PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CMAKE_BIN="$PROJECT_ROOT/build/tools/cmake-native/cmake/data/bin/cmake"
TD_SOURCE="$PROJECT_ROOT/build/porting/source/td-4c6377aa7386c39217878a9a56d581922ad37ca4"
WORK="$PROJECT_ROOT/build/porting/work"
PREFIX="$WORK/native-prefix"
TOOLS="$PROJECT_ROOT/porting/native"
STAGE="${1:-probe}"
mkdir -p "$WORK" "$PREFIX"
python3 "$PROJECT_ROOT/porting/prepare-clang-headers.py"
RUNTIME="$WORK/libbb10-clang-memory.a"
if [ ! -f "$RUNTIME" ] || [ "$PROJECT_ROOT/porting/aeabi-memory.c" -nt "$RUNTIME" ]; then
    python3 "$PROJECT_ROOT/porting/native-compiler.py" prepare-runtime
fi
case "$STAGE" in
    probe)
        "$TOOLS/bb10-cxx" -std=c++17 -O2 "$PROJECT_ROOT/porting/probe.cpp" -o "$WORK/native-probe"
        file "$WORK/native-probe"
        ;;
    openssl-probe)
        "$TOOLS/bb10-cc" -O2 -I "$PREFIX/include" "$PROJECT_ROOT/porting/openssl-probe.c" \
            "$PREFIX/lib/libssl.a" "$PREFIX/lib/libcrypto.a" \
            -pthread -ldl -lsocket -o "$WORK/openssl-probe"
        file "$WORK/openssl-probe"
        ;;
    generate)
        "$CMAKE_BIN" -S "$TD_SOURCE" -B "$WORK/td-native-host" \
            -DCMAKE_BUILD_TYPE=Release -DTD_GENERATE_SOURCE_FILES=ON
        "$CMAKE_BIN" --build "$WORK/td-native-host" --target prepare_cross_compiling -j2
        ;;
    dependencies)
        mkdir -p "$WORK/native-zlib" "$WORK/native-openssl"
        cd "$WORK/native-zlib"
        CHOST=arm-blackberry-qnx8eabi CC="$TOOLS/bb10-cc" \
            AR="$TOOLS/bb10-ar" RANLIB="$TOOLS/bb10-ranlib" \
            CFLAGS='-O2 -fPIC' "$PROJECT_ROOT/build/porting/source/zlib-1.3.2/configure" \
                --static --prefix="$PREFIX"
        make -j2
        make install
        cd "$WORK/native-openssl"
        OPENSSL_LOCAL_CONFIG_DIR="$PROJECT_ROOT/porting" \
            CC="$TOOLS/bb10-cc" CXX="$TOOLS/bb10-cxx" \
            AR="$TOOLS/bb10-ar" RANLIB="$TOOLS/bb10-ranlib" \
            perl "$PROJECT_ROOT/build/porting/source/openssl-3.5.9/Configure" bb10-armv7 \
                --prefix="$PREFIX" --openssldir=/app/native/ssl --libdir=lib \
                no-shared no-tests no-apps no-module no-asm no-async
        make -j2 build_libs
        make install_dev
        ;;
    platform)
        "$CMAKE_BIN" -S "$TD_SOURCE" -B "$WORK/td-native-platform" \
            -DCMAKE_TOOLCHAIN_FILE="$PROJECT_ROOT/porting/native-armv7.cmake" \
            -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF \
            -DCMAKE_DISABLE_FIND_PACKAGE_OpenSSL=ON -DCMAKE_DISABLE_FIND_PACKAGE_ZLIB=ON
        "$CMAKE_BIN" --build "$WORK/td-native-platform" --target tdutils -j2
        ;;
    tdutils|tdjson)
        if [ "$STAGE" = tdjson ] && [ ! -f "$TD_SOURCE/.berrygram-split-v1" ]; then
            docker run --rm --platform linux/amd64 --network none \
                --mount "type=bind,source=$PROJECT_ROOT,target=$PROJECT_ROOT,readonly" \
                --mount "type=bind,source=$PROJECT_ROOT/build,target=$PROJECT_ROOT/build" \
                --workdir "$TD_SOURCE" --entrypoint php berrygram/porting:bookworm \
                -r 'srand(0); require $argv[1];' "$TD_SOURCE/SplitSource.php"
            printf '%s\n' 'TDLib SplitSource.php, seed 0, upstream 4c6377aa' > "$TD_SOURCE/.berrygram-split-v1"
        fi
        "$CMAKE_BIN" -S "$TD_SOURCE" -B "$WORK/td-native-arm" \
            -DCMAKE_TOOLCHAIN_FILE="$PROJECT_ROOT/porting/native-armv7.cmake" \
            -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$PREFIX" \
            -DCMAKE_SHARED_LINKER_FLAGS='-Wl,--no-undefined,--no-allow-shlib-undefined' \
            -DOPENSSL_ROOT_DIR="$PREFIX" -DOPENSSL_USE_STATIC_LIBS=ON \
            -DZLIB_INCLUDE_DIR="$PREFIX/include" -DZLIB_LIBRARY="$PREFIX/lib/libz.a" \
            -DBUILD_TESTING=OFF
        "$CMAKE_BIN" --build "$WORK/td-native-arm" --target "$STAGE" -j2
        ;;
    *) echo "Usage: $0 [probe|openssl-probe|generate|dependencies|platform|tdutils|tdjson]" >&2; exit 2 ;;
esac

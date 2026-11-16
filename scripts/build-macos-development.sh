#!/bin/bash
set -eo pipefail
PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WORK="$PROJECT_ROOT/build/macos-development"
PREFIX="$WORK/prefix"
CMAKE="$PROJECT_ROOT/build/tools/cmake-native/cmake/data/bin/cmake"
STAGE="${1:-dependencies}"
mkdir -p "$WORK/logs" "$PREFIX"
case "$STAGE" in
    qt)
        mkdir -p "$WORK/qt"
        cd "$WORK/qt"
        if [ ! -f Makefile ]; then
            "$WORK/source/qtbase-everywhere-src-5.15.19/configure" \
                -prefix "$PREFIX/qt" -opensource -confirm-license -release \
                -platform macx-clang -device-option QMAKE_APPLE_DEVICE_ARCHS=arm64 \
                -no-framework -no-gui -no-widgets -no-opengl -no-dbus -no-icu \
                -nomake examples -nomake tests -nomake tools
        fi
        make -j2
        make install
        ;;
    dependencies)
        mkdir -p "$WORK/zlib" "$WORK/openssl"
        cd "$WORK/zlib"
        "$WORK/source/zlib-1.3.2/configure" --static --prefix="$PREFIX"
        make -j2
        make install
        cd "$WORK/openssl"
        perl "$WORK/source/openssl-3.5.9/Configure" darwin64-arm64-cc \
            --prefix="$PREFIX" --openssldir="$PREFIX/ssl" --libdir=lib \
            no-shared no-tests no-apps no-module
        make -j2 build_libs
        make install_dev
        ;;
    tdlib)
        TD_SOURCE="$WORK/source/td-4c6377aa7386c39217878a9a56d581922ad37ca4"
        if [ ! -f "$TD_SOURCE/.berrygram-macos-split-v1" ]; then
            docker run --rm --platform linux/amd64 --network none \
                --mount "type=bind,source=$WORK,target=/work" \
                --workdir /work/source/td-4c6377aa7386c39217878a9a56d581922ad37ca4 \
                --entrypoint php berrygram/porting:bookworm \
                -r 'srand(0); require "SplitSource.php";'
            printf '%s\n' 'Official SplitSource.php, seed 0, native macOS build' > "$TD_SOURCE/.berrygram-macos-split-v1"
        fi
        "$CMAKE" -S "$TD_SOURCE" -B "$WORK/tdlib" \
            -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 \
            -DOPENSSL_ROOT_DIR="$PREFIX" -DOPENSSL_USE_STATIC_LIBS=ON \
            -DZLIB_INCLUDE_DIR="$PREFIX/include" -DZLIB_LIBRARY="$PREFIX/lib/libz.a" \
            -DBUILD_TESTING=OFF
        "$CMAKE" --build "$WORK/tdlib" --target tdjson -j2
        ;;
    *) echo "Usage: $0 [qt|dependencies|tdlib]" >&2; exit 2 ;;
esac

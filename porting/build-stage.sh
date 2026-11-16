#!/bin/bash
set -eo pipefail
STAGE="$1"
source /root/qnx800/env.sh
TD_SOURCE=/sources/td-4c6377aa7386c39217878a9a56d581922ad37ca4
PREFIX=/work/prefix
mkdir -p "$PREFIX"
case "$STAGE" in
    probe)
        arm-blackberry-qnx8eabi-g++ --version
        arm-blackberry-qnx8eabi-g++ -I /project/build/porting/clang-include \
            -std=c++17 -O2 /project/porting/probe.cpp -o /work/cxx17-probe
        file /work/cxx17-probe
        readelf -l /work/cxx17-probe | sed -n '/interpreter/p'
        readelf -d /work/cxx17-probe | sed -n '/NEEDED/p'
        ;;
    generate)
        cmake -S "$TD_SOURCE" -B /work/td-host \
            -DCMAKE_BUILD_TYPE=Release -DTD_GENERATE_SOURCE_FILES=ON
        cmake --build /work/td-host --target prepare_cross_compiling -j2
        ;;
    dependencies)
        mkdir -p /work/zlib-arm /work/openssl-arm
        cd /work/zlib-arm
        CHOST=arm-blackberry-qnx8eabi CC=arm-blackberry-qnx8eabi-gcc \
            AR=arm-blackberry-qnx8eabi-ar RANLIB=arm-blackberry-qnx8eabi-ranlib \
            CFLAGS='-O2 -fPIC' /sources/zlib-1.3.2/configure --static --prefix="$PREFIX"
        make -j2
        make install
        cd /work/openssl-arm
        OPENSSL_LOCAL_CONFIG_DIR=/project/porting \
            perl /sources/openssl-3.5.9/Configure bb10-armv7 \
                --prefix="$PREFIX" --openssldir=/app/native/ssl \
                --libdir=lib no-shared no-tests no-apps no-module no-asm no-async
        make -j2 build_libs
        make install_dev
        ;;
    platform)
        cmake -S "$TD_SOURCE" -B /work/td-arm-platform \
            -DCMAKE_TOOLCHAIN_FILE=/project/porting/qnx-armv7.cmake \
            -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF \
            -DCMAKE_DISABLE_FIND_PACKAGE_OpenSSL=ON -DCMAKE_DISABLE_FIND_PACKAGE_ZLIB=ON
        cmake --build /work/td-arm-platform --target tdutils -j1
        ;;
    tdutils|tdjson)
        cmake -S "$TD_SOURCE" -B /work/td-arm \
            -DCMAKE_TOOLCHAIN_FILE=/project/porting/qnx-armv7.cmake \
            -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$PREFIX" \
            -DOPENSSL_ROOT_DIR="$PREFIX" -DOPENSSL_USE_STATIC_LIBS=ON \
            -DZLIB_INCLUDE_DIR="$PREFIX/include" -DZLIB_LIBRARY="$PREFIX/lib/libz.a" \
            -DBUILD_TESTING=OFF
        cmake --build /work/td-arm --target "$STAGE" -j2
        ;;
esac

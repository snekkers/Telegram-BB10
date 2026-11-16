#!/bin/bash
set -eu
TARGET="${1:?target}"
PREFIX="/output/$TARGET/voice-codecs"
if [ -f "$PREFIX/fixed-codecs-v2" ] && [ -s "$PREFIX/lib/libopusfile.a" ] && [ -s "$PREFIX/lib/libopus.a" ] && [ -s "$PREFIX/lib/libogg.a" ]; then exit 0; fi
case "$TARGET" in
 device) VARIANT=gcc_ntoarmv7le; HOST=arm-unknown-nto-qnx6.6.0; BIN=ntoarmv7 ;;
 simulator) VARIANT=gcc_ntox86; HOST=i686-unknown-nto-qnx6.6.0; BIN=ntox86 ;;
 *) exit 2 ;;
esac
export CC="qcc -V$VARIANT" CXX="QCC -V$VARIANT" AR="$BIN-ar" RANLIB="$BIN-ranlib" CFLAGS="-O2 -fPIC"
mkdir -p "$PREFIX" "/output/$TARGET/voice-build"
for PACKAGE in libogg-1.3.5 opus-1.6.1 opusfile-0.12; do
 mkdir -p "/output/$TARGET/voice-build/$PACKAGE"
 cd "/output/$TARGET/voice-build/$PACKAGE"
 case "$PACKAGE" in
  libogg*) OPTIONS="" ;;
  opus-*) OPTIONS="--disable-extra-programs --disable-doc --disable-intrinsics --disable-asm --enable-fixed-point --disable-float-api --disable-deep-plc --disable-dred --disable-osce" ;;
  opusfile*) OPTIONS="--disable-http --disable-examples --disable-doc --enable-fixed-point --disable-float" ;;
 esac
 export OGG_CFLAGS="-I$PREFIX/include" OGG_LIBS="-L$PREFIX/lib -logg"
 export OPUS_CFLAGS="-I$PREFIX/include/opus" OPUS_LIBS="-L$PREFIX/lib -lopus -lm"
 export DEPS_CFLAGS="-I$PREFIX/include/opus -I$PREFIX/include" DEPS_LIBS="-L$PREFIX/lib -lopus -logg -lm"
 /source/build/voice-codecs/source/$PACKAGE/configure --host="$HOST" --build=x86_64-unknown-linux-gnu --prefix="$PREFIX" --disable-shared --enable-static $OPTIONS
 make -j2
 mkdir -p "$PREFIX/lib" "$PREFIX/include/ogg" "$PREFIX/include/opus"
 case "$PACKAGE" in
  libogg*) cp src/.libs/libogg.a "$PREFIX/lib/"; cp /source/build/voice-codecs/source/$PACKAGE/include/ogg/*.h "$PREFIX/include/ogg/"; cp include/ogg/config_types.h "$PREFIX/include/ogg/" ;;
  opus-*) cp .libs/libopus.a "$PREFIX/lib/"; cp /source/build/voice-codecs/source/$PACKAGE/include/*.h "$PREFIX/include/opus/" ;;
  opusfile*) cp .libs/libopusfile.a "$PREFIX/lib/"; cp /source/build/voice-codecs/source/$PACKAGE/include/opusfile.h "$PREFIX/include/opus/" ;;
 esac
done
touch "$PREFIX/fixed-codecs-v2"

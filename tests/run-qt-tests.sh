#!/bin/bash
set -eu
cd /work
cc -std=c99 -Wall -Wextra -Werror /source/engine/tdjson-host.c -ldl -o berrygram-tdjson
qmake /source/tests/session-tests.pro
make -j2
./session-tests -o results.xml,junitxml -o results.qt.xml,xml -o -,txt
mkdir -p store
cd store
qmake /source/tests/store-tests.pro
make -j2
./store-tests -o results.xml,junitxml -o results.qt.xml,xml -o -,txt
mkdir -p /work/avatars
cd /work/avatars
qmake /source/tests/avatar-tests.pro
make -j2
./avatar-tests -o results.xml,junitxml -o results.qt.xml,xml -o -,txt
mkdir -p /work/voice
cd /work/voice
ffmpeg -nostdin -hide_banner -loglevel error -y -f lavfi -i "sine=frequency=440:sample_rate=48000" -t 60 -c:a libopus -b:a 24k sample.ogg
ffmpeg -nostdin -hide_banner -loglevel error -y -f lavfi -i "anullsrc=r=48000:cl=mono" -t 1801 -c:a libopus -b:a 8k overlong.ogg
qmake /source/tests/voice-tests.pro
make -j2
./voice-tests -o results.xml,junitxml -o results.qt.xml,xml -o -,txt

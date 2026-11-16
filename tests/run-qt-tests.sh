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

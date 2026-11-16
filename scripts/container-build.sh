#!/bin/bash
set -eo pipefail

TARGET_PLATFORM="${1:-simulator}"
case "$TARGET_PLATFORM" in
    simulator)
        QMAKE_SPEC=blackberry-x86-qcc
        BUILD_CONFIG=Simulator-Debug
        SDK_OUTPUT_SUBDIRECTORY=o-g
        ;;
    device)
        QMAKE_SPEC=blackberry-armv7le-qcc
        BUILD_CONFIG=Device-Debug
        SDK_OUTPUT_SUBDIRECTORY=o.le-v7-g
        ;;
    *) echo "Unknown target: $TARGET_PLATFORM" >&2; exit 2 ;;
esac

SDK_ENV="${BBNDK_ENV:-}"
if [ -z "$SDK_ENV" ]; then
    for SDK_DIRECTORY in /opt/bbndk /root/bbndk /home/*/bbndk /home/*/bin/bbndk; do
        for CANDIDATE in "$SDK_DIRECTORY"/bbndk-env_10_3_1_995.sh "$SDK_DIRECTORY"/bbndk-env*.sh; do
            if [ -f "$CANDIDATE" ]; then
                SDK_ENV="$CANDIDATE"
                break 2
            fi
        done
    done
fi
if [ ! -f "$SDK_ENV" ]; then
    echo "Cannot find bbndk-env*.sh in the SDK image." >&2
    exit 1
fi

echo "Loading BB10 SDK: $SDK_ENV"
source "$SDK_ENV"

BUILD_DIRECTORY="/output/$TARGET_PLATFORM"
mkdir -p "$BUILD_DIRECTORY"
cd "$BUILD_DIRECTORY"

qmake -spec "$QMAKE_SPEC" -o Makefile /source/berrygram.pro \
    "CONFIG+=$TARGET_PLATFORM debug" "CONFIG-=release"
make -j2
test -s "$BUILD_DIRECTORY/$SDK_OUTPUT_SUBDIRECTORY/berrygram"
mkdir -p "$BUILD_DIRECTORY/bin"
cp "$BUILD_DIRECTORY/$SDK_OUTPUT_SUBDIRECTORY/berrygram" "$BUILD_DIRECTORY/bin/berrygram"

# The legacy packager writes assets/.assets.index. Give it a temporary copy
# rather than write access to the source checkout.
PACKAGE_STAGE="$(mktemp -d /tmp/berrygram-package.XXXXXX)"
cp /source/bar-descriptor.xml "$PACKAGE_STAGE/bar-descriptor.xml"
cp -R /source/assets "$PACKAGE_STAGE/assets"
mkdir -p "$PACKAGE_STAGE/build/$TARGET_PLATFORM/bin"
cp "$BUILD_DIRECTORY/bin/berrygram" "$PACKAGE_STAGE/build/$TARGET_PLATFORM/bin/berrygram"
if [ "$TARGET_PLATFORM" = device ]; then
    cp -R /output/device/telegram-runtime "$PACKAGE_STAGE/build/device/telegram-runtime"
fi
cd "$PACKAGE_STAGE"
blackberry-nativepackager -package "$BUILD_DIRECTORY/berrygram-$TARGET_PLATFORM.bar" \
    -devMode -configuration "$BUILD_CONFIG" bar-descriptor.xml

echo "Built: build/$TARGET_PLATFORM/berrygram-$TARGET_PLATFORM.bar"

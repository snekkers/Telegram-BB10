#!/bin/sh
set -eu
case "$(uname -s):$(uname -m)" in
    QNX:arm*|QNX:ARM*) ;;
    *) echo "This diagnostic bundle requires a BB10 ARM phone." >&2; exit 64 ;;
esac
PROBE_DIRECTORY="$(cd "$(dirname "$0")" && pwd)"
LD_LIBRARY_PATH="$PROBE_DIRECTORY/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export LD_LIBRARY_PATH
"$PROBE_DIRECTORY/bin/cxx17-probe"
"$PROBE_DIRECTORY/bin/openssl-probe"
"$PROBE_DIRECTORY/bin/berrygram-tdjson" --library "$PROBE_DIRECTORY/lib/libtdjson.so" <<'TDLIB_REQUEST'
{"@type":"getOption","name":"version","@extra":"device-probe"}
TDLIB_REQUEST

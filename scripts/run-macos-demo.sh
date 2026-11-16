#!/bin/bash
set -eu
PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEMO_ROOT="$PROJECT_ROOT/build/macos-development/demo"
umask 077
mkdir -p "$DEMO_ROOT"
printf '%s\n' 'synthetic' > "$DEMO_ROOT/libtdjson.so"
exec python3 "$PROJECT_ROOT/diagnostic/server.py" --synthetic \
    --engine "$PROJECT_ROOT/tests/fake-chat-engine.py" \
    --library "$DEMO_ROOT/libtdjson.so" --data "$DEMO_ROOT/private" "$@"

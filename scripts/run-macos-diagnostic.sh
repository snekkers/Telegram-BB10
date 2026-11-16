#!/bin/bash
set -eu
PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
exec python3 "$PROJECT_ROOT/diagnostic/server.py" "$@"

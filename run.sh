#!/usr/bin/env bash
# Builds the project if needed and starts the sample launcher.
# Usage: ./run.sh [debug|release]

set -euo pipefail

preset="${1:-debug}"
case "$preset" in
    debug)   build_dir="build" ;;
    release) build_dir="build-release" ;;
    *)
        echo "Unknown preset '$preset', expected debug or release" >&2
        exit 1
        ;;
esac

cd "$(dirname "$0")"

if [ ! -f "$build_dir/CMakeCache.txt" ]; then
    cmake --preset "$preset"
fi
cmake --build --preset "$preset"

exec "$build_dir/bin/launcher"

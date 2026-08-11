#!/usr/bin/env bash
# Measures the build speed (Linux/macOS): clean configure + full build.
# Usage: ./tools/bench_build.sh [17|20|23] [Ninja|Unix Makefiles]
set -euo pipefail

STANDARD="${1:-17}"
GENERATOR="${2:-Unix Makefiles}"

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT/bench-build"

echo "Cleaning $BUILD_DIR ..."
rm -rf "$BUILD_DIR"

echo "Configure (C++$STANDARD, Release) ..."
START=$(date +%s.%N)
cmake -S "$ROOT" -B "$BUILD_DIR" -G "$GENERATOR" \
    -DCMAKE_BUILD_TYPE=Release -DLOGGER_CXX_STANDARD="$STANDARD"

echo "Build ..."
cmake --build "$BUILD_DIR" -j
END=$(date +%s.%N)

TIME=$(echo "$END - $START" | bc)
SIZE=$(du -sh "$BUILD_DIR" | cut -f1)

echo ""
echo "Build time : ${TIME}s"
echo "C++ std    : $STANDARD"
echo "Generator  : $GENERATOR"
echo "Output     : $SIZE"
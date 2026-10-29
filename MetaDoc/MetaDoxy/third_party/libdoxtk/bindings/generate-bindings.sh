#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BUILD_DIR="${BUILD_DIR:-"$ROOT/build/bindings"}"
SWIG="${SWIG:-swig}"
mkdir -p "$BUILD_DIR"
if ! command -v "$SWIG" >/dev/null 2>&1; then
  echo "error: SWIG executable not found (set SWIG=/path/to/swig)" >&2
  exit 127
fi
echo "Binding interface is a scaffold; generation is intentionally not invoked until typemaps and target language are specified." >&2
echo "Interface: $ROOT/bindings/libdoxir.i"
echo "Build directory: $BUILD_DIR"

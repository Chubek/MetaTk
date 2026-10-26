#!/usr/bin/env sh
set -eu
cmake -S "$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)" -B "${GPPX_BUILD_DIR:-build}" -DCMAKE_BUILD_TYPE="${GPPX_BUILD_TYPE:-Release}"
cmake --build "${GPPX_BUILD_DIR:-build}" --parallel

#!/usr/bin/env sh
set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
build="${GPPX_BUILD_DIR:-$root/build}"
cmake -S "$root" -B "$build" -DCMAKE_BUILD_TYPE=Debug -DGPPX_BUILD_TESTS=ON -DGPPX_BUILD_WEBBER=ON
cmake --build "$build" --parallel
ctest --test-dir "$build" --output-on-failure
"$build/gppx" --plugin "$build/plugins/gppx_webber.so" "$root/examples/webber/demo.gppx" >/dev/null

#!/usr/bin/env sh
set -eu
build_dir="${1:-_build}"
cmake -S .. -B "$build_dir"
cmake --build "$build_dir" --target docs-html docs-pdf

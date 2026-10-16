#!/usr/bin/env bash
# Historical filename retained for compatibility. Generates source, not binaries.
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
language=python
output="$root/bindings/generated"
while (($#)); do
    case "$1" in
        --language) [[ $# -ge 2 ]] || { echo 'missing language' >&2; exit 2; }; language=$2; shift 2 ;;
        --output-root) [[ $# -ge 2 ]] || { echo 'missing output path' >&2; exit 2; }; output=$2; shift 2 ;;
        --help|-h) echo 'Usage: generate-bidnings.sh [--language python|ruby|java|csharp|lua|tcl] [--output-root DIR]'; exit 0 ;;
        *) echo "unknown argument: $1" >&2; exit 2 ;;
    esac
done
case "$language" in python|ruby|java|csharp|lua|tcl) ;; *) echo "unsupported language: $language" >&2; exit 2 ;; esac
command -v swig >/dev/null || { echo 'SWIG is required' >&2; exit 1; }
mkdir -p -- "$output/$language"
swig "-$language" -I"$root/include" -outdir "$output/$language" \
    -o "$output/$language/Chomsky3_wrap.c" "$root/bindings/Chomsky3.i"
echo "Generated $language binding sources in $output/$language"

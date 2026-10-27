#!/bin/sh
# Bootstrap TeXScrape from the repository's unmodified PlainTeX.tex, then run
# the TeX82 engine with the freshly produced format. No texmf tree required.
set -eu
plain_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
engine=${TEXSCRAPE_TEX_ENGINE:-${TeXScrape_TeX_ENGINE:-}}
if [ -z "$engine" ]; then
    if command -v texscrape >/dev/null 2>&1; then
        engine=$(command -v texscrape)
    else
        echo 'TeXScrape: set TEXSCRAPE_TEX_ENGINE to the compiled texscrape executable' >&2
        exit 2
    fi
fi
case "$engine" in /*) ;; *) engine=$(CDPATH= cd -- "$(dirname -- "$engine")" && pwd)/$(basename -- "$engine");; esac
[ -x "$engine" ] || { echo "TeXScrape: engine not executable: $engine" >&2; exit 2; }

# Generated .fmt/.log files never belong in the source tree.
format_dir=${TEXSCRAPE_FORMAT_DIR:-${XDG_CACHE_HOME:-${HOME:-/tmp}/.cache}/texscrape}
mkdir -p -- "$format_dir"
format_file="$format_dir/PlainTeX.fmt"

kpse=${KPSEWHICH:-kpsewhich}
find_resource() { if command -v "$kpse" >/dev/null 2>&1; then "$kpse" "$1" 2>/dev/null || :; fi; }
hyphen=$(find_resource hyphen.tex)
cm=$(find_resource cmr10.tfm)
man=$(find_resource manfnt.tfm)
font_paths=${TEXFONTS:-}
for file in "$cm" "$man"; do
    [ -n "$file" ] || continue
    dir=$(dirname -- "$file")
    font_paths=${font_paths:+$font_paths:}$dir
done
input_paths=$plain_dir
if [ -n "$hyphen" ]; then input_paths=$input_paths:$(dirname -- "$hyphen"); fi
input_paths=${input_paths}${TEXINPUTS:+:$TEXINPUTS}

# A temporary boot directory prevents concurrent writers from exposing
# partially written .fmt files to readers.
if [ ! -s "$format_file" ] || [ "$plain_dir/PlainTeX.tex" -nt "$format_file" ]; then
    tmp=$(mktemp -d "$format_dir/.bootstrap.XXXXXXXX")
    trap 'rm -rf -- "$tmp"' EXIT HUP INT TERM
    (
        cd "$tmp"
        TEXINPUTS="$input_paths" TEXFONTS="$font_paths" \
            "$engine" '\input PlainTeX \dump' </dev/null
    )
    [ -s "$tmp/PlainTeX.fmt" ] || { echo 'TeXScrape: PlainTeX format generation failed' >&2; exit 1; }
    mv -f -- "$tmp/PlainTeX.fmt" "$format_file"
    rm -rf -- "$tmp"
    trap - EXIT HUP INT TERM
fi
if [ "${1:-}" = --init-only ]; then
    printf 'PlainTeX format: %s\n' "$format_file"
    exit 0
fi
TEXINPUTS="$input_paths" TEXFONTS="$font_paths" \
TEXFORMATS="$format_dir${TEXFORMATS:+:$TEXFORMATS}" \
exec "$engine" '&PlainTeX' "$@"

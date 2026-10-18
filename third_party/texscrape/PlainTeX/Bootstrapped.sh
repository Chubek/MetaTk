#!/usr/bin/env sh
#
# Launch the repository's TeX engine with the PlainTeX format preloaded.
# The format is generated lazily in this directory, so a fresh checkout works
# without committing generated binary files.

set -eu

plain_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_dir=$(CDPATH= cd -- "$plain_dir/.." && pwd)
TeXScrape_TeX=${TeXScrape_TeX_ENGINE:-"$repo_dir/tex"}
export TeXScrape_TeX
format_file="$plain_dir/PlainTeX.fmt"

kpsewhich_path=${KPSEWHICH:-kpsewhich}
if command -v "$kpsewhich_path" >/dev/null 2>&1; then
    hyphen_file=$("$kpsewhich_path" hyphen.tex 2>/dev/null || :)
    cm_font=$("$kpsewhich_path" cmr10.tfm 2>/dev/null || :)
    manfnt_font=$("$kpsewhich_path" manfnt.tfm 2>/dev/null || :)
else
    hyphen_file=
    cm_font=
    manfnt_font=
fi

input_paths=$plain_dir
[ -n "$hyphen_file" ] && input_paths=$input_paths:$(dirname "$hyphen_file")
font_paths=
[ -n "$cm_font" ] && font_paths=$(dirname "$cm_font")
[ -n "$manfnt_font" ] && font_paths=${font_paths:+$font_paths:}$(dirname "$manfnt_font")

if [ ! -r "$format_file" ] || [ "$plain_dir/PlainTeX.tex" -nt "$format_file" ]; then
    (
        cd "$plain_dir"
        TEXINPUTS=$input_paths${TEXINPUTS:+:$TEXINPUTS} \
        TEXFONTS=$font_paths${TEXFONTS:+:$TEXFONTS} \
        "$TeXScrape_TeX" '\input PlainTeX \dump' </dev/null
    )
fi

TEXINPUTS=$input_paths${TEXINPUTS:+:$TEXINPUTS} \
TEXFONTS=$font_paths${TEXFONTS:+:$TEXFONTS} \
TEXFORMATS=$plain_dir${TEXFORMATS:+:$TEXFORMATS} \
exec "$TeXScrape_TeX" '&PlainTeX' "$@"

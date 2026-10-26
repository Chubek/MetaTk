#!/usr/bin/env bash
# Run from any directory. All generated files stay in the chosen output tree.
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
OUT="$ROOT/build"
LUA_BUILD="$ROOT/build/luaroff"
MODE=auto
FORMAT=pdf
PREFIX="${HOME:-/usr/local}/.local"
JOBS=2
INSTALL=0
CHECK=0
PPLUA_BIN="${PPLUA:-}"
METAML_BIN="${METAML:-$ROOT/../build/metaml-repl-cpp}"
CMAKE_ARGS=()
usage() {
    cat <<'HELP'
Usage: metaml-book/build.sh [options]
  --format pdf|txt|roff|all  Output format (default: pdf; all includes all three)
  --output-dir DIR          Output directory (default: metaml-book/build)
  --build-luaroff           Configure/build the bundled luaROFF with CMake
  --no-build-luaroff        Require an existing local or installed pplua
  --pplua FILE              Use this executable (also: PPLUA environment variable)
  --luaroff-build-dir DIR   CMake build directory for luaROFF
  --install-luaroff         Build and install luaROFF, then build the book
  --prefix DIR             Installation prefix (default: $HOME/.local)
  --jobs N                 CMake parallelism (default: 2)
  --cmake-arg ARG          Extra CMake configure argument; repeat as needed
  --check                  Check chapters, rofflets, and executable MetaML examples
  --metaml FILE             MetaML executable used by --check (also: METAML)
  -h, --help               Show this help

Auto mode uses --pplua/PPLUA, a local build, then PATH; if absent it builds
the bundled source. --build-luaroff forces a local rebuild. Installation is
opt-in, uses cmake --install, honors DESTDIR, and never invokes sudo. The book
can always use the local executable, including after a staged installation.

Dependencies: Bash, Python 3, groff (me, pic, eqn, gropdf for PDF).
Building luaROFF additionally requires CMake, C++17, Lua 5.4 headers/library,
and the bundled sol2 headers. Graphviz and TeX are not required.
HELP
}
die() { printf 'book: %s\n' "$*" >&2; exit 1; }
need_value() { [[ $# -ge 2 && -n $2 ]] || die "$1 requires a value"; }
while (($#)); do
    case "$1" in
        --format) need_value "$@"; FORMAT=$2; shift 2 ;;
        --output-dir) need_value "$@"; OUT=$2; shift 2 ;;
        --luaroff-build-dir) need_value "$@"; LUA_BUILD=$2; shift 2 ;;
        --prefix) need_value "$@"; PREFIX=$2; shift 2 ;;
        --jobs) need_value "$@"; JOBS=$2; shift 2 ;;
        --pplua) need_value "$@"; PPLUA_BIN=$2; shift 2 ;;
        --metaml) need_value "$@"; METAML_BIN=$2; shift 2 ;;
        --cmake-arg) need_value "$@"; CMAKE_ARGS+=("$2"); shift 2 ;;
        --build-luaroff) MODE=build; shift ;;
        --no-build-luaroff) MODE=existing; shift ;;
        --install-luaroff) INSTALL=1; shift ;;
        --check) CHECK=1; shift ;;
        -h|--help) usage; exit 0 ;;
        *) die "unknown option: $1 (see --help)" ;;
    esac
done
[[ $FORMAT =~ ^(pdf|txt|roff|all)$ ]] || die "invalid format: $FORMAT"
[[ $JOBS =~ ^[1-9][0-9]*$ ]] || die '--jobs must be a positive integer'
[[ $INSTALL == 0 || $MODE != existing ]] || die '--install-luaroff conflicts with --no-build-luaroff'
command -v python3 >/dev/null || die 'Python 3 is required'
absolute() { python3 -c 'import os,sys; print(os.path.abspath(sys.argv[1]))' "$1"; }
OUT=$(absolute "$OUT")
LUA_BUILD=$(absolute "$LUA_BUILD")
PREFIX=$(absolute "$PREFIX")
if [[ -n $PPLUA_BIN ]]; then
    PPLUA_BIN=$(command -v -- "$PPLUA_BIN") || die 'specified pplua was not found'
    PPLUA_BIN=$(absolute "$PPLUA_BIN")
    [[ -x $PPLUA_BIN ]] || die 'specified pplua is not executable'
fi
if [[ $INSTALL == 1 ]]; then MODE=build; fi
if [[ $MODE != build && -z $PPLUA_BIN ]]; then
    if [[ -x $LUA_BUILD/pplua ]]; then PPLUA_BIN="$LUA_BUILD/pplua"
    elif command -v pplua >/dev/null; then PPLUA_BIN=$(command -v pplua)
    elif [[ $MODE == existing ]]; then die 'no pplua found; use --build-luaroff or --pplua'
    else MODE=build
    fi
fi
if [[ $MODE == build ]]; then
    command -v cmake >/dev/null || die 'CMake is required to build luaROFF'
    cmake -S "$ROOT/luaROFF" -B "$LUA_BUILD" -DCMAKE_BUILD_TYPE=Release \
        "-DCMAKE_INSTALL_PREFIX=$PREFIX" "${CMAKE_ARGS[@]}"
    cmake --build "$LUA_BUILD" --parallel "$JOBS"
    PPLUA_BIN="$LUA_BUILD/pplua"
    if [[ $INSTALL == 1 ]]; then cmake --install "$LUA_BUILD" --prefix "$PREFIX"; fi
fi
[[ -x $PPLUA_BIN ]] || die 'pplua is not executable'
if [[ $FORMAT != roff ]]; then
    command -v groff >/dev/null || die 'groff is required for PDF/text output'
fi
mkdir -p -- "$OUT"
# Unique temporary directory plus atomic publication: failed builds never replace a good book.
TMP=$(mktemp -d "$OUT/.book.XXXXXXXX")
trap 'rm -rf -- "$TMP"' EXIT
python3 "$ROOT/scripts/check_book.py" --structure-only
if [[ $CHECK == 1 ]]; then
    python3 "$ROOT/scripts/check_book.py" --metaml "$METAML_BIN" --pplua "$PPLUA_BIN"
fi
INPUTS=("$ROOT/macros/book.me" "$ROOT/index.lroff")
for ((i=1; i<=42; ++i)); do INPUTS+=("$ROOT/chapters/ch-$i.me"); done
INPUTS+=("$ROOT/macros/end.lroff")
preprocess() {
    local device=$1 target=$2
    "$PPLUA_BIN" -n -D "BOOK_ROOT=$ROOT" -D "BOOK_FORMAT=$device" \
        -e 'dofile(BOOK_ROOT .. "/macros/book.lua")' "${INPUTS[@]}" \
        >"$target" 2>"$TMP/pplua-$device.log"
    # luaROFF 0.1 may report Lua failures without a nonzero exit status.
    if [[ -s $TMP/pplua-$device.log ]]; then
        cat "$TMP/pplua-$device.log" >&2
        die "luaROFF reported diagnostics ($device)"
    fi
    [[ -s $target ]] || die 'luaROFF produced empty output'
}
render() {
    local device=$1 extension=$2 source=$3
    local options=()
    if [[ $device == utf8 ]]; then options=(-P-c -P-b -P-u); fi
    groff -Kutf8 -k -p -t -e -me -T"$device" "${options[@]}" "$source" \
        >"$TMP/metaml-book.$extension" 2>"$TMP/groff-$extension.log"
    if [[ -s $TMP/groff-$extension.log ]]; then
        cat "$TMP/groff-$extension.log" >&2
        die "groff reported diagnostics ($extension)"
    fi
    [[ -s $TMP/metaml-book.$extension ]] || die "empty $extension output"
}
if [[ $FORMAT != txt ]]; then
    preprocess pdf "$TMP/metaml-book.roff"
    if [[ $FORMAT == pdf || $FORMAT == all ]]; then render pdf pdf "$TMP/metaml-book.roff"; fi
fi
if [[ $FORMAT == txt || $FORMAT == all ]]; then
    preprocess txt "$TMP/metaml-book-text.roff"
    render utf8 txt "$TMP/metaml-book-text.roff"
fi
python3 "$ROOT/scripts/check_book.py" --structure-only --manifest "$TMP/manifest.json"
for artifact in "$TMP"/metaml-book.* "$TMP/manifest.json"; do
    [[ -f $artifact ]] && mv -f -- "$artifact" "$OUT/"
done
printf 'Book built in %s (%s)\n' "$OUT" "$FORMAT"

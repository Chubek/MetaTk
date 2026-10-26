#!/usr/bin/env sh
# Verify that separately compiled C and C++ clients can use an installed tree.
# Usage: scripts/verify-install.sh [prefix]
set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
prefix=${1:-"$root/prebuilt/linux-x86_64"}
cc=${CC:-cc}
cxx=${CXX:-c++}
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
cat >"$tmp/client.c" <<'SOURCE'
#include <GPPX-all.h>
#include <string.h>
int main(void) {
    GPPX_State *s=gppx_create();
    char *out=0;
    int bad;
    if (!s) return 1;
    bad=gppx_macro_define_spec(s,"CHECK=works") ||
        gppx_macro_process_text(s,"CHECK",&out) ||
        !out || strcmp(out,"works") ||
        gppx_pmatch_glob("a*?","abcd")<=0;
    gppx_free(out);
    gppx_destroy(s);
    return bad?1:0;
}
SOURCE
cat >"$tmp/client.cpp" <<'SOURCE'
#include <GPPX.hpp>
int main() {
    gppx::Engine engine;
    engine.define("CHECK","works");
    return engine.process("CHECK\n") == "works\n" ? 0 : 1;
}
SOURCE
"$cc" -std=c11 -Wall -Wextra -Werror -I"$prefix/include" "$tmp/client.c" -L"$prefix/lib" -Wl,-rpath,"$prefix/lib" -lgppx -o "$tmp/check-c"
"$cxx" -std=c++17 -Wall -Wextra -Werror -I"$prefix/include" "$tmp/client.cpp" -L"$prefix/lib" -Wl,-rpath,"$prefix/lib" -lgppx -o "$tmp/check-cpp"
"$tmp/check-c"
"$tmp/check-cpp"
"$prefix/bin/gppx" --version
printf '%s\n' "Installed C and C++ clients passed"

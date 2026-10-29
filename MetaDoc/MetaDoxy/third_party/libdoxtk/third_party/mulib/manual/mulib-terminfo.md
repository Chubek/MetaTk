# MuLIB terminfo

`include/mulib-terminfo.h` provides a **native compiled-record reader**, a
numeric terminfo parameter interpreter, and a separately selected ANSI profile.
It does not use curses or another terminal library.

## Integration and ownership

Define `MULIB_IMPL_TERMINFO` and its dependency `MULIB_IMPL_STRBUF` in exactly one
translation unit, before any include. Other translation units include normally.
Declarations are C++17 compatible; implementation is ISO C11. No feature macros
or external libraries are required.

Initialize `mulib_terminfo_t` with `mulib_terminfo_init` (or zero initialization).
`mulib_terminfo_load`, `mulib_terminfo_load_file`, and `mulib_terminfo_ansi` replace
an initialized object; `mulib_terminfo_free` releases and zeroes it. Do not call
`init` on a live record. String/name accessors return borrowed pointers, valid
until successful replacement or free. Loaded input is copied and can be released
immediately, including when reloading from the object's current `data`.

## Reader scope

* Recognizes compiled magic `0x011a` (16-bit numbers) and `0x021e` (32-bit numbers)
  in little- or big-endian order. Names, booleans, alignment, numeric tables,
  16-bit string offsets and the core string table are bounds-checked.
* Core header counts/string-table size are limited to 32767; input records/files
  are limited to 1 MiB. Referenced strings and the names field must terminate
  within their declared extents. Missing/cancelled entries are supported.
* All core entries can be accessed by their standard table index. A practical
  subset also has short-name lookup, listed below.
* Trailing data, including ncurses extended tables, is ignored, **not parsed**.
  There is no extended-name lookup, terminfo source parser, `use=` resolver,
  terminal-size detection, automatic `$TERM`/database-directory search, or
  capability inference. Pass an explicit compiled-file path to `load_file`.
* `mulib_terminfo_ansi` is an explicit, allocation-free ANSI/xterm-like profile:
  delayed autowrap, common VT controls/keys, 256 indexed colors, basic attributes,
  alternate-screen control strings. It does not read a database or verify the
  connected terminal. It intentionally has no `cols` or `lines` value.

### Accessors

| API | Result |
| --- | --- |
| `mulib_terminfo_names(info)` | Name/alias field, or NULL for an empty object |
| `mulib_terminfo_bool_at(info, index)` / `bool(info, name)` | 0/1; -1 if absent, cancelled, unknown or out of range |
| `mulib_terminfo_num_at(info, index)` / `num(info, name)` | Nonnegative `int32_t`; -1 for missing/cancelled/negative/unknown |
| `mulib_terminfo_str_at(info, index)` / `str(info, name)` | Borrowed NUL-terminated string, or NULL |

Named booleans: `bw am xenl km mir msgr xon npc bce`.
Named numbers: `cols it lines colors pairs ncv`.

Named strings:

```text
cbt bel cr csr tbc clear el ed hpa cup cud1 home civis cub1 cnorm cuf1
cuu1 cvvis dch1 dl1 blink bold smcup dim smir invis rev smso smul ech
sgr0 rmcup rmir rmso rmul ich1 il1 kbs kdch1 kcud1 kf1..kf12 khome
kich1 kcub1 knp kpp kcuf1 kcuu1 rmkx smkx dch dl cud ich il cub cuf
cuu rc vpa sc ind ri hts ht kcbt smam rmam kend kent el1 op sitm ritm
setaf setab
```

`smxx` (strikeout) is additionally available **only in the ANSI profile**; it
is not treated as a standard core-table index. Unknown names never trigger a
guess. Indexed access provides capabilities outside this named subset.

## Parameter expansion

`mulib_terminfo_expand(format, params, output)` appends to an initialized
`mulib_strbuf_t`; `params` supplies nine numeric `int32_t` arguments, or NULL
means nine zeroes. `mulib_terminfo_emit(info, name, params, output)` performs
string lookup followed by the same expansion. **Arguments are zero-based for
`cup`** (row, column); a capability's `%i` performs any necessary increment.

Supported syntax:

* `%%`, `%p1` through `%p9`, `%{signed-decimal}`, `%'c'`, `%i`.
* Binary `+ - * / m & | ^ = > < A O`; unary `! ~`. Operators use a numeric
  stack; binary operators pop the right operand first. `A/O` are logical AND/OR.
* `%Pa`/`%ga` through `%Pz`/`%gz`: per-call, initially-zero local variables.
* `%d %o %x %X %c`; numeric field width 0..32 and zero fill, with optional `:`:
  e.g. `%02d`, `%:3d`. Decimal is signed; octal/hex interpret values as uint32.
  `%c` accepts byte values 1..255, without a nonzero width.
* `%? expression %t then %e else %;`, nested to 16 levels, including chained
  `%e expression %t` else-if clauses used by common `setaf` entries. Operations
  in inactive branches are parsed but not evaluated.
* `$<digits[.digits][*][/]>` padding annotations are removed. Flags may be in
  either order. **No delay or line-count multiplication is performed.** Raw
  string lookup retains these annotations; only expansion removes them.

Unsupported: string parameters/`%s`/`%l`, persistent uppercase variables, old
termcap operators, printf precision/flags other than zero fill, and nonnumeric
extensions. Unsupported syntax fails even in an inactive branch. Arithmetic
overflow, division by zero, stack underflow/overflow, malformed conditionals and
padding annotations fail. Stack capacity is 32; expansion is limited to 65536
outer interpreter steps and 64 KiB of generated bytes per call. Numeric constant
and width overflow is checked. Extra stack values at completion are permitted.

## Errors and output

Load/expansion/emit return **1 on success, 0 on failure**; no `errno` contract.
Allocation failure, invalid input and missing capability share the 0 result.
Failed loads leave the old record intact. Failed expansion/emission leaves the
caller's output unchanged: expansion is built in a temporary strbuf first.
Neither module writes terminal bytes itself. Caller-supplied capability strings
and compiled databases must be trusted for the terminal actions they encode.
Do not copy owned objects by value or concurrently mutate the same object.

## Runnable example

```c
#include <assert.h>
#include <string.h>
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_TERMINFO
#include "mulib-terminfo.h"
int main(void) {
    mulib_terminfo_t info;
    mulib_strbuf_t out;
    int32_t p[9] = {2, 4};
    mulib_terminfo_init(&info);
    mulib_terminfo_ansi(&info);
    mulib_strbuf_init(&out);
    assert(mulib_terminfo_emit(&info, "cup", p, &out));
    assert(strcmp(out.data, "\033[3;5H") == 0);
    mulib_strbuf_free(&out);
    mulib_terminfo_free(&info);
    return 0;
}
```

Compile/run `examples/modules/terminfo.c`:

```sh
cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude examples/modules/terminfo.c -o ./terminfo-example
./terminfo-example
```

`tests/test_terminal.c` builds synthetic compiled records for both widths/byte
orders, checks truncation/offset errors, transactional replacement, numeric
expansion, malformed programs and the output/stack limits. No interactive
terminal or installed terminfo database is required.

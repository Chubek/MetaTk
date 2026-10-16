# 4. Libraries and modules

## 4.1 Using a library

```qscript
use math, bits;          # one or more names, comma-separated
use date;                # `use` may appear anywhere at top level

say gcd(12, 18);         # unqualified
say bits::popcount(255); # qualified with the library name
say math::INT_MAX;       # constants too
return date::days_in_month(2024, 2);   # 29
```

`use` makes a library's public functions and constants available in the file
that says it, either unqualified or as `LIB::name`. Libraries are compiled
together with the program; only the functions actually reachable from the
program end up in the output, so unused library code costs nothing.

**Name resolution.** An unqualified call `f(...)` resolves to, in order:

1. a function `f` defined in the same file;
2. the one imported library that exports `f`.

If two imported libraries export `f`, the unqualified call is an error that
names both candidates; qualify it (`math::abs(x)`). A local definition always
wins over imports. Unqualified constants resolve the same way, after local
variables; constants with the same value in several libraries (for example
`INT_MAX` in `math`, `checked` and `prelude`) are not ambiguous.

Imports are not transitive: if `geom` uses `math`, a program that says
`use geom;` still needs `use math;` to call `math` functions itself.

## 4.2 Writing a library

A library is a QScript source containing only `fn`, `const` and `use`. Names
beginning with `_` are private.

```qscript
// shapes.qs
use math;

const UNIT = 1000;                       # public constant
fn _sq(x) { return x * x; }              # private helper
fn hypot(a, b) { return isqrt(_sq(a) + _sq(b)); }
fn area(w, h) { return w * h; }
```

```qscript
use shapes;
say hypot(3, 4);          # 5
say shapes::UNIT;         # 1000
# _sq(3) would be an error: private to shapes
```

Inside a library, calls to its own functions need no qualification, and it
may `use` other libraries (including in cycles). A top-level statement in a
library is an error.

Library names are identifiers, optionally qualified with `::` to form a
hierarchy: `use text::ascii;` refers to the file `text/ascii.qs`.

## 4.3 Where `use` looks

For `use NAME;` QScript tries, in order:

1. **Registered sources**: `Options::libraries` (C++), `qs_add_library`
   (C), or libraries attached to a module (`qs_module_add_library`).
2. **A resolver callback**: `Options::resolver` or `qs_set_library_resolver`,
   for hosts that store scripts elsewhere.
3. **Library directories**: for each directory added with `-L DIR`,
   `Options::library_paths` or `qs_add_library_path`, the file
   `DIR/NAME.qs` (with `::` in NAME turned into `/`).
4. **The embedded standard library**, unless disabled (`--no-stdlib`,
   `Options::stdlib = false`, `qs_set_stdlib(c, 0)`).

QScript never touches the filesystem unless a library directory was added.
Diagnostics in library code name the library file (`stdlib/math.qs:12:5`,
`lib/geo/area.qs:1:1`, ...).

```sh
qscript -L ./lib run main.qs
```

```cpp
qscript::Options o;
o.libraries["config"] = "const PORT = 8080;";
o.resolver = [](std::string_view name) -> std::optional<std::string> {
    return load_script_from_database(name);   // nullopt if unknown
};
o.library_paths = {"/usr/share/myapp/qs"};
auto p = qscript::compile("use config; return PORT;", o);
```

## 4.4 Modules: several sources as one program

A *module* compiles several sources into one program. Their items are
concatenated in order, so earlier sources act as a prelude: top-level `let`
variables of an earlier source are visible to later ones, and `fn`, `const`
and `use` from any member are visible to all.

```sh
qscript run setup.qs main.qs       # several files = one module
```

```c
qs_module *m = qs_module_create();
qs_module_add_file(m, "setup.qs");
qs_module_add_source(m, text, length, "main.qs");
qs_module_add_library(m, "util", util_src, util_len);   /* for `use util;` */
qs_program *p;
if (qs_module_compile(context, m, &p) == QS_OK) { /* ... */ }
qs_module_destroy(m);
```

A `return` in an earlier source ends the whole program, so preludes should
only declare things. Member diagnostics keep their own file names. The
16-variable limit applies to the merged top-level program.

## 4.5 The standard library

25 libraries ship embedded; [chapter 7](07-stdlib.md) documents every
function. List them with `qscript libs` and read any of them with
`qscript lib NAME`.

| | |
|---|---|
| Arithmetic | `math`, `checked`, `round`, `fixed`, `logic` |
| Number theory | `prime`, `ntheory`, `combin`, `seq`, `digits` |
| Bits and encodings | `bits`, `pack`, `codec`, `hash`, `checksum`, `random` |
| Domains | `date`, `time`, `units`, `geom`, `color`, `range`, `ascii` |
| Utilities | `prelude`, `testing` |

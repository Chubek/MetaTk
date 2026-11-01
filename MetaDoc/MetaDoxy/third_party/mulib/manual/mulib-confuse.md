# mulib-confuse

Dynamic, libconfuse-inspired configuration, recognized by the PEG/parsec engine.
It is not libconfuse's schema API or ABI.

Define `MULIB_IMPL_CONFUSE`, `MULIB_IMPL_PARSER`, `MULIB_IMPL_PARSEC`,
`MULIB_IMPL_PEG`, `MULIB_IMPL_AST`, `MULIB_IMPL_STRBUF`, `MULIB_IMPL_GLOB` before
including `mulib-confuse.h` in exactly one C11 translation unit.

```conf
port = 8080;
enabled = yes
server "main" {
    host = localhost
    ports = {80, 443,}
    labels = ["public", 'primary']
}
```

`mulib_confuse_parse(bytes,length,error)` parses an explicit byte range;
`mulib_confuse_load(path,error)` reads a regular file. Both require an entire
match and return an owned root section, or NULL with an optional structured
error. Destroy the root with `mulib_confuse_destroy`.

Supported syntax:

* `name = value`, optional semicolons, and arbitrary whitespace.
* `name [title] { entries }`, titles quoted or identifier-like.
* Lists in braces or brackets, comma-separated, optionally trailing comma;
  nested lists and heterogeneous values are allowed.
* Single/double quoted strings; escapes for backslash, matching quote, `n/r/t`.
* Bare identifiers/strings; signed decimal integers, decimal/exponent reals.
* Boolean `true/false`, `yes/no`, `on/off` (lowercase only).
* `#`, `//` line comments and nonnested `/* ... */` comments.

Names and bare strings begin with an ASCII letter or underscore; remaining
characters may also be digits or `_.:/-`. Quote paths and values containing
other characters. Numeric conversion is range checked; real conversion uses
the process C numeric locale (use the `C` locale for dotted decimals).
Duplicate options and sections are preserved in source order. No includes,
interpolation, default schema values or automatic type coercion are supplied.
Parser resource limits apply; this is not an unlimited configuration loader.
Embedded NUL bytes are rejected rather than truncated into C strings.

`mulib_confuse_get(parent,name,index)` borrows the nth matching child; NULL name
selects the nth child overall, including list elements. Sections carry optional
`title` strings. Values have SECTION/LIST/STRING/INTEGER/REAL/BOOLEAN kinds and
corresponding `as` union members. `getstr/getint/getreal/getbool` select the first
named child and return the supplied fallback if missing or differently typed.
All lookup pointers are valid only while the owning configuration remains alive.

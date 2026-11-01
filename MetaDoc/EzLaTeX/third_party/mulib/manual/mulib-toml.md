# mulib-toml

TOML tables and values mapped onto MuLIB's owned JSON DOM. This is a useful
**TOML 1.0 configuration subset, not full TOML conformance**.

## Integration

In exactly one C11 translation unit, define `MULIB_IMPL_TOML`,
`MULIB_IMPL_JSON`, `MULIB_IMPL_PARSEC`, `MULIB_IMPL_STRBUF`, and
`MULIB_IMPL_UTF8` before any MuLIB includes. Include `mulib-toml.h` normally
elsewhere. Dependencies are never enabled implicitly. Declarations support
C++17; no external library is required.

## API and ownership

```c
mulib_json_t *mulib_toml_parse(const char *source, size_t length,
                             mulib_toml_error_t *error);
char *mulib_toml_stringify(const mulib_json_t *object,
                          mulib_toml_error_t *error);
```

Parse returns a caller-owned JSON object, even for empty input. It copies keys
and strings; release the source at any time and use `mulib_json_destroy` for
the result. NULL source is accepted only with zero length. Writer input is
borrowed; free its returned NUL-terminated allocation with `free`. Neither
operation returns partial output on failure.

Optional diagnostics have a static message, zero-based byte offset, and
one-based byte line/column. Success clears the diagnostic. Writer errors use
0/1/1. Allocation/overflow errors, invalid UTF-8/control bytes, unsupported
syntax, conflicting definitions and nesting beyond 128 return NULL. Escaped
NULs in strings and quoted keys are retained; raw source NULs are rejected.

## Supported scope

- Bare or single/double-quoted keys, dotted keys, whitespace around dots,
  `[tables]`, nested tables, implicit parent-table promotion, `[[arrays.of.tables]]`
  and subtables of the most recent array element. Empty quoted keys work.
- Basic and literal strings, multiline basic/literal strings, initial newline
  trimming, CRLF normalization, basic multiline backslash continuation, and
  valid TOML `\b \t \n \f \r \" \\ \uHHHH \UHHHHHHHH` escapes. Surrogates and
  out-of-range Unicode scalar escapes fail.
- Lowercase booleans; signed decimal integers; unsigned `0x`, `0o`, `0b`
  integers; finite decimal floats and exponents. Numeric underscores must be
  between digits. Decimal leading zeros, `.5`, and `1.` are rejected as TOML
  requires.
- Arrays with nested/mixed values, multiline arrays, comments and trailing
  commas. Inline tables support nested values and dotted keys, but not trailing
  commas or newlines/comments between entries (TOML 1.0 behavior).
- UTF-8 (an initial BOM is tolerated), LF/CRLF, comments, and empty documents.

Duplicate keys/table definitions, reopening dotted-key-defined tables,
extending closed inline tables, and treating normal arrays as arrays of tables
are rejected. Dotted assignments cannot traverse header-defined tables.
Distinct array-of-table elements have independent subtable
definitions. Tables are objects and arrays of tables are arrays of objects.

**Not represented/supported:** dates/times (quoted dates remain strings), `inf`,
`nan`, TOML 1.1 extensions, and integers outside
±9,007,199,254,740,991. Rejecting the latter prevents silent precision loss in
the JSON double representation; it is deliberately stricter than TOML's
64-bit integer range. Floats use the JSON parser's finite-double range.

The writer requires an object root and rejects JSON null anywhere (TOML has no
null). Each root member becomes a quoted key assignment; nested objects become
inline tables and arrays are written inline. Formatting, comments, header
layout, and integer-base spelling are not preserved. All accepted DOM values
roundtrip; large integral doubles use a fractional/exponent spelling rather
than a lossy integer token. Cyclic/shared-ownership DOM graphs are not valid
writer inputs.

## Runnable example

```sh
cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude \
  examples/modules/toml.c -o ./mulib-toml
./mulib-toml
```

The example parses tables, dotted keys, arrays, inline tables and an array of
workers, then serializes and reparses them:

```toml
[service]
listen.host = 'localhost'
listen.ports = [8080, 8081]
[[workers]]
name = 'alpha'
limits = { connections = 100, enabled = true }
```

`tests/test_config.c` covers ownership, explicit byte lengths, Unicode,
malformed/conflicting definitions, depth bounds, and generated roundtrips.

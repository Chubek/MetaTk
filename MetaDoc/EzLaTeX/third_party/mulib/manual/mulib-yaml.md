# mulib-yaml

A small, owned-JSON-DOM configuration reader and block-style YAML writer. This
is a **documented YAML 1.2-style subset, not a complete YAML processor**.

## Integration

Define these switches before any MuLIB include in exactly one C11 translation
unit: `MULIB_IMPL_YAML`, `MULIB_IMPL_JSON`, `MULIB_IMPL_PARSEC`,
`MULIB_IMPL_STRBUF`, `MULIB_IMPL_UTF8`. Other translation units include
`mulib-yaml.h` without switches. No dependency switch is implicitly enabled.
Declarations also support C++17.

## API and ownership

```c
mulib_json_t *mulib_yaml_parse(const char *source, size_t length,
                             mulib_yaml_error_t *error);
char *mulib_yaml_stringify(const mulib_json_t *value,
                          mulib_yaml_error_t *error);
```

Parsing copies all keys and strings; the input may be released immediately.
Destroy the returned tree with `mulib_json_destroy`. Serialization borrows the
tree and returns a NUL-terminated allocation to release with `free`; NULL input
serializes as `null`. No partial tree/output is returned on failure. Optional
errors have a static `message`, a zero-based byte `offset`, and one-based byte
`line`/`column`; success clears them. Writer errors have location 0/1/1.

Empty or comment-only documents are JSON null. `source == NULL` is allowed only
with zero length. Invalid UTF-8, YAML-disallowed raw controls/noncharacters, duplicate keys,
allocation/overflow failures, and excessive nesting fail with NULL. Strings and
keys may contain NULs through quoted escapes and retain their explicit lengths.
JSON DOM append/set ownership rules apply when constructing writer inputs.

## Supported syntax

- One document; optional standalone `---` and final `...` markers, comments,
  UTF-8 including an initial BOM, LF/CRLF, space indentation of arbitrary width.
- Nested block mappings and sequences, null-valued entries, indentless sequences
  under mapping entries, and compact mapping entries such as `- name: alpha`.
- Single-line flow arrays and mappings with explicit values, nested collections,
  trailing commas. Plain flow keys need separation after their colon (or an
  immediately following collection); quoted keys also allow `{"key":1}`.
  Implicit null flow entries such as `{key}` or `{key:value}` are rejected,
  not reinterpreted as a different key/value pair.
- Simple string keys, including empty quoted keys. Plain mapping keys are kept
  as text (there is no implicit boolean/numeric key coercion). Reserved leading
  indicators such as `,` or a `-` followed by whitespace require quoting;
  `-name` stays legal, as do commas inside block keys (`foo,bar`).
- Plain strings (including `yes`/`no` and dates as strings), doubled-quote single
  strings, double-quoted strings with JSON escapes and YAML escapes
  `\0 \a \v \e \  \N \_ \L \P \xHH \UHHHHHHHH`. Unicode escape scalar values
  are validated; JSON-style UTF-16 surrogate pairs are also accepted.
- YAML 1.2 null spellings `null/Null/NULL/~`, `true/True/TRUE`,
  `false/False/FALSE`, signed decimal integers, finite decimal fractions and
  exponents (including `.5`, `1.`, and leading decimal zeros).

Integers are limited to ±9,007,199,254,740,991 so converting to the JSON DOM's
double never silently rounds them. Floats use the JSON parser's finite-double
range. Numeric strings should be quoted when their type must stay string.

Rejected features include tags, anchors, aliases, merge keys, complex keys,
directives, multiple documents, tab indentation, block/folded or multiline
scalars, multiline flow collections, compact nested sequences (`- - item`),
document markers with inline content, non-decimal numeric prefixes, and
infinities/NaNs. Use a separate indented
sequence line instead of compact nested sequences. Both reader and writer
bound collections to 128 levels, counting the root collection as level one
and counting empty containers too; a 129th nested container is rejected.
This module does not perform schema resolution, Unicode
normalization, or application-level configuration validation.

The writer emits mappings/sequences in block form, empty containers in flow
form, and quoted keys/strings with JSON-compatible escaping. It preserves JSON
types and values, not comments, source formatting, quote style, or YAML-specific
types. Additional escapes protect DEL/C1/noncharacters and Unicode line
separators that a JSON writer may leave raw. Large integral doubles use a fractional/exponent spelling
so the reader does not reinterpret them as an out-of-range integer token.
Exponent forms also include a decimal point for compatibility with common YAML
1.1 readers.

## Runnable example

```sh
cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude \
  examples/modules/yaml.c -o ./mulib-yaml
./mulib-yaml
```

The example parses nested service/worker settings, checks DOM members, writes
YAML, and parses the generated text again. For instance:

```yaml
service:
  name: MuLIB
  ports: [8080, 8081]
workers:
  - name: alpha
    enabled: true
```

`tests/test_config.c` adds malformed-input, Unicode, ownership, depth-limit, and
generated-DOM roundtrip coverage.

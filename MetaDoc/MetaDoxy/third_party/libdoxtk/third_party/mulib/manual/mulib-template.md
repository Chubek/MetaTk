# mulib-template

A length-delimited template renderer using borrowed JSON contexts and MuLIB
strbuf output. It is a small template language, not a Mustache/Handlebars
compatibility implementation or a general expression interpreter.

## Integration and API

Define `MULIB_IMPL_TEMPLATE`, `MULIB_IMPL_JSON`, `MULIB_IMPL_PARSEC`,
`MULIB_IMPL_STRBUF`, and `MULIB_IMPL_UTF8` before any MuLIB include in one C11
translation unit. Dependencies are not implicitly enabled. Other units include
`mulib-template.h` without switches; declarations are C++17-compatible.

```c
char *mulib_template_render(const char *source, size_t length,
                           const mulib_json_t *context, size_t *output_length,
                           mulib_template_error_t *error);
```

The result is malloc-owned and NUL-terminated; release it with `free`.
`output_length` is optional and counts bytes excluding the final terminator.
Explicit lengths preserve literal/context embedded NULs; use the returned
length rather than `strlen` in that case. Source and context are borrowed,
never modified or consumed, and need live only for the call. NULL context is
JSON null; NULL source is valid only with length zero. Empty output still
returns a non-NULL allocation.

Failure returns NULL, sets `output_length` to zero, and returns no partial
output. Optional errors contain a static message, zero-based byte offset, and
one-based byte line/column. Success clears errors. Invalid source UTF-8,
allocation/overflow failure, malformed/unknown tags, invalid paths, mismatched
controls, duplicate `else`, or control nesting beyond 128 fail. All syntax is
checked **before rendering**, including inactive branches and empty loops.

## Language

| Syntax | Meaning |
|---|---|
| `{{name}}` | Interpolate with HTML escaping of `& < > " '` |
| `{{{name}}}` | Raw interpolation; use only with trusted content |
| `{{! comment}}` | Ignore a tag |
| `{{#if path}}yes{{else}}no{{/if}}` | Conditional; `else` is optional |
| `{{#each path}}body{{else}}empty{{/each}}` | Iterate arrays or objects |

Controls may nest in any combination. No whitespace-stripping or automatic
standalone-line behavior is performed. Whitespace inside tags is trimmed.
Only `if`, `each`, optional `else`, and their matching closing tags are
recognized. There are no includes, helpers, arbitrary expressions, or an
escape syntax for literal opening delimiters.

Paths use dot-separated segments of ASCII letters/digits/underscore/hyphen or
UTF-8 non-ASCII bytes. Array indices are decimal segments (`users.0.name`).
Missing members/intermediate non-containers resolve to null; they are not
render errors. Special paths:

- `.` or `this`: current context; `this.name` reads the current member.
- `../name`: enclosing **loop** context; repeat `../` to ascend further.
- `@root.name`: original root, including inside loops.
- `@index`: zero-based current loop index (also for object iteration).
- `@key`: the current object's exact key, including embedded NULs; empty for
  array iterations or outside loops.

There is no implicit parent-member fallback. JSON keys containing a dot,
whitespace, or other path punctuation cannot be addressed directly, though an
object loop exposes their values with `.` and names with `@key`.

Null/missing, false, zero, empty string/array/object are false; other values are
true. Strings interpolate their exact bytes; other non-null values use compact
JSON spelling. Null/missing interpolate as empty text. Arrays/objects iterate
in DOM order; the loop context is each element/member value. A conditional
does not change context. Empty/non-container `each` uses its optional `else`.

HTML escaping is suitable for ordinary HTML text and quoted attributes, **not**
JavaScript, CSS, URL validation, or arbitrary unquoted attributes. Raw
interpolation intentionally bypasses it.

## Runnable example

```sh
cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude \
  examples/modules/template.c -o ./mulib-template
./mulib-template
```

The example renders a JSON user list through nested `each`/`if` blocks and
asserts that `Ada & Lin` becomes `<p>Ada &amp; Lin</p>`.
`tests/test_config.c` also checks nested alternates, inactive-branch errors,
Unicode, NULs, root/parent/index/key paths, and borrowed-context ownership.

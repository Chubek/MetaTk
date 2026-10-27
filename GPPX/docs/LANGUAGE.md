# GPPX language reference

## Input model and comments

Input is read line-by-line. A directive must start after optional horizontal whitespace and the configured marker (`#` by default). Other lines are expanded and sent to output. Object macros are expanded as identifiers unless the active macro style is `dollar` or `mustache`. In bare mode, expansion skips string/character literals, line comments and block comments (C-style by default); `expand_strings: true` permits expansion inside literals. Macro expansion does not evaluate inserted text as a new directive line.

Supported profiles:

| Profile | Directive | Macro form |
|---|---|---|
| `c` | `#` | `NAME`, `FUN(args)` |
| `percent` | `%` | `NAME`, `FUN(args)` |
| `template` | `@` | `{{NAME}}`, `{{FUN(args)}}` |
| `jackscript` | `#` | `$(NAME)`, `$(FUN(args))` |

`#syntax profile template` changes the grammar immediately; the next directive must use `@`. Individual syntax options: `directive` or `prefix`, `macro_style`/`macros`, `macro_open`, `macro_close`, `line_comment`, `block_open`, `block_close`, `expand_strings`, `strict`, `max_depth`, `max_output`, `max_includes`.

## Macro definition

```gppx
#define VALUE 42
#define SUM(a,b) ((a)+(b))
#define LOG(fmt,...) fmt, __VA_ARGS__
#define STRINGIFY(x) #x
#define CONCAT(x,y) x ## y
#macro BLOCK(x)
This is a multiline expansion of x.
#endmacro
#push_macro VALUE
#define VALUE temporary
#pop_macro VALUE
#undef VALUE
```

Function-like definitions must have `(` immediately after the name. Function invocations can have whitespace before `(`. Named arguments are token-substituted in the replacement body; nested function calls and nested comma-separated argument expressions are recognized. Recursive definitions are blocked by the active expansion stack, and expansion is also depth-limited. Whitespace and token-paste semantics are GPPX-specific; do not assume exact ISO C behavior.

`__FILE__`, `__LINE__`, `__COUNTER__`, and `__GPPX__` are builtins. `#line N` sets the next logical line number to N.

## Conditionals and expressions

```gppx
#if defined(DEBUG) && DEBUG >= 2
  enabled
#elifmatch regex "error" => "an error message"
  matched
#else
  disabled
#endif
```

`#if`, `#ifdef`, `#ifndef`, `#ifmatch`, `#elif`, `#elifmatch`, `#else`, and `#endif` support nesting; inactive branches do not expand. Integer expressions recognize decimal/hexadecimal/octal literals, unknown identifiers (zero), unary `! ~ + -`, `* / % + - << >> < <= > >= == != & ^ | && ||` and ternary `?:`. `&&`, `||` and `?:` avoid evaluating inactive branches. Division by zero raises an error. Pattern match syntax: `#ifmatch ENGINE PATTERN => SUBJECT`. Engine `regex` is built in; plugins register named matchers.

## Includes, blocks and resources

```gppx
#include "shared.gppx"
#include <shared.gppx>
#include_once "generated.gppx"
#import "once.gppx"
#pragma once
#repeat 3
A
#endrepeat
#for i in 1..3
Item i
#endfor
#for color in red,green,blue
color
#endfor
#raw
Literal VALUE and #directives are not processed.
#endraw
#embed "payload.bin" c_array
#embed "payload.bin" base64
#embed "payload.bin" string
```

Include resolution checks the containing file's directory, `-I` paths, then the current working directory; relative path resolution is predictable for nested files. `#pragma once`, `#include_once` and `#import` prevent subsequent expansions, including ordinary `#include`; cycles cause errors. Raw blocks ignore directives and expansion inside them except their nested `#raw`/`#endraw` delimiters. `#for` uses inclusive ranges and restores a previously defined variable on completion. Block terminators inside comments or nested raw/macro blocks do not close an enclosing loop.

`max_depth` bounds macro expansion, nested processing blocks, and reentrant function calls. Limits must be positive decimal integers. Boolean options accept `true`/`false`, `yes`/`no`, or `1`/`0`. String embeds escape binary bytes using fixed-width octal escapes, including NUL.

## Output and diagnostics

`#emit TEXT` emits expanded text; `#eval EXPRESSION` emits an integer. `#assert EXPRESSION`, `#error TEXT` and `#warning TEXT` provide diagnostics. `#rewrite "REGEX" => "REPLACEMENT"` registers a line rewrite performed after macro expansion; `#clear_rewrites` removes them. `#sink NAME TEXT` dispatches expanded content to a named registered sink (`stdout`, `stderr`, `discard`, or plugin-specific).

Unknown directives are errors by default. `--no-strict` or `#syntax strict false` copies unknown directives through unchanged. Loading a plugin from a source is possible with `#plugin "path/to/plugin.so"` (native code execution; use trusted files only).

## Configuration

`--config driver/config.yaml` enables a small YAML subset: `key: value` scalar settings, `include_path: path`, `plugin: path`, two dash-item lists `include_paths:` and `plugins:`, and one-level `syntax:`, `options:`, and `defines:` mappings with two-space indentation. Paths are relative to the configuration file. Quote simple values with matching single/double quotes; `""` is an empty scalar. Deeper mappings, aliases, arbitrary escapes, and more advanced YAML types are not supported.

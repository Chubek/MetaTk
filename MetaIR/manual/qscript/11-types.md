# 11. Strings, lists, grammars and matches

QScript has five types. `int` is the register value itself. The other four
are *handles* into the run's heap (`Session`, see chapter 9): the value in a
register is an index, and the object it names lives until the run ends.

| Type | Written as | Notes |
|------|-----------|-------|
| `int` | `42`, `0x2a`, `-7` | signed 64-bit, wrapping arithmetic (chapter 2) |
| `str` | `"text"` | bytes; no character semantics |
| `list<T>` | `[1, 2]`, `let xs: list<str> = []` | any element type, including lists |
| `grammar` | a declared name, `grammar { ... }` | chapter 5 |
| `match` | returned by `parse` | a node of a match tree |

Parameters and `let` may carry types: `fn f(xs: list<int>, g: grammar): match`.
A type annotation is required for an empty list literal, because its element
type cannot be inferred: `let xs: list<int> = [];`. Types are checked at
compile time; a mismatch is a `verify` diagnostic that names the expected and
the actual type.

## 11.1 Operators on strings and lists

| Expression | Meaning |
|-----------|---------|
| `a ~ b` | concatenation of two strings (`~` is the string operator) |
| `a == b`, `a != b` | equality; for `str`, bytewise |
| `a < b`, `a <= b`, `a > b`, `a >= b` | ordering of two `str` values, bytewise (`"apple" < "banana"`) |
| `xs[i]` | element `i` of a list; out of range is a trap |
| `xs[i] = v`, `xs[i] += v`, `xs[i] ~= s`, `xs[i]++` | element update; compound forms evaluate `xs` and `i` once |
| `g[i][j] = v` | nested lists update in place |

Strings have no index operator; use `.at(i)`. Comparing an `int` with a
`str` is a compile error. Ordering comparisons are defined only for `int` and
`str`.

## 11.2 Methods

A method is called with `receiver.name(args)`. A method name may be a
keyword (`s.sub(1, 2)`, `xs.last()`), because the name follows a dot.
Calling a method that does not exist for the receiver's type with that
number of arguments is a compile error: `no method nope with 0 arguments on str`.

Built-in methods are resolved statically. Chapter 9 explains how they are
lowered.

### Integers

| Method | Result | Notes |
|--------|--------|-------|
| `n.str()` | `str` | decimal text, e.g. `-12` |
| `n.chr()` | `str` | one byte; `n` must be `0..255`, else a trap |

### Strings

| Method | Result | Notes |
|--------|--------|-------|
| `s.len()` | `int` | length in bytes |
| `s.at(i)` | `int` | the byte at `i` (`0..255`), not a character; trap if out of range |
| `s.sub(from, count)` | `str` | `count` bytes starting at `from`; trap unless both fit |
| `s.find(t)`, `s.find(t, from)` | `int` | byte offset of the first `t`, or `-1`; `from` must be `0..len(s)` |
| `s.starts_with(t)`, `s.ends_with(t)`, `s.contains(t)` | `int` | `1` or `0` |
| `s.split(sep)` | `list<str>` | parts between occurrences of `sep`; `sep` must not be empty; empty parts are kept |
| `s.repeat(n)` | `str` | `n` copies; `n` must not be negative |
| `s.replace(from, to)` | `str` | every occurrence; `from` must not be empty |
| `s.to_int()` | `int` | signed 64-bit decimal; trap otherwise |
| `s.upper()`, `s.lower()` | `str` | ASCII letters only |
| `s.trim()` | `str` | strips space, tab, CR, LF, FF and VT from both ends |

### Lists

| Method | Result | Notes |
|--------|--------|-------|
| `xs.len()` | `int` | |
| `xs.push(v)` | `int` | appends `v`; the result is `0`, so use it as a statement |
| `xs.pop()` | `T` | removes and returns the last element; trap when empty |
| `xs.last()` | `T` | the last element without removing it; trap when empty |
| `xs.clear()` | `int` | removes everything; the result is `0` |
| `xs.reverse()` | `int` | reverses in place; the result is `0` |
| `xs.slice(from, count)` | `list<T>` | a new list with `count` elements from `from`; trap unless both fit |
| `xs.contains(v)` | `int` | for `list<int>` and `list<str>` only |
| `xs.sort()` | `int` | in place: numeric for `int`, bytewise for `str`; the result is `0` |
| `xs.join(sep)` | `str` | for `list<str>` only |

`contains` and `sort` are rejected at compile time for other element types.

### Grammars

| Method | Result | Notes |
|--------|--------|-------|
| `g.parse(text)` | `match` | parses from the rule `TOP` |
| `g.parse(text, rule)` | `match` | parses from `rule`; an unknown rule is a trap |
| `g.name()` | `str` | the grammar's declared name |
| `g.source()` | `str` | the full source the grammar was compiled from |
| `g.rules()` | `list<str>` | rule names; inherited rules first, then in declaration order |
| `g.has_rule(name)` | `int` | `1` or `0` |
| `g.extend(src)` | `grammar` | a new grammar with the rules in `src` replacing or adding rules; `g` is unchanged |

A failed parse is not an error: `parse` returns a `match` whose `ok()` is `0`,
and `error()` says what was expected and where. Chapter 5 covers grammars in
depth.

### Matches

A `match` is a node of the tree produced by a successful parse. A failed
match has no node: `ok()` is `0`, `error()` gives the message, and the
structural methods trap (`at()` and `child()`) or return `0` (`size()`,
`count()`, `has()`).

| Method | Result | Notes |
|--------|--------|-------|
| `m.ok()` | `int` | `1` for a successful match |
| `m.text()` | `str` | the matched input |
| `m.rule()` | `str` | the rule or alias name |
| `m.sym()` | `str` | the proto symbol, or `""` |
| `m.from()`, `m.to()` | `int` | byte offsets |
| `m.line()`, `m.column()` | `int` | 1-based position of the start (for a failed match, of the failure) |
| `m.size()` | `int` | number of **direct** children |
| `m.at(i)` | `match` | the `i`th direct child |
| `m.has(name)` | `int` | whether a **direct** child is named `name` |
| `m.count(name)` | `int` | direct children named `name`; `count("*")` counts all direct children |
| `m.child(name)` | `match` | the first direct child named `name`; trap if there is none (check `has` first) |
| `m.nth(name, i)` | `match` | the `i`th direct child named `name` |
| `m.children(name)` | `list<match>` | every direct child named `name`, in order |
| `m.find_all(name)` | `list<match>` | every **descendant** named `name`, in pre-order |
| `m.dump()` | `str` | the S-expression, as `qscript tree` prints it |
| `m.error()` | `str` | the failure message; `""` for a successful match |

`find_all` searches the whole subtree, while every other child accessor
looks only one level down. Searching for `"num"` in an expression tree
therefore finds every number, while `children("expr")` gives only the
expressions directly under the node.

## 11.3 Printing and the text output

`print` and `say` (synonyms) write one value per line. Their behaviour
depends on the type:

* An `int` goes to the program output: the `qs_output_fn` callback, the
  `output` of `metail::Execution`, and `qscript run` prints it on its own line.
* Any other type is rendered as text and needs a **text output**, installed
  with `qs_set_text_output` (chapter 6). Without one, the print traps with
  `printing a string, list, grammar or match needs a text output`.
  The driver always installs one.

When a program prints any non-integer, every `print` in it goes through the
host, so integers and text keep their order. With a text output installed,
the integers are rendered as decimal text; without one they are kept for
the program output as before.

Rendering:

| Value | Printed as |
|-------|-----------|
| `int` | decimal |
| `str` | its bytes (quoted as `"..."` only inside a list) |
| `list<T>` | `[a, b, c]`, elements rendered the same way, strings quoted |
| `grammar` | `grammar NAME` |
| `match` | the S-expression of the tree, or `(no match: MESSAGE)` |

## 11.4 Limits and errors

Heap objects and bytes are bounded by `Options::max_heap_objects` (default
1,000,000) and `Options::max_heap_bytes` (default 256 MiB), in the C++
interface. Exceeding either is a trap, not a crash. The general C context API
does not expose heap-limit setters; the generated-runtime bridge's C config
does provide them explicitly.

Where a program can run outside the VM:

* `qscript c` generates C99 for all five types. Integer/string/list-only programs
  carry a standalone C runtime. Grammar/match programs explicitly link
  `MetaChain::QScript` through `QScript-Generated.h` (chapter 10); they use the
  existing grammar engine, not a second parser or a VM fallback. Output and
  built-in diagnostics match the VM in both modes.
* `qscript native`, `build` and `object` support all five types. Integers, strings
  and lists retain the small assembly runtime and ASMtk/LDtk pipeline. Programs
  using grammars/matches are encoded by ASMtk and statically linked to the
  existing parser runtime using a C++ driver. They require static C++/C libraries
  at build time, but no compiler, VM, source files or QScript installation when
  deployed. See chapter 10 for the distinct artifact/linking contracts.
  Native output of a bare program (`qscript native` on an IL program without its
  unit) that calls a built-in is refused with `native output of a program that uses
  strings, lists, grammars or matches needs its unit; compile the source file`.

Use `run`, `--jit`, typed host calls, linked generated C, or static native
executables for grammar/match built-ins. CLI `call` remains integer-only. The IL dump (`ir`)
works for any program.

`die(message)` stops the run with `message` as the diagnostic (or `die` when
the message is empty). Every run-time trap in this chapter is reported as an
execute-stage diagnostic; `qscript run` also prints the source position of the
failing instruction when the program has one.

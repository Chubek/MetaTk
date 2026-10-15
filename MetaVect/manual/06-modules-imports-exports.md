# Chapter 6 — Modules, Imports, and Exports

**Status:** syntax implemented; imports are not loaded and exports are not
enforced across module boundaries yet.

## Module header

```mvct
module examples.first
import prelude
```

A source unit optionally opens with one `module` declaration giving a
dotted qualified name, followed by any number of `import` declarations
with dotted names, then bindings. An optional single semicolon may follow
each header line. A file with no header is a valid anonymous module.

## Bindings and exports

```mvct
export answer : Int = 42
let message : String = "hello"
plain = true
```

Each binding may carry `export`, `let`, both, or neither — all four shapes
parse. `export` marks the name as part of the module's public surface (a
promise the future linker will honor). `let` is accepted at the top level
for uniformity with `let` expressions. Bindings may sit adjacent with no
separator; newlines are not terminators.

## Current limits

- Import names are syntax only: no module is loaded, so a misspelled
  import is accepted today and must fail once the loader exists.
- Duplicate or conflicting exports are not yet diagnosed beyond duplicate
  top-level names (chapter 19).
- There is no visibility rule yet: every name is usable wherever it is in
  scope.

# Chapter 5 — Finding and loading MetaArchDB

## Directory layout

The convenience loader expects a database root containing architecture
subdirectories. For `amd64`, it constructs:

```text
ROOT/amd64/amd64.march
```

The root is the `MetaArchDB` directory, not its parent and not an individual
`.march` file. Target names must satisfy the name rules; the path-like names `.`
and `..` are explicitly rejected by this loader.

## Lookup precedence

1. A nonempty explicit root supplied to `target` or `--db`.
2. The value of `METAARCHDB`, when the environment variable is present.
3. `MetaArchDB` in the working directory, if it is a directory.
4. `../MetaArchDB`, if it is a directory.

An explicitly selected root is used as supplied. The loader does not search
other roots after discovering that a target is missing there. Avoid setting
`METAARCHDB` to an empty or obsolete path; unset it to restore directory discovery.

## C++ loading

```cpp
auto target = mair::target("riscv64", "MetaArchDB", 0);
auto names = mair::targets("MetaArchDB");
```

The last argument selects a target record inside the loaded specification. It
defaults to zero and is range-checked. `targets` returns sorted names whose
directory contains the expected `.march` file. Enumeration alone does not
validate every listed file; loading performs validation.

For a separately loaded specification, construct a `Target` from an owned
`metaarch::Spec`, a display name satisfying the identifier rules, and an optional
record index. That path is useful for application-supplied specifications and
tests. The handle shares the resulting specification across copies and does
not lazily load a backend during rendering or assembly.

[Previous](04-first-program.md) · [Contents](README.md) · [Next](06-target-inspection.md)

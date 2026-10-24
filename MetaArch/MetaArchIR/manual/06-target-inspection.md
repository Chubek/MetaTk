# Chapter 6 — Inspecting targets and instruction metadata

## Discover before emitting

Use the CLI listings to inspect the database revision your application will use:

```sh
build/marchir --db MetaArchDB --list-targets
build/marchir -T riscv64 --db MetaArchDB --list-registers
build/marchir -T riscv64 --db MetaArchDB --list-encoders
build/marchir -T riscv64 --db MetaArchDB --list-forms
```

Register rows contain name, width and encoding. Form rows contain index, mnemonic,
encoder and declared operand count. Encoder rows show the encoder name followed
by the compiled program's parameter names. None of these listings automatically
selects the correct form for an arbitrary operand sequence.

## Equivalent C++ queries

```cpp
auto target = mair::target("riscv64", "MetaArchDB");
auto candidates = target.forms("addi");
auto parameters = target.parameters("addi");
auto destination = target.register_info("x10");
bool known = target.supports("addi");
```

`forms` matches the form's mnemonic exactly and returns all matching indices in
database order. No match returns an empty vector. Unknown encoders and registers
throw `mair::Error` through the other two lookup functions.

`supports` answers a native-mnemonic question. It consults declared mnemonics and
aliases, including their configured case behavior, and supported native spelling
conventions. It does not establish that a particular form's required features
are enabled, that its operands fit, or that an encoder of the same name exists.

## Inspecting deeper metadata

`target.description()` exposes the selected `DsymTarget`; `target.spec()` exposes
the const MetaArch C++ specification interface. Use these for architecture,
ABI, feature and operand-record information not covered by convenience methods.

Parameter enumeration reflects the encoder's own naming/order. **Never zip it
blindly with a form's operand list.** The explicit mapping discussed in chapter 13
is still required. Also, `register_info` copies the record but borrows its string
fields from the target's shared specification; chapter 22 explains their lifetime.

[Previous](05-database-loading.md) · [Contents](README.md) · [Next](07-language-grammar.md)

# Chapter 19 — Building programs in C++

## A complete builder example

```cpp
#include <MetaArchIR/MetaArchIR.hpp>
#include <iostream>

int main() {
    try {
        auto target = mair::target("amd64", "MetaArchDB");
        mair::Program p;
        p.section(".text").global("answer").label("answer")
         .ins("mov", "$42", "%eax").ins("ret");
        std::cout << mair::render(p, target);
        return 0;
    } catch (const mair::Error &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
```

Link this translation unit with `MetaArch::ir`, as described in chapter 3. It
prints assembler text; it does not execute `answer` or directly encode its native
operands.

## Builder groups

Instruction construction uses `emit`, variadic `ins`, `encode` and `form`.
Structural directives use `section`, `global`, `label` and `align`. Data methods
are `ascii` and `data`; `comment` adds an explicit textual comment. These mutators
return `Program&` so calls can be chained in source order.

`emit` takes a vector of native operand strings; `ins` is convenient for a fixed
argument list. `data` takes one of `byte`, `word`, `long`, `quad` and a vector of
value strings. The C++ method does not take the leading-dot parser aliases.

## Inspection and source attribution

`size()` counts statements, not instructions or bytes. `empty()` checks for no
statements. `clear()` removes both statements and the duplicate-label registry.
`statements()` exposes a const vector view; it is not a mutable transformation
interface.

`at({file, line, column})` assigns a location to the most recently added statement.
It is a no-op on an empty Program. Generators can use it to connect encoding errors
back to their own source language. Do not keep vector references across appends
or `clear`, since these can invalidate them.

[Previous](18-data-and-endianness.md) · [Contents](README.md) · [Next](20-operand-api.md)

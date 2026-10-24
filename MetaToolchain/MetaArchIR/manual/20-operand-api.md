# Chapter 20 — Typed operands and explicit bindings

## Operand alternatives

The public types are intentionally small:

```cpp
using Operand = std::variant<int64_t, std::string, mair::SymbolRef>;
```

Within `mair::Operand`, an integer is an immediate-like value, a string is a
physical register name, and `SymbolRef` is a name plus signed addend. `Binding`
combines a parameter name with one such operand. These types represent assembly
inputs, not value-flow nodes or symbolic expressions for optimization.

Use the helpers to make intent clear:

```cpp
mair::Binding destination{"rd", mair::reg("x10")};
mair::Binding immediate{"imm", mair::imm(-1)};
mair::Binding reference{"sym", mair::symbol("callee", -4)};
```

`reg` and `symbol` validate identifier syntax. Actual register existence and
relocation compatibility depend on the chosen target and are checked later.

## Explicit form mapping

```cpp
std::vector<mair::Binding> operands{
    {"rd", mair::reg("x10")},
    {"rs1", mair::reg("x10")},
    {"imm", mair::imm(-1)}
};
mair::Program p;
p.form(11, operands);
```

For the shipped riscv64 `addi` form, this maps declared operand positions to
encoder inputs. Names alone are insufficient for form checking: positions still
determine which descriptor is applied to each value.

## Native operands are a separate interface

Do not pass a `Binding` to `ins` or a native addressing string to `reg` and expect
conversion. Native `ins` arguments become strings for an external assembler;
checked operands become named inputs to a database program. The API deliberately
keeps these assembly interfaces distinguishable.

Symbol operands are accepted only by low-level encoder execution, and only when
the named input actually produces a relocation record. Form execution rejects
them rather than pretending that a symbol's addend satisfies an integer operand
constraint.

[Previous](19-program-api.md) · [Contents](README.md) · [Next](21-parsing-and-errors.md)

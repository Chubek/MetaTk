# 6. Bindings and definitions

A definition associates a name with an expression. `:=` is the clearest binding marker; the parser also accepts `=` in definition contexts. `val` is useful in modules and in explicitly annotated bindings. A separate signature uses `::`, while an inline annotation after `val` uses `:`.

```metaml
base := 40
add :: Int -> Int -> Int
add x y := x + y
val answer : Int := add base 2
Test.equal "binding" answer 42
```

Bindings are immutable at the language level. A REPL may introduce a later binding with the same name, but that is shadowing rather than assignment to a mutable variable. Existing functions retain statically resolved top-level binding cells where the checker annotated them. This makes redefinition useful for exploration without implying every earlier closure is dynamically rebound.

The session processes a program in checked units. Earlier successfully executed units remain available if a later unit fails. Imports and IO may already have produced effects. Consequently, a source file is not an all-or-nothing database transaction, and retrying a failed file may repeat earlier effects. Avoid partial top-level side effects when building reloadable libraries.

Signatures are rank-one: a polymorphic value can be instantiated at different uses, but a function parameter does not automatically become rank-two polymorphic. Explicit signatures are particularly helpful where tacit composition might otherwise be ambiguous.

Prefer one top-level definition per line or block. A newline at column one can terminate the previous form. For multiline arithmetic or pipelines, indent the continuation or use parentheses. The parser's boundary handling is practical, not an implementation of unrestricted whitespace-insensitive ML syntax.

**Related:** chapter 11 explains inference; chapter 12 covers local bindings and recursion.

<!-- nav:begin -->
[Contents](README.md) · [Previous](05-values-and-numbers.md) · [Next](07-functions.md)
<!-- nav:end -->

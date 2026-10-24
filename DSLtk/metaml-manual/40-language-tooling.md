# 40. Frontend, Type, Module, and Loader

These host modules expose language-tooling operations without requiring native code for every editor command. Frontend wraps tokenization, parsing, inference, type formatting, and isolated evaluation. Type manipulates textual types. Module inspects the calling session's checked modules. Loader inspects search paths and performs explicit source loading.

```metaml
Test.equal "inference" (Frontend.type_of "fn x -> x + 1") (Ok "Int -> Int")
Test.equal "isolated evaluation" (Frontend.eval "double x := x * 2\ndouble 21") (Ok "42")
Test.equal "type unification" (Type.unify "'a -> Int" "String -> 'b") (Ok "String -> Int")
Test.check "known module" (Module.exists "List")
Test.check "resolved source" (Option.is_some (Loader.resolve "list"))
```

Frontend's nested evaluator uses a fresh session with core primitives, not a clone of the caller's globals and installed host modules. Output is forwarded to the calling session. A definition made there does not become a binding in the parent. This isolation is useful for small examples, but does not make arbitrary evaluation a secure, pure operation.

Type normalization renames variables consistently. `unify` returns a compatible type or Err; `equal` compares normalized type structure; `instance_of` checks a directional relationship rather than symmetric equality. `arity` counts function arguments in the parsed type. Unknown type constructor names are not silently accepted as existing session types in every tooling context.

Module queries return known names, members, submodules, signatures, and member types. Introspection reports the checked visibility surface; it should not be used to circumvent opaque sealing. Some host primitive namespaces are registered paths rather than ordinary structure definitions, so they need not appear as a source module in every query.

Loader's load operation mutates the calling session and can execute effects. Grant it only where source loading is intended. `resolve` is a search query, not proof that a file will remain unchanged before a later load.

**Sources:** Frontend, Type, Module, and Loader headers and D modules; chapter 45 contains the primitive signature index.

<!-- nav:begin -->
[Contents](README.md) · [Previous](39-parser-libraries.md) · [Next](41-cpp-embedding.md)
<!-- nav:end -->

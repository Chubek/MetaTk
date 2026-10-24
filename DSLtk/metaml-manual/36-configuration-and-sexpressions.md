# 36. TOML, YAML, and S-expressions

TOML and YAML modules provide configuration-oriented parsing over the Json value model. This gives callers a shared representation for strings, numbers, booleans, lists, and objects without introducing a second object API. They are practical in-tree parsers, not claims of exhaustive conformance to every version and extension of their respective standards.

```metaml
Test.equal "TOML scalar" (TOML.parse "answer = 42\n" |> Result.map (JSON.get "answer")) (Ok (Some (JNum 42.0)))
Test.equal "YAML scalar" (YAML.parse "name: demo\n" |> Result.map (JSON.get "name")) (Ok (Some (JStr "demo")))
Test.equal "S-expression" (SExpr.parse_one "(a (b c))") (Ok (SList [Atom "a", SList [Atom "b", Atom "c"]]))
```

TOML supports common tables, arrays of tables, dotted keys, and string forms. YAML supports common mappings, sequences, scalar forms, flow collections, and block strings. Check the source and fixtures before relying on a special feature such as anchors, arbitrary tags, or round-trip source preservation. A parse error returns Err with context; it should not become an unexplained native exception.

Rendering is normalized output, not a source editor. Comments, formatting choices, and original key spellings are not retained as syntax trivia. TOML rendering can fail when a Json value has no supported TOML form; handle its Result rather than assuming every JSON tree is representable.

SExpr uses `Atom String` and `SList [SExpr]`. `parse` accepts multiple top-level expressions; `parse_one` requires one. Quoting and escaping preserve atom boundaries when an atom contains spaces or punctuation. This makes it a compact substrate for syntax trees and tooling interchange.

ASTBuilder bridges generic trees to S-expressions, but that representation is distinct from arbitrary MetaML values. Use explicit conversion functions and check errors. For configuration files from untrusted sources, also bound input size and nesting externally.

**Sources:** [`TOML.h`](../metalib/cpp/TOML.h), [`YAML.h`](../metalib/cpp/YAML.h), [`SExpr.h`](../metalib/cpp/SExpr.h).

<!-- nav:begin -->
[Contents](README.md) · [Previous](35-json.md) · [Next](37-graphs.md)
<!-- nav:end -->

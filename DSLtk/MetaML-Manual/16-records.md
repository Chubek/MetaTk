# 16. Records and row polymorphism

Records associate names with values. A field is written `name = expression`; projection uses a dot after a record-valued expression. Field order does not determine record identity: runtime construction sorts labels for deterministic rendering and lookup.

```metaml
person := { name = "Ada", score = 40 }
get_name :: { name : String | r } -> String
get_name p := p.name
Test.equal "row polymorphism" (get_name person) "Ada"
Test.equal "field access" person.score 40
```

The type `{ name : String | r }` has a known `name` field and an open row for additional fields. A function using this type accepts a larger record without discarding its extra data. Row polymorphism is static: it is not a dynamic dictionary or unchecked member lookup.

Record patterns can unpack fields, and an open pattern uses `..` for additional fields. Use named fields when tuple positions would be hard to remember. A record returning `{ value = ..., warnings = ... }` is often clearer than a three-component nested tuple.

Record updates create another immutable record rather than mutating the original. The current parser supports a record-with form; keep update code and expected field types explicit, and use fresh construction if readability is better. Duplicate labels and inconsistent field types should be diagnosed rather than treated as arbitrary overwrite order.

Dot syntax has both module-path and record-projection uses. Uppercase module qualifiers, such as `List.map`, resolve through the module/primitive machinery; a lowercase value such as `person.name` is projected from its inferred record type.

Structural record comparison includes values, and equality includes labels. If fields contain callable values, generic equality can fail at runtime. For domain entities, compare a stable identifier or an explicit set of fields instead of relying on all fields being comparable forever.

**Sources:** record expression/pattern cases, row unification, `make_record`, and `record_field` in both runtimes.

<!-- nav:begin -->
[Contents](README.md) · [Previous](15-comprehensions.md) · [Next](17-pattern-matching.md)
<!-- nav:end -->

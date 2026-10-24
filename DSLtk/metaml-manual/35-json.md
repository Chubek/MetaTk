# 35. JSON data and conversion

The JSON module declares a `Json` ADT with `JNull`, `JBool`, `JNum`, `JStr`, `JArr`, and `JObj`. Parsing returns `Result Json String`, keeping malformed external input separate from interpreter diagnostics. Object values are lists of string/value pairs rather than opaque mutable tables.

```metaml
j := JSON.parse "{\"name\":\"Ada\",\"scores\":[1,2]}"
Test.check "JSON parse" (Result.is_ok j)
value := Result.unwrap j
Test.equal "member" (JSON.get "name" value) (Some (JStr "Ada"))
Test.equal "nested index" (JSON.get "scores" value |> Option.flat_map (JSON.at 1)) (Some (JNum 2.0))
Test.equal "compact rendering" (JSON.render (JArr [JBool true, JNull])) "[true,null]"
```

`render` produces compact text; `pretty` uses readable indentation. `get` searches an object member and `at` selects an array element, returning None where the operation does not apply or the member is absent. Matching the ADT directly is appropriate when the application needs to distinguish wrong shapes from missing data.

JSON numbers use Float in this representation. Large integer tokens can lose precision beyond binary64's exact-integer range. Encode identifiers as strings or provide an application-specific format when exact 64-bit decimal values must round trip.

`of_value` converts MetaML data using the module's conventions, including records as objects. It is a convenience conversion, not a reversible universal encoding of functions, abstract types, and foreign resources. For a stable public protocol, write explicit constructors and validate the expected schema after parsing.

Avoid assuming parse/render preserves comments, original whitespace, numeric spelling, or every duplicate-key distinction. Those are source-level properties rather than properties of a normalized data tree. Apply size/depth limits outside the interpreter when consuming hostile documents.

**Sources:** [`cpp/JSON.h`](../metalib/cpp/JSON.h), [`d/JSON.d`](../metalib/d/JSON.d). Chapter 45 lists the registered signatures.

<!-- nav:begin -->
[Contents](README.md) · [Previous](34-hashes-and-uris.md) · [Next](36-configuration-and-sexpressions.md)
<!-- nav:end -->

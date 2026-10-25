# 33. KernelRegistry queries

`KernelRegistry.hpp` offers validated `.optk` declarations by keyword, tag, kind,
and use case. A registry copies metadata from a Context and can outlive it:

```cpp
metail::KernelRegistry registry(context);
metail::KernelQuery query;
query.keywords = "multiply power";
query.tags = {"strength-reduction"};
query.use = "shifts";
query.kind = "rewrite";
auto kernels = registry.search(query);
auto files = registry.request(query);
```

All filters are combined with AND. Every whitespace-separated keyword must occur
literally in searchable metadata; use-case terms must occur in uses. Every tag
must match an entire tag. Matching folds ASCII case and is not regex or semantic
applicability analysis. Results are sorted by registered name.

`search` returns metadata values; `paths` returns absolute paths; `request(query)`
returns `KernelFile` values with current disk contents. `find(name)` is an exact,
case-sensitive lookup returning a borrowed pointer or nullptr. `request(name)`
throws for an unknown name or unreadable file. Empty queries select all kernels;
unmatched queries return empty vectors.

Convert a homogeneous selection with `KernelRegistry::passes(kernels)` and pass
the string to `Context::optimize` or `analyze`. Empty or mixed analysis/transform
selections are rejected, preventing accidental all-pass execution.

After loading additional kernels, call `refresh(context)`. Refresh snapshots the
already-loaded declarations; it does not reexecute edited files. Create a new
Context to reload changed definitions.

[Contents](README.md) · [Next: Custom kernels](34-custom-kernels.md)

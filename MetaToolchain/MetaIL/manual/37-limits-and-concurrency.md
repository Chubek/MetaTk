# 37. Limits and concurrency

The textual parser limits source input to 8 MiB and token count to 200,000. It also
limits expression nesting/tree depth to 256. These checks reject pathological
inputs before recursive processing grows without bound.

Structured import/export has its own limits:

| Quantity | Limit |
|---|---:|
| Expression depth | 256 |
| Functions, parameters, statements, expression occurrences combined | 200,000 |
| Aggregate copied string bytes | 8 MiB per import/export |
| Individual array count | 200,000 |
| Optimization rounds | 1–12 |

Shared C subtree pointers are expanded by occurrence, so sharing does not bypass
the item budget. Cyclic descriptor graphs hit the depth check and fail. The limits
do not authorize invalid pointer/count pairs: C input storage must still be valid.
The C++ Expr builder validates expression shape and depth before accepting a tree.

Calls on one Context must be serialized by the caller. Separate contexts have
independent Lua states and registries and may be used concurrently. C++ Module
values can be shared for read-only use; synchronize mutation as for ordinary STL
containers. A registry snapshot supports concurrent const queries, but refresh
must not race with readers.

These are structural limits, not CPU quotas for arbitrary trusted Lua kernels.
For untrusted code execution, application-level isolation is a separate concern.

[Contents](README.md) · [Next: Testing](38-testing-and-debugging.md)

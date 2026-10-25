# 17. C expression descriptors

`metail_expr` is a plain descriptor. Initialize unused fields to zero, then select
the fields relevant to its kind:

| Kind | Relevant fields |
|---|---|
| `METAIL_INT` | `value` |
| `METAIL_VAR` | `name` |
| `METAIL_OP` | `op`, `args`, `arg_count` |
| `METAIL_CALL` | `name`, `args`, `arg_count` |

Here is an addition tree with stack-owned descriptors:

```c
metail_expr x = {.kind=METAIL_VAR, .name="x"};
metail_expr zero = {.kind=METAIL_INT, .value=0};
const metail_expr *args[] = {&x, &zero};
metail_expr sum = {
    .kind=METAIL_OP, .op="add", .args=args, .arg_count=2
};
```

Argument order is semantic. A descriptor may be referenced from multiple parents;
the bridge copies each occurrence into the canonical expression tree. It does not
interpret shared C pointers as a single evaluation or introduce SSA identities.

Descriptors and arrays are borrowed for the duration of a structured operation.
Keep all referenced storage alive and unmodified until that call returns. Cycles,
missing required pointers, invalid arities, and excess depth produce errors.
As with ordinary C APIs, pointers must refer to readable storage for the declared
counts; the library cannot validate arbitrary dangling addresses.

`metail_operator_arity("add")` returns 2, `"neg"` returns 1, `"select"` returns 3,
and unknown or NULL operator names return -1.

[Contents](README.md) · [Next: C modules](18-c-modules.md)

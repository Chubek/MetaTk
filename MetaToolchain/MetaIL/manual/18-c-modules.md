# 18. C statements, functions, and modules

`metail_stmt` describes one statement. `name` is used by `METAIL_LET` and
`METAIL_LABEL`; `expr` by let, return, eval, assume, and branch; `target` by goto;
and `yes`/`no` by branch. Enum values are declared in `MetaIL.h`.

Continue the expression example from chapter 17 with:

```c
metail_stmt statements[] = {
    {.kind=METAIL_LABEL, .name="entry"},
    {.kind=METAIL_RETURN, .expr=&sum}
};
const char *parameters[] = {"x"};
metail_function function = {
    .name="identity",
    .params=parameters, .param_count=1,
    .statements=statements, .statement_count=2
};
metail_module module = {
    .name="example", .functions=&function, .function_count=1
};
```

These are caller-owned records, not handles allocated by MetaIL. They can be
constructed on the stack, in an arena, or in your application's object model.
The operation reads them synchronously and creates its own runtime tables.

A NULL array pointer is permitted when its count is zero. Parameters and
statements may be empty; a validated module must contain at least one function.
Set module `name=NULL` to omit the module declaration.

To edit a caller-owned program, modify its descriptors before the next call. For
library-returned snapshots, keep their nested storage read-only and build your own
descriptors for edits. Release only library-allocated snapshots with
`metail_module_free`; never pass a stack-built module to that function.

[Contents](README.md) · [Next: C operations](19-c-operations.md)

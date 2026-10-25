# 19. C structured operations

The structured operations take a `const metail_module *` and do not parse text:

| Function | Output |
|---|---|
| `metail_module_check` | Status only |
| `metail_module_emit` | Allocated canonical MetaIL text |
| `metail_module_analyze` | Allocated JSON report |
| `metail_module_optimize` | Owned structured module, optional JSON statistics |

Optimization creates an independent result and leaves your input descriptors
unchanged, including when a pass fails. For example:

```c
metail_module *result = NULL;
char *stats = NULL;
metail_status status = metail_module_optimize(
    ctx, &module, "AddZeroRight", 2, &result, &stats);
if (status == METAIL_OK) {
    const metail_stmt *ret = &result->functions[0].statements[1];
    /* Inspect ret->expr->kind, name, value, op, or args. */
    (void)ret;
}
metail_free(stats);
metail_module_free(result);
```

`metail_parse(ctx, source, filename, &result)` is the explicit text-to-structure
entry point. Its result has the same ownership contract as structured optimization.
The filename is diagnostic context, not a file to open.

Pass filters are NULL/`"all"` or comma-separated exact kernel names. Empty names
and incorrect pass kinds are errors. Optimization rounds must be 1–12. Validation
checks IR structure and CFG references, but does not prove local initialization,
callee existence, or arithmetic preconditions. Result output slots are NULL on
failure, and the context remains reusable.

[Contents](README.md) · [Next: Ownership](20-ownership-and-errors.md)

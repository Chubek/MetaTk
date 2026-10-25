# 16. C runtime contexts

Include `MetaIL.h` from C11. A `metail_context` owns an independent Lua runtime,
its loaded kernel declarations, and its latest diagnostic. Create and destroy it
explicitly:

```c
char error[2048];
metail_context *ctx = metail_create(NULL, error, sizeof error);
if (!ctx) {
    fprintf(stderr, "%s\n", error);
    return 1;
}
/* Perform operations with ctx. */
metail_destroy(ctx);
```

Pass an explicit runtime root instead of NULL when deployment paths are known.
The root contains `lua/` and `optimize-kernels/`. With NULL, discovery checks
`METAIL_HOME`, the current directory, and configured install/source roots. An
explicit home or environment setting is authoritative; an invalid one is an error.

The creation diagnostic is copied into your buffer and truncated if necessary.
Subsequent operations return `metail_status`; inspect `metail_last_error(ctx)` on
failure. That pointer is borrowed until the next operation. Do not use one context
concurrently without caller synchronization.

Construction does not require loading all kernels. Validation and emission can
operate before the first analysis, optimization, or kernel inventory request.
Those requests load bundled kernels lazily. Additional directories can be loaded
with `metail_load_kernels`.

`metail_destroy(NULL)` is valid. After destruction, borrowed context data is
invalid; independently allocated output strings and structured modules remain owned
by their caller until explicitly released.

[Contents](README.md) · [Next: C expressions](17-c-expressions.md)

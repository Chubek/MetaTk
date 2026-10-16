# libchomsky3

libchomsky3 compiles extended regular expressions to bytecode, optionally
executes them with SLJIT, and exposes C source generation and serialization.
The public API is in `chomsky3/chomsky3.h`.

```c
#include <chomsky3/chomsky3.h>

int main(void) {
    chomsky3_context_t *ctx = chomsky3_context_new();
    if (!ctx) return 1;
    chomsky3_error_t error;
    chomsky3_pattern_t *pattern = chomsky3_compile(
        ctx, "[a-z]+", CHOMSKY3_TARGET_BYTECODE, CHOMSKY3_FLAG_NONE, &error);
    if (!pattern) { chomsky3_context_free(ctx); return 1; }
    bool matched = chomsky3_exec(pattern, "hello", 5, NULL);
    chomsky3_pattern_free(pattern);
    chomsky3_context_free(ctx);
    return matched ? 0 : 1;
}
```

Pass an initialized `chomsky3_match_t` to retrieve match positions and groups;
release its allocated group storage with `chomsky3_match_free`. Patterns must
be released before their context. Check `chomsky3_jit_available()` before
requesting JIT compilation. Match lengths are byte lengths, not character counts.

SWIG source bindings can be generated with
`bash bindings/generate-bidnings.sh --language python`. Compile the generated
wrapper against the public headers, libchomsky3, and your language's runtime.

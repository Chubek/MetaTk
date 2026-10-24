# 43. Embedding through the C API

`metalib/c/MetaML.h` is a C99-compatible API backed by a separately compiled C++20 adapter. Link the `metaml-c` target and a C++ runtime. Unlike the `.c` fragments under `metalib/cpp`, a client including this header is genuinely compiled as C; the test suite verifies that distinction.

```c
#include "metalib/c/MetaML.h"
#include <stdio.h>
#include <string.h>
int main(void) {
    metaml_c_session *s = NULL;
    metaml_text text = {0};
    int status = 1;
    const char *source = "List.sum [10, 20, 12]";
    if (metaml_session_create(&s) != METAML_OK) return 1;
    if (metaml_c_load_stdlib(s, "metalib") != METAML_OK) goto done;
    if (metaml_c_eval(s, source, strlen(source), &text) != METAML_OK) goto done;
    fwrite(text.data, 1, text.size, stdout);
    status = 0;
done:
    if (status) fputs(metaml_last_error(s), stderr);
    metaml_text_free(&text);
    metaml_session_destroy(s);
    return status;
}
```

Initialize each `metaml_text` to zero and free it before reusing it as an output argument. Results are allocated by the adapter and must be released with `metaml_text_free`, which also clears the structure. The byte count excludes a terminating NUL; embedded NUL bytes are valid. Failed output-producing calls clear the output structure.

Statuses distinguish invalid arguments, language errors, and host failures. `metaml_last_error` returns borrowed diagnostic text valid until the next mutating operation or session destruction. Copy it if longer retention is needed. No C++ exception is allowed to cross the ABI.

`metaml_c_type_of` checks without evaluation. `metaml_output` copies captured ordinary IO output and can clear it after a successful copy. Evaluation returns canonical MetaML notation, not a typed native value tree or unquoted string contents. The `metaml_c_*` names and `metaml_c_session` type avoid collisions with the older optional `CBridge.h` ABI; the two APIs are distinct and their session pointers are not interchangeable.

Fresh sessions contain core primitives only. Install the portable host bundle with `metaml_install_metalib`, and grant selected names through `metaml_allow_primitives`. Installation itself does not grant access. There is no C callback-registration API in this initial adapter; use the C++ low-level API when native callbacks are required.

<!-- nav:begin -->
[Contents](README.md) · [Previous](42-d-embedding.md) · [Next](44-sandboxing-testing-deployment.md)
<!-- nav:end -->

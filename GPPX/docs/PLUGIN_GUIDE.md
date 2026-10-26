# Authoring a GPPX plugin

The recommended plugin interface is the C ABI in `GPPX.h`. Each dynamic library must export `gppx_plugin_init` and should check `abi_version` before calling any host function. Do not throw across the plugin boundary. Keep plugin callback state alive for the entire engine lifetime.

```c
#include <GPPX.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static int shout(GPPX_State *state, const char *text, char **out, void *data) {
    (void)state; (void)data;
    size_t n = strlen(text);
    *out = (char *)malloc(n + 2);
    if (!*out) return 1;
    for (size_t i = 0; i < n; i++) (*out)[i] = (char)toupper((unsigned char)text[i]);
    (*out)[n] = '!'; (*out)[n+1] = '\0';
    return 0;
}

#ifdef _WIN32
__declspec(dllexport)
#endif
int gppx_plugin_init(const GPPX_HostAPI *host, GPPX_State *state) {
    if (!host || host->abi_version != GPPX_ABI_VERSION) return 1;
    return host->register_directive(state, "shout", shout, NULL);
}
```

On Linux (the callback uses only the provided host API):

```sh
cc -std=c11 -fPIC -shared -Iinclude examples/c_plugin/shout.c -o shout.so
./build/gppx --plugin ./shout.so -e '#shout Hello, plugin'
```

This prints `HELLO, PLUGIN!`. Every directive handler is called with the raw argument text, and its `replacement` is inserted into output with the original directive's newline if needed. Callable macro hooks receive already-expanded argument strings. `register_rewriter` replaces each expanded line with its returned text. Reader hooks are selected by the extension string such as `.alt`. Embed hooks receive raw byte sequences and lengths. Pattern matchers return positive for a match and 0 for no match; negative signals failure. A syntax hook receives the value after its registered option name.

**Errors:** callbacks return nonzero on failure. If a replacement is returned, allocate using `malloc` and let GPPX free it. Hooks are only valid while their dynamic library remains loaded; GPPX does not provide safe hot unloading or sandboxing. A failed plugin initialization should not leave global state or dangling callbacks.

See `plugins/webber/Assembler.cpp` for a complete plugin with four different categories. `#embed "examples/webber/sample.wbr" wbr_asm` runs its example two-pass assembler and emits C-style byte literals.

For embedding GPPX in C++ without a dynamically loaded plugin:

```cpp
#include <GPPX.hpp>
#include <iostream>
int main() {
    gppx::Engine engine;
    engine.define("VERSION", "3");
    engine.registerDirective("hi", [](std::string_view arg) {
        return "hello " + std::string(arg);
    });
    std::cout << engine.process("#hi world\nVERSION\n");
}
```

Link with `-lgppx` and the appropriate library search/runtime path. The C API provides `gppx_create`, `gppx_process`, `gppx_process_file`, and the extension registration functions. See `tests/test.cpp` for C and C++ embedding coverage.

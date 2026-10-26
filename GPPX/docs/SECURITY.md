# Security and current limitations

- Plugins are native shared libraries running in the host process and can access files, memory, environment variables and other resources. There is **no plugin sandbox**. Do not load untrusted plugins.
- `#include`, `#embed` and `#plugin` can access the filesystem. Processes running on untrusted input should run in a filesystem sandbox and normally disable source-controlled plugin loading externally.
- The default output limit, macro expansion depth and include recursion limit prevent common accidental explosions, but hostile inputs may consume memory/CPU via large regexes, huge files or native plugin code. Set external process CPU/memory limits.
- The expression evaluator uses 64-bit signed integer values with two's-complement-style wraparound for basic arithmetic. This is **not** the ISO C preprocessor's exact type, integer promotion or macro expansion model.
- Comments, string literals and token concatenation are deliberately simplified; this is a programmable text preprocessor, not a standards-conforming C/C++ preprocessing front-end.
- C ABI processing is NUL-terminated and not binary-safe for arbitrary input/output; use the C++ API and byte-specific embedding for binary resources.
- Config files accept only a small subset of YAML; this avoids a dependency but is not a full YAML parser. Escape sequence decoding is limited.
- Plugins cannot be safely unloaded individually. A plugin initialization failure after partial registration should be treated as an error requiring a fresh Engine instance.
- Engine instances are stateful and are not thread-safe for concurrent calls. Use an Engine per concurrent worker.
- Source spans are tracked at file/line granularity, not columns. There is no debug symbol table/source-map format yet.

# Memory and Ownership

`archld_add_memory` copies its buffer. Borrowed image, symbol-name, and error pointers remain valid until free. Each instance requires exclusive mutation access; successful read-only queries may be shared with separate output streams.

See the neighboring chapters for the surrounding workflow and the project README for build commands and current limitations.

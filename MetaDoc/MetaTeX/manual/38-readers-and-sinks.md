# 38 — Readers and sinks

The binary I/O interfaces are declared in `MetaTeX-Reader.h` and
`MetaTeX-Sink.h`, under `metatex::io`. A `Reader` fills a caller-provided buffer,
returns zero at EOF, and throws on errors. `readAll` enforces a configurable
byte limit, defaulting to 16 MiB.

Available readers include files, borrowed C++ streams, borrowed memory,
POSIX descriptors, and mapped regular files. Descriptor readers can optionally
own their descriptor. Mapped readers handle empty files and copy only the
requested bytes; they are not a scanner-specific source format.

```cpp
#include "MetaTeX-Reader.h"
#include "MetaTeX-Sink.h"
std::string storage = "Hello";
metatex::io::MemoryReader input(storage);
metatex::io::MemorySink output;
output.write(input.readAll());
auto bytes = output.take();
```

Memory readers borrow their input, so keep the backing storage alive. File,
memory, and descriptor sinks implement `write` and `flush`; descriptor writes
handle short writes. Descriptor ownership and flush semantics are documented
by the concrete type and platform.

`readFile` and `writeFile` are convenient native-engine helpers using these
interfaces. They do not imply transactional replacement of an existing file.
When an application needs atomic publishing, render successfully first and
apply its own temporary-file and rename protocol. The TeX scanner validates
UTF-8 separately from binary I/O; a reader can transport bytes that are not
valid MetaTeX source.

[Contents](README.md)

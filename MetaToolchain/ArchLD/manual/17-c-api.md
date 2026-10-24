# C API

Describe `archld_new`, `archld_add`, `archld_add_memory`, `archld_link`, `archld_write_elf`, `archld_write_map_file`, symbol/image queries, diagnostics, and `archld_free`. Errors are sticky; failed instances should be released.

See the neighboring chapters for the surrounding workflow and the project README for build commands and current limitations.

The central lifecycle is:

```c
ArchLD *ld = archld_new(catalogue, "ELF64", 0x401000);
if (!ld || archld_error(ld)[0]) { /* report */ }
archld_add(ld, "entry.o");
archld_link(ld);
uint64_t entry;
archld_symbol(ld, "_start", &entry);
archld_write_elf(ld, "a.out", entry);
archld_free(ld);
```

`archld_add_memory` is useful for archive servers and language bindings; it
copies the supplied bytes before returning. `archld_image` and
`archld_image_size` expose the linked image for inspection or custom writers.

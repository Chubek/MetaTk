# Chapter 18 — Serialization, files, readers, and caching

[Previous](17-resource-limits-and-security.md) · [Contents](README.md) · [Next](19-vmweave-builder.md)

## Machinery wire format version 1

The binary is padding-free and little-endian, independent of C struct layout:

```text
4 bytes: "MVM\0"
u32: version = 1
u32: import_count
u32: function_count
each import: name, u32 arity
each function: name, u32 arity, u32 registers, u32 instruction_count, instructions
name: u32 byte_length followed by ASCII identifier bytes, no terminating NUL
instruction: u32 opcode, u32 a, u32 b, u32 c, i64 immediate
```

An instruction occupies 24 wire bytes. The i64 is its two's-complement bit
pattern. Opcode numbers follow `mvm_opcode` declaration order: NOP 0, CONST 1,
NIL 2, BOOL 3, MOV 4, ADD through SHR 5–14, EQ/LT/LE/NOT 15–18,
JMP/JZ/CALL/HOST/RET 19–23, ARRAY/GET/SET/LEN 24–27.

Changing wire semantics requires a new format version. Binary modules do not
contain source-line maps, heap objects, host callback addresses, native
bindings, or a VM heap snapshot. Do not serialize `mvm_instruction` with raw
`fwrite`: native padding, enum representation, and endian order are not the wire.

## Buffers and readers

Initialize `mvm_buffer buffer = {0}` and dispose with `mvm_buffer_dispose`.
There is no implicit NUL terminator. `mvm_buffer_append` preserves contents/size
on failure and requires nonaliasing input. `mvm_serialize` verifies and appends
atomically to an initialized buffer.

`mvm_deserialize(bytes, length, max_bytes, &module, &error)` consumes the entire
input, rejects trailing data and malformed/version-mismatched content, verifies
the result, and returns a new owned module. Output is null on failure.
`mvm_reader_init`, `mvm_reader_bytes`, `mvm_reader_u32`, and `mvm_reader_u64`
provide bounded borrowed reads; a failed read leaves its cursor unchanged.

## Direct writes versus atomic replacement

`mvm_load_file` detects binary magic, otherwise compiles assembly.
`mvm_read_file` appends atomically to a buffer, with a byte limit. Direct
`mvm_write_file` and `mvm_save_file` can leave partial files on write failure.

`mvm_write_file_atomic` and `mvm_save_file_atomic` create an exclusive sibling,
close it, then rename into place. On supported replacement platforms this
preserves the previous destination until commit. A platform whose rename
refuses replacement returns `MVM_IO` without deleting the old destination.
Cleanup of staging files is attempted on failure.

Atomic rename is not fsync durability, a multi-file transaction, or preservation
of permissions/xattrs. Control the destination directory. `mvm_path_join`
only performs a lexical slash join; an absolute leaf replaces the base, and
`..` is not normalized. Its allocated result needs `free`.

## Snapshot cache

`mvm_cache_create(capacity)` creates a bounded LRU of independently cloned
verified modules. `mvm_cache_put` inserts/replaces a snapshot;
`mvm_cache_get` looks up a borrowed module; `mvm_cache_clear` removes entries;
`mvm_cache_destroy` releases the cache. Modifying the original
does not update its cached snapshot. `get` returns a borrowed view that can
be invalidated by insertion, clearing, or destruction. Synchronize cache access;
clone a borrowed module before retaining it across cache mutation.

Caching does not persist VM state or reuse threaded-dispatch preparation.
Choose cache keys that include the source/compiler/profile identity needed by
your application, not just a convenient filename.

**References:** [wire API](../Machinery/BinarySerialization.h), [file API](../Machinery/OS.h), [cache](../Machinery/Cache.h).

/* MetaLD embedding ABI v1, ISO C11. Link MetaLD::metald.
 * Uses the full MetaArch/ArchBFD runtime, and may coexist with MetaAS.
 * The separate ArchLD JSON satellite must not be linked into this process.
 * Handles own copied inputs and output images. Mutators require caller exclusion;
 * successful linked images/symbols are safe for concurrent read-only queries.
 * No function exits the host process; failures return status and diagnostics.
 */
#ifndef METATOOLCHAIN_METALD_H
#define METATOOLCHAIN_METALD_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define METALD_ABI_VERSION 1u
typedef struct MetaLD MetaLD;
enum { METALD_OK, METALD_INVALID, METALD_BAD_STATE, METALD_ERROR, METALD_NO_MEMORY, METALD_NOT_FOUND };
enum { METALD_COLLECTING, METALD_LINKED, METALD_FAILED };
typedef struct MetaLDError { int32_t code; char message[1024]; } MetaLDError;
/* Fixed ABI v1 layouts, without packing. All input strings are copied/consumed
 * during create. target: ELF64 (x86-64) or ELF32 (i386), little-endian. */
typedef struct MetaLDOptions {
    uint32_t abi_version;
    size_t struct_size;
    const char *catalogue, *target;
    uint64_t base;
} MetaLDOptions;
typedef struct MetaLDSymbol {
    const char *name; /* Borrowed from linked context until free. */
    uint64_t address, size;
} MetaLDSymbol;
/* Return 1 with an address, 0 if not found, -1 on failure. On failure optionally
 * write a NUL-terminated message (capacity includes NUL). All arguments except
 * user are borrowed only during the callback. Successful resolutions are cached.
 * Called for missing strong globals AFTER archive extraction. Undefined weak
 * symbols retain the existing zero fallback. Do not reenter/free the context. */
typedef int (*MetaLDResolver)(void *user, const char *name, uint64_t *address,
                              char *message, size_t capacity);

uint32_t metald_abi_version(void);
/* Defaults: ELF64, base 0x401000, default catalogue. NULL-safe. */
void metald_options_init(MetaLDOptions *);
/* Catalogue: explicit, METALD_CATALOGUE, installed, source default (in that order).
 * Returns NULL on failure; optional error gets a bounded NUL-ended diagnostic. */
MetaLD *metald_create(const MetaLDOptions *, MetaLDError *error);
void metald_free(MetaLD *); /* NULL-safe. */
const MetaLDError *metald_error(const MetaLD *); /* Borrowed until next mutator/free. */
int32_t metald_state(const MetaLD *); /* NULL -> METALD_FAILED. */

/* Collecting only. add_memory copies an ELF object or ordinary ar archive;
 * the name is diagnostic context. NULL data is valid only when size is zero.
 * define installs a strong absolute symbol; duplicate strong definitions fail.
 * ELF64 reserves _GLOBAL_OFFSET_TABLE_ for its internally allocated GOT.
 * The resolver/user is borrowed until free. NULL resolver disables the hook. */
int32_t metald_add_file(MetaLD *, const char *path);
int32_t metald_add_memory(MetaLD *, const char *name, const void *data, size_t size);
int32_t metald_define(MetaLD *, const char *name, uint64_t address);
int32_t metald_set_resolver(MetaLD *, MetaLDResolver, void *user);
/* One shot: COLLECTING -> LINKED or FAILED. Parse/link/write failures are sticky;
 * recreate a failed context. Invalid argument/state calls leave state unchanged. */
int32_t metald_link(MetaLD *);
/* Borrowed image is valid until free. Before LINKED/for NULL returns NULL/0.
 * Bytes are relocated for metald_image_base(), not the buffer's host address;
 * no executable memory is allocated or host functions called automatically. */
const uint8_t *metald_image(const MetaLD *);
size_t metald_image_size(const MetaLD *);
/* Configured base in every state; NULL -> 0. */
uint64_t metald_image_base(const MetaLD *);
/* Read-only queries do not replace the last error. Count is 0 before linking;
 * lookups return BAD_STATE before linking, NOT_FOUND for missing names/indices,
 * INVALID for NULL context/name/output. Output is zeroed on non-INVALID failure. */
size_t metald_symbol_count(const MetaLD *);
int32_t metald_symbol_at(const MetaLD *, size_t index, MetaLDSymbol *out);
int32_t metald_find_symbol(const MetaLD *, const char *name, uint64_t *out);
/* LINKED only. ELF writer uses existing ArchLD W-X layout and atomic publication.
 * Explicit address must be inside executable text; entry-name helper resolves
 * a linked global first. Map writer returns an error on I/O failure. */
int32_t metald_write_elf(MetaLD *, const char *path, uint64_t entry);
int32_t metald_write_executable(MetaLD *, const char *path, const char *entry_name);
int32_t metald_write_map(MetaLD *, const char *path);
#ifdef __cplusplus
}
#endif
#endif

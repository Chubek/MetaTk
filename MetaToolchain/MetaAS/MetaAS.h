/* MetaAS embedding ABI v1 (ISO C11). Link MetaAS::metaas.
 * Handles own their storage; destroy with the corresponding free function.
 * All operations on one assembler require caller exclusion. Distinct assemblers
 * are independent; user callbacks must provide their own synchronization.
 * No exception crosses the C ABI. Callbacks must not reenter/free the assembler.
 */
#ifndef METATOOLCHAIN_METAAS_H
#define METATOOLCHAIN_METAAS_H
#include <stddef.h>
#include <stdint.h>
#include "../ArchAS/ArchASPlugin.h"
#ifdef __cplusplus
extern "C" {
#endif
#define METAAS_ABI_VERSION 1u
typedef struct MetaAS MetaAS;
typedef struct MetaASResult MetaASResult;
enum { METAAS_OK, METAAS_INVALID, METAAS_ERROR, METAAS_IO_ERROR, METAAS_NO_MEMORY };
enum { METAAS_ATT, METAAS_INTEL };
enum { METAAS_AUTO, METAAS_GAS, METAAS_LLVM, METAAS_BUILTIN };
enum { METAAS_OBJECT, METAAS_ASSEMBLY, METAAS_BINARY, METAAS_HEX, METAAS_JSON };
/* Fixed v1 layouts; do not pack. New layouts use a new ABI version. */
typedef struct MetaASError {
    int32_t code;
    size_t line, column;
    char file[512], message[1024]; /* Always NUL-terminated; long text is truncated. */
} MetaASError;
typedef struct MetaASOptions {
    uint32_t abi_version;
    size_t struct_size;
    const char *target, *database, *specification;
    size_t target_index;
    int32_t syntax, backend, format;
    const char *assembler, *triple, *bfd_database;
    size_t max_bytes, max_statements, max_depth, lua_memory, lua_instructions;
} MetaASOptions;

uint32_t metaas_abi_version(void);
/* NULL is harmless. Set target after initialization; other defaults match domas. */
void metaas_options_init(MetaASOptions *options);
/* Copies all options/strings. NULL on failure; optional error is filled even for
 * allocation failure. No handle is published until target loading succeeds. */
MetaAS *metaas_create(const MetaASOptions *options, MetaASError *error);
void metaas_free(MetaAS *assembler); /* NULL-safe. */
/* Borrowed until next operation/free; NULL for NULL assembler. */
const MetaASError *metaas_error(const MetaAS *assembler);

/* Setters copy strings and return METAAS_*. Last error is replaced per operation.
 * A failed compile does not poison the assembler: it may be reused. */
int32_t metaas_add_include(MetaAS *, const char *path);
int32_t metaas_add_lua(MetaAS *, const char *path);
int32_t metaas_add_plugin(MetaAS *, const char *path);
int32_t metaas_add_assembler_argument(MetaAS *, const char *argument);
int32_t metaas_define(MetaAS *, const char *name, int64_t value);
/* Same invocation/emitter ABI as shared-library plugins. Name is copied, user
 * is borrowed through assembler lifetime. Emit source; return 0 on success.
 * Reserved/duplicate names fail. No user destructor is invoked. */
int32_t metaas_register_directive(MetaAS *, const char *name, ArchASDirective callback, void *user);

/* Input is borrowed only during this call; NULL valid only for length 0.
 * file is diagnostic/include context (NULL -> <memory>). An owned result is
 * returned on success; NULL on failure with metaas_error() populated.
 * Each call has a fresh macro table, Lua VM and plugin instances. */
MetaASResult *metaas_compile(MetaAS *, const void *source, size_t length, const char *file);
void metaas_result_free(MetaASResult *); /* NULL-safe. Results outlive assembler. */
/* Borrowed views valid until result_free; byte length excludes a trailing NUL.
 * NULL result/index returns NULL/0. Diagnostics contains native tool warnings. */
const uint8_t *metaas_result_data(const MetaASResult *);
size_t metaas_result_size(const MetaASResult *);
const char *metaas_result_diagnostics(const MetaASResult *);
size_t metaas_result_dependency_count(const MetaASResult *);
const char *metaas_result_dependency(const MetaASResult *, size_t index);
#ifdef __cplusplus
}
#endif
#endif

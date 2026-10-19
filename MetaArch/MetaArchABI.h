/* Versioned, layout-independent C ABI for MetaArch language bindings.
 * C11/C++ compatible; no dependency on the internal Dsym* model headers.
 * All handles are owned: free with their matching function, never free().
 * All returned strings/bytes and view members are borrowed until the owning
 * handle is freed (diagnostic views also expire on diagnostics_clear()).
 * Read-only spec operations may run concurrently. Freeing a handle, loading
 * its backend, or modifying a diagnostics handle requires caller exclusion.
 * Functions never retain caller arrays/strings. NULL free/clear is harmless.
 */
#ifndef METAARCH_ABI_H
#define METAARCH_ABI_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#define METAARCH_ABI_VERSION 1u
typedef struct MaSpec MaSpec;
typedef struct MaDiagnostics MaDiagnostics;
typedef struct MaResult MaResult;
typedef int32_t MaStatus;
typedef int32_t MaFormat;
enum { MA_OK = 0, MA_INVALID_ARGUMENT = 1, MA_NOT_FOUND = 2 };
enum { MA_AUTO = 0, MA_YAML = 1, MA_JSON = 2, MA_XML = 3, MA_SEXPR = 4 };
enum { MA_NOTE = 0, MA_WARNING = 1, MA_ERROR = 2 };
enum { MA_LITTLE_ENDIAN = 0, MA_BIG_ENDIAN = 1 };
enum { MA_LOAD_BACKEND = 1u, MA_LOAD_NO_COMPILE = 2u,
       MA_LOAD_STRICT = 4u, MA_LOAD_LINT = 8u };
enum { MA_WRITE_COMPACT = 1u, MA_WRITE_EMBED_BACKEND = 2u,
       MA_WRITE_REF_BACKEND = 4u, MA_WRITE_NO_HEADER = 8u,
       MA_WRITE_SORT_KEYS = 16u };

/* Version-1 view layouts are fixed. Future additions use new view types and
 * functions, not extra members in these structs. Use sizeof on this platform;
 * do not pack these structures. Strings are UTF-8 / native core diagnostics. */
typedef struct MaDiagnosticView {
    int32_t severity;
    uint32_t line, column;
    const char *file, *path, *message;
} MaDiagnosticView;
typedef struct MaTargetView {
    const char *arch, *subarch, *object_format, *abi;
    int64_t address_width;
    uint32_t byte_order;
} MaTargetView;
typedef struct MaRegisterView {
    const char *name, *reg_class;
    int64_t width, encoding;
} MaRegisterView;
typedef struct MaFormView {
    const char *mnemonic, *encoder;
    size_t operand_count, visible_operand_count;
} MaFormView;
typedef struct MaRelocationView {
    const char *name;
    uint32_t offset, size, bit_offset, bit_width, byte_order, symbol_parameter;
    int64_t symbol_value;
} MaRelocationView;

uint32_t metaarch_abi_version(void);
/* NULL on allocation failure. Diagnostics accumulate across calls until clear. */
MaDiagnostics *metaarch_diagnostics_create(void);
void metaarch_diagnostics_free(MaDiagnostics *d);
void metaarch_diagnostics_clear(MaDiagnostics *d);
size_t metaarch_diagnostics_count(const MaDiagnostics *d);
MaStatus metaarch_diagnostics_get(const MaDiagnostics *d, size_t index, MaDiagnosticView *out);

/* NULL on invalid input, I/O, validation or allocation failure. Diagnostics are
 * optional; supplied handles receive errors/warnings. Paths/names are NUL-ended;
 * parse input is length-delimited. A NULL data pointer is valid only for len=0.
 * Flags must be a combination of the corresponding MA_LOAD_* values. */
MaSpec *metaarch_spec_load(const char *path, MaFormat format, uint32_t flags, MaDiagnostics *d);
MaSpec *metaarch_spec_parse(const void *data, size_t len, MaFormat format,
                          const char *virtual_path, uint32_t flags, MaDiagnostics *d);
void metaarch_spec_free(MaSpec *s);
/* Explicit mutation: 1 success, 0 failure. Load before sharing a spec. */
int32_t metaarch_spec_load_backend(MaSpec *s, MaDiagnostics *d);
size_t metaarch_spec_target_count(const MaSpec *s);
size_t metaarch_spec_register_count(const MaSpec *s);
size_t metaarch_spec_encoder_count(const MaSpec *s);
size_t metaarch_spec_form_count(const MaSpec *s);
MaStatus metaarch_spec_target(const MaSpec *s, size_t index, MaTargetView *out);
MaStatus metaarch_spec_register(const MaSpec *s, size_t index, MaRegisterView *out);
MaStatus metaarch_spec_form(const MaSpec *s, size_t index, MaFormView *out);
/* Borrowed encoder name; NULL for an invalid handle/index. */
const char *metaarch_spec_encoder_name(const MaSpec *s, size_t index);
/* -1 if absent or an invalid argument. */
int32_t metaarch_spec_find_register(const MaSpec *s, const char *name);
int32_t metaarch_spec_find_encoder(const MaSpec *s, const char *name);

/* Owned result or NULL on failure (reported in d, if supplied). Results survive
 * destruction of the spec: relocation names are copied. Byte buffers have an
 * explicit length; serialized text has no counted trailing NUL. */
MaResult *metaarch_spec_write(const MaSpec *s, MaFormat format, uint32_t flags, MaDiagnostics *d);
/* Low-level named encoder: every parameter must be bound. Does not enforce form
 * metadata. Duplicate names are rejected. Arrays may be NULL only for n=0.
 * At most 64 named bindings (the native encoder's current binding limit). */
MaResult *metaarch_spec_encode(const MaSpec *s, const char *encoder, size_t target,
                              const char *const *names, const int64_t *values,
                              size_t n, MaDiagnostics *d);
/* Checked form API: visible operand order, skipping hidden/implicit records.
 * Checks bounds, register enc/class/width and one-to-one relocation mapping.
 * Both indices must exist. See README for the positional mapping contract. */
MaResult *metaarch_spec_encode_form(const MaSpec *s, size_t form, size_t target,
                                   const int64_t *values, size_t n, MaDiagnostics *d);
void metaarch_result_free(MaResult *r);
const uint8_t *metaarch_result_data(const MaResult *r);
size_t metaarch_result_size(const MaResult *r);
size_t metaarch_result_relocation_count(const MaResult *r);
MaStatus metaarch_result_relocation(const MaResult *r, size_t index, MaRelocationView *out);

#ifdef __cplusplus
}
#endif
#endif

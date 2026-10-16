#ifndef METACHAIN_QSCRIPT_CFFI_H
#define METACHAIN_QSCRIPT_CFFI_H
#include "QScript-FFI.h"
#include "QScript-SharedLibrary.h"
#include "QScript-Serialization.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum qs_cffi_type {
    QS_CFFI_VOID=0, QS_CFFI_I8, QS_CFFI_U8, QS_CFFI_I16, QS_CFFI_U16,
    QS_CFFI_I32, QS_CFFI_U32, QS_CFFI_I64, QS_CFFI_U64,
    QS_CFFI_FLOAT, QS_CFFI_DOUBLE, QS_CFFI_POINTER
} qs_cffi_type;
typedef struct qs_cffi_value {
    int type;
    union { int64_t i64; uint64_t u64; double f64; void *pointer; } data;
} qs_cffi_value;
typedef struct qs_cffi_function qs_cffi_function;
int qs_cffi_available(void);
/* Fixed-arity default-platform C ABI, up to 16 arguments; no variadics or
 * by-value structs. Types are copied. The function retains its library. */
qs_cffi_function *qs_cffi_function_create(qs_context *, const qs_shared_library *,
    const char *symbol, int result_type, const int *argument_types, size_t count);
void qs_cffi_function_destroy(qs_cffi_function *);
qs_status qs_cffi_call(qs_context *, const qs_cffi_function *,
    const qs_cffi_value *args, size_t count, qs_cffi_value *result);
/* Register a native function as a QScript function. Integers are range
 * checked; uint64 uses all 64 bits, float/double use IEEE bit patterns,
 * pointers use uintptr_t bits. Void returns zero. Binding retains function. */
qs_status qs_cffi_bind(qs_context *, const char *name, const qs_cffi_function *);
/* Install primitives used by `use cffi;`. Buffers, libraries and functions
 * are context-owned handles, released explicitly or on context destruction.
 * Repeated enable is a no-op. Available only in libffi-enabled builds. */
qs_status qs_cffi_enable(qs_context *);
/* Copy bytes between C storage and context-owned script buffers. Enable cffi
 * first. NULL initial data creates a zero-filled buffer. Handles can be passed
 * to qs_call and used by cffi::read/write; pointers are never serialized.
 * Reads/writes validate the entire range before copying. */
qs_status qs_cffi_buffer_create(qs_context *, const void *data, size_t length, int64_t *handle);
qs_status qs_cffi_buffer_read(qs_context *, int64_t handle, size_t offset, void *out, size_t length);
qs_status qs_cffi_buffer_write(qs_context *, int64_t handle, size_t offset, const void *data, size_t length);
qs_status qs_cffi_release(qs_context *, int64_t handle);
#ifdef __cplusplus
}
#endif
#endif

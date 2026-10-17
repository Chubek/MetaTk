/* Carbine C API: the types shared by the tooling library, the native
 * functions QScript programs call, and C extension modules (Carbine-Module.h).
 *
 * Every native takes and returns 64-bit integers, so QScript never needs
 * strings: text, paths and files stay on the C side, and QScript refers to
 * targets, arrays and modules by index or handle. */
#ifndef CARBINE_API_H
#define CARBINE_API_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CARBINE_API_VERSION 1

typedef enum cb_status {
    CB_OK = 0,
    CB_ERROR = 1,
    CB_INVALID_ARGUMENT = 2,
    CB_NOT_FOUND = 3,
    CB_IO_ERROR = 4,
    CB_UNSUPPORTED = 5
} cb_status;

/* Same shape as qs_host_fn in QScript-FFI.h, so a native can be handed to
 * qs_register_function unchanged. Return CB_OK on success; any other status
 * traps the QScript program that called it. */
typedef int (*cb_native_fn)(const int64_t *args, size_t count, int64_t *result, void *user);

typedef struct cb_native {
    const char *name;   /* QScript name, e.g. "carbine::target_count" */
    size_t arity;
    cb_native_fn fn;
} cb_native;

const char *cb_version(void);
const char *cb_status_text(cb_status status);
/* Diagnostic from the most recent failing call (never NULL, may be ""). */
const char *cb_last_error(void);

#ifdef __cplusplus
}
#endif
#endif

#ifndef METACHAIN_QSCRIPT_FFI_H
#define METACHAIN_QSCRIPT_FFI_H
#include "qscript/QScript.h"
#include "QScript-Runtime.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Register before compiling. Names are QScript identifiers (optionally
 * qualified). Arguments/results are signed 64-bit values. User data must
 * outlive the registration and active calls. Returning non-OK traps execution.
 * Callbacks may call qs_call using a DIFFERENT context. A context is not
 * thread-safe or reentrant. Registration copies the name, not user data.
 * Unregistering invalidates previously compiled calls; IDs are never reused. */
typedef qs_status (*qs_host_fn)(const int64_t *args, size_t count,
                                int64_t *result, void *user);
qs_status qs_register_function(qs_context *, const char *name, size_t arity,
                               qs_host_fn, void *user);
qs_status qs_unregister_function(qs_context *, const char *name);
size_t qs_registered_function_count(const qs_context *);
#ifdef __cplusplus
}
#endif
#endif

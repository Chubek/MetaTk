#ifndef METACHAIN_QSCRIPT_SHARED_LIBRARY_H
#define METACHAIN_QSCRIPT_SHARED_LIBRARY_H
#include "qscript/QScript.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct qs_shared_library qs_shared_library;
/* NULL path opens the current process on POSIX. Eager, local symbol binding.
 * Errors are reported through qs_error. A symbol is valid until close unless
 * retained by a qs_cffi_function or registered binding. */
qs_shared_library *qs_shared_library_open(qs_context *, const char *path);
void qs_shared_library_close(qs_shared_library *);
qs_status qs_shared_library_symbol(qs_context *, const qs_shared_library *,
                                    const char *name, void **address);
#ifdef __cplusplus
}
#endif
#endif

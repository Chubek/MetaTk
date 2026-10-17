#ifndef METACHAIN_QSCRIPT_FFI_H
#define METACHAIN_QSCRIPT_FFI_H
// Host functions: native callbacks that QScript programs call by name.
//
//   static qs_status add(const int64_t *args, size_t count, int64_t *result, void *user) {
//       (void)user; if (count != 2) return QS_INVALID_ARGUMENT;
//       *result = args[0] + args[1]; return QS_OK;
//   }
//   qs_register_function(c, "host::add", 2, add, NULL);
//
// A program then writes `use host;` or calls `host::add(1, 2)`. Names are
// `ident` or `ident::ident...`. Arity is 0..16. The callback runs on the same
// thread as the VM; returning a status other than QS_OK traps the program.
#include "qscript/QScript.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef qs_status (*qs_host_fn)(const int64_t *args, size_t count, int64_t *result, void *user);

qs_status qs_register_function(qs_context *, const char *name, size_t arity, qs_host_fn fn, void *user);
qs_status qs_unregister_function(qs_context *, const char *name);
size_t qs_registered_function_count(const qs_context *);

#ifdef __cplusplus
}
#endif
#endif

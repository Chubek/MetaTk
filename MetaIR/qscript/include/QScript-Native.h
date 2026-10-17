#ifndef METACHAIN_QSCRIPT_NATIVE_H
#define METACHAIN_QSCRIPT_NATIVE_H
/* C ABI between ASMtk-emitted x86-64 code and the statically linked parser
   runtime. No VM, QScript compiler, libffi or QScript shared library is needed
   in the executable. Use MetaChain::QScriptNativeRuntime when linking objects. */
#include "qscript/include/QScript-Generated.h"
#ifdef __cplusplus
extern "C" {
#endif
enum { QS_NATIVE_ABI = 1 };
typedef struct qs_native_unit {
    uint64_t abi;
    qs_runtime_config runtime;
    uint64_t max_source_bytes, max_table_entries;
} qs_native_unit;
typedef int64_t (*qs_native_entry_fn)(void *runtime);
/* Launch owns/frees the runtime; entry returns the script result/status.
   Dispatch receives exactly 32 registers and returns 0, 125, 126 or 127.
   Errors are reported once as "qscript: MESSAGE" on standard error. */
int qs_native_launch(const qs_native_unit *, qs_native_entry_fn);
int qs_native_dispatch(void *runtime, int64_t id, const int64_t *registers, int64_t *result);
#ifdef __cplusplus
}
#endif
#endif

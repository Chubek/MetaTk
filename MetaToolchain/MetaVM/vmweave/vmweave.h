#ifndef VMWEAVE_H
#define VMWEAVE_H
#include "../Machinery/MetaVM.h"
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Bootstrap input is Machinery assembly, never MetaML. Output is owned. */
mvm_status vmwv_compile_assembly(const char *, size_t, mvm_module **, mvm_error *);
/* Emit a verified module as portable C11 with a caller-chosen factory symbol.
 * Factory has signature mvm_status symbol(mvm_module **, mvm_error *).
 * Output stream is caller-owned; write failures return MVM_IO. */
mvm_status vmwv_emit_c(const mvm_module *, const char *symbol, FILE *, mvm_error *);
/* Stage emitted C and commit with mvm_write_file_atomic. Bounded to 128 MiB.
 * Invalid modules/symbols and write failures preserve an existing destination. */
mvm_status vmwv_emit_c_file(const mvm_module *, const char *symbol,
    const char *path, mvm_error *);
#ifdef __cplusplus
}
#endif
#endif

#ifndef METAVM_METAIL_ISEL_H
#define METAVM_METAIL_ISEL_H
#include "AbstractMachine.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Dependency-free scalar operation selection for an external IR producer.
 * Recognizes add/sub/mul/div/mod/and/or/xor/shl/shr/eq/lt/le. This adapter does
 * not parse MetaIL or link its Lua-backed runtime. */
mvm_status mvm_select_integer_operation(const char *name, mvm_opcode *);
#ifdef __cplusplus
}
#endif
#endif

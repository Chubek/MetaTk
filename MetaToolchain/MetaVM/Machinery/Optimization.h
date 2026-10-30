#ifndef METAVM_OPTIMIZATION_H
#define METAVM_OPTIMIZATION_H
#include "Module.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Verified, instruction-count-preserving peephole pass: self-moves, jumps to
 * the next instruction, and adjacent integer constants within a basic block.
 * Keeps branch indices and instruction budgets stable. Trapping operations
 * are never folded away. No mutation on failure. changes may be NULL. */
mvm_status mvm_optimize(mvm_module *, size_t *changes, mvm_error *);
#ifdef __cplusplus
}
#endif
#endif

#pragma once
#include "MetaVM.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Portable example only, not native guest execution. On success *out is owned
 * by caller (mvm_module_destroy); on failure it is NULL. Optional error is
 * caller-owned. No shared mutable state. See docs/VMS.md for native coverage. */
mvm_status legacy_LuaVM_example(mvm_module **,mvm_error *);
#ifdef __cplusplus
}
#endif

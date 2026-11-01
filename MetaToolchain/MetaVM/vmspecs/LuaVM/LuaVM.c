#include "LuaVM.h"
#include "../../VMLibrary/LuaVM/LuaVM.h"
mvm_status legacy_LuaVM_example(mvm_module **out,mvm_error *error) { return LuaVM_example_module(out,error); }

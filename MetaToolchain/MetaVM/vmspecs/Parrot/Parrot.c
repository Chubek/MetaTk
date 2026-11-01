#include "Parrot.h"
#include "../../VMLibrary/ParrotVM/ParrotVM.h"
mvm_status legacy_Parrot_example(mvm_module **out,mvm_error *error) { return ParrotVM_example_module(out,error); }

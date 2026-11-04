#include "LuaJIT.h"
#include <stdio.h>
mvm_status legacy_LuaJIT_example(mvm_module **out,mvm_error *error) { mvm_error_clear(error);
 if(out)*out=0;
 mvm_status status=out?MVM_NOT_FOUND:MVM_INVALID;
 if(error){error->status=status;snprintf(error->message,sizeof(error->message),"LuaJIT: no machine specification or catalog mapping supplied");}
 return status; }

#include "MetaVM.h"
extern mvm_status installed_factory(mvm_module **,mvm_error *);
int main(void) {
    mvm_module *module=NULL;mvm_vm *vm=NULL;mvm_error error;
    mvm_value result=mvm_nil();int failed=1;
    static const char source[]=".function main 0 1\nconst r0 42\nret r0\n.end\n";
    if(vmwv_compile_assembly(source,sizeof(source)-1,&module,&error)!=MVM_OK)return 1;
    mvm_module_destroy(module);module=NULL;
    if(installed_factory(&module,&error)!=MVM_OK)return 1;
    vm=mvm_create(NULL,&error);
    if(vm&&mvm_run_bytecode(vm,module,0,NULL,0,&result,&error)==MVM_OK)
        failed=result.kind!=MVM_INT||result.as.integer!=42;
    mvm_destroy(vm);mvm_module_destroy(module);return failed;
}

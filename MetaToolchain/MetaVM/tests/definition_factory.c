#include "MetaVM.h"
#include <stdio.h>
extern mvm_status toy_r16_factory(mvm_module **,mvm_error *);
#define REQUIRE(x) do { if(!(x)) {fprintf(stderr,"factory test failed at line %d\n",__LINE__); failed=1;goto cleanup;} }while(0)
int main(void) {
    mvm_module *module=NULL;mvm_vm *vm=NULL;mvm_root *rr_root=NULL,*mem_root=NULL;
    mvm_value rr=mvm_nil(),mem=mvm_nil(),args[2],result=mvm_nil(),pixel=mvm_nil();
    mvm_error error;size_t i;int failed=0,engine;
    REQUIRE(toy_r16_factory(&module,&error)==MVM_OK);
    vm=mvm_create(NULL,&error);REQUIRE(vm);
    REQUIRE(mvm_array_new(vm,16,&rr)==MVM_OK);REQUIRE(mvm_pin(vm,rr,&rr_root)==MVM_OK);
    REQUIRE(mvm_array_new(vm,256,&mem)==MVM_OK);REQUIRE(mvm_pin(vm,mem,&mem_root)==MVM_OK);
    for(i=0;i<16;++i)REQUIRE(mvm_array_set(vm,rr,i,mvm_int(0))==MVM_OK);
    for(i=0;i<256;++i)REQUIRE(mvm_array_set(vm,mem,i,mvm_int(0))==MVM_OK);
    args[0]=rr;args[1]=mem;
    for(engine=MVM_ENGINE_BYTECODE;engine<=MVM_ENGINE_XVM;++engine) {
        REQUIRE(mvm_run(vm,module,0,args,2,(mvm_engine)engine,&result,&error)==MVM_OK);
        REQUIRE(result.kind==MVM_INT&&result.as.integer==42);
        REQUIRE(mvm_array_get(vm,mem,3,&pixel)==MVM_OK);
        REQUIRE(pixel.kind==MVM_INT&&pixel.as.integer==42);
    }
cleanup:
    mvm_unpin(mem_root);mvm_unpin(rr_root);mvm_destroy(vm);mvm_module_destroy(module);
    return failed;
}

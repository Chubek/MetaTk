#include "vmweave.h"
#include <string.h>
int main(int argc,char**argv) {
    mvm_module *module=NULL; mvm_error e; mvm_status s;
    if(argc<4 || (strcmp(argv[1],"compile") && strcmp(argv[1],"emit-c")) ||
       (!strcmp(argv[1],"emit-c")?argc!=5:argc!=4)) {
        fputs("usage: vmweave compile INPUT.mvm OUTPUT.mvmb\n       vmweave emit-c INPUT.mvm OUTPUT.c SYMBOL\nMetaML: use vmweave-metaml for .vmweave sources\n",stderr); return 2;
    }
    s=mvm_load_file(argv[2],0,&module,&e);
    if(s==MVM_OK) {
        if(!strcmp(argv[1],"compile")) s=mvm_save_file_atomic(argv[3],module,&e);
        else s=vmwv_emit_c_file(module,argv[4],argv[3],&e);
    }
    if(s!=MVM_OK) fprintf(stderr,"%s:%zu: %s (%s)\n",argv[2],e.line,e.message,mvm_status_string(s));
    mvm_module_destroy(module); return s==MVM_OK?0:1;
}

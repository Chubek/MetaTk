/* Generated portable Machinery example. Regenerate with tools/generate_catalog.py. */
#include "Monty.h"
static const char source[] = ".function main 0 3\nconst r0 6\nconst r1 7\nmul r2 r0 r1\nret r2\n.end\n";
mvm_status Monty_example_module(mvm_module **out,mvm_error *error) {
 return mvm_compile(source,sizeof(source)-1,0,out,error);
}

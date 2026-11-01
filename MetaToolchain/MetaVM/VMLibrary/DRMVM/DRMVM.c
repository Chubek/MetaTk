/* Generated portable Machinery example. Regenerate with tools/generate_catalog.py. */
#include "DRMVM.h"
static const char source[] = ".import drm_clear 1\n.import drm_rect 5\n.function main 0 7\nconst r0 0\nhost r6 drm_clear r0\nconst r0 1\nconst r1 1\nconst r2 2\nconst r3 2\nconst r4 16711680\nhost r6 drm_rect r0\nconst r6 42\nret r6\n.end\n";
mvm_status DRMVM_example_module(mvm_module **out,mvm_error *error) {
 return mvm_compile(source,sizeof(source)-1,0,out,error);
}

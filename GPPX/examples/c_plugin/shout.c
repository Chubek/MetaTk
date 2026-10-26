#include <GPPX.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
static int shout(GPPX_State *state,const char *text,char **out,void *data){
 (void)state;(void)data;size_t n=strlen(text);*out=(char*)malloc(n+2);if(!*out)return 1;
 for(size_t i=0;i<n;++i)(*out)[i]=(char)toupper((unsigned char)text[i]);
 (*out)[n]='!';(*out)[n+1]='\0';return 0;
}
#if defined(_WIN32)
__declspec(dllexport)
#endif
int gppx_plugin_init(const GPPX_HostAPI* host,GPPX_State* state){
 if(!host||host->abi_version!=GPPX_ABI_VERSION)return 1;
 return host->register_directive(state,"shout",shout,NULL);
}

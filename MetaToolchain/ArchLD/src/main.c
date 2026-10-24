/* ArchLD executable driver. Only supported linker flags are accepted.
 * Unknown flags fail closed rather than silently generating bad executables. */
#define _GNU_SOURCE
#include "ArchLD.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <unistd.h>

static void usage(FILE *f){
 fputs("ArchLD - ArchBFD-based static ELF32/ELF64 linker\n"
 "usage: ArchLD [options] input.o [libfoo.a ...]\n"
 "  -o FILE, --output FILE         executable output (default a.out)\n"
 "  -e SYM, --entry SYM           entry point (default _start)\n"
 "  --catalogue FILE              ArchBFD JSON catalogue\n"
 "  --target ELF64                ArchBFD target: ELF64 or ELF32\n"
 "  --base ADDR                   first .text address (target default)\n"
 "  -Map=FILE, --map FILE         human-readable link map\n"
 "  -L DIR, -LDIR                 static archive search directory\n"
 "  -l NAME, -lNAME               search libNAME.a (no shared libs)\n"
 "  --version, --help             version or help\n"
 "GNU/BSD ar archives, strong/weak/common symbols, checked static\n"
 "relocations, page-separated LOAD segments, ELF symbol/section tables.\n",f);
}
static bool parse_u64(const char*s,uint64_t*out){
 if(!s||!*s||*s=='-')return false;errno=0;char*end;unsigned long long x=strtoull(s,&end,0);
 if(errno||*end)return false;*out=x;return true;
}
static char *ld_copy(const char*s){size_t n=strlen(s)+1;char*p=malloc(n);if(p)memcpy(p,s,n);return p;}
typedef struct {char **items;size_t n,cap;} Strings;
static bool push(Strings*v,const char*s){if(v->n==v->cap){size_t n=v->cap?v->cap*2:16;char**p=realloc(v->items,n*sizeof(char*));if(!p)return false;v->items=p;v->cap=n;}v->items[v->n]=ld_copy(s);if(!v->items[v->n])return false;v->n++;return true;}
static void clear(Strings*v){for(size_t i=0;i<v->n;i++)free(v->items[i]);free(v->items);}
static bool add_lib(Strings*in,Strings*dirs,const char*name){
 for(size_t i=0;i<dirs->n;i++){
  size_t len=strlen(dirs->items[i])+strlen(name)+9;
  char*p=malloc(len);if(!p)return false;
  snprintf(p,len,"%s/lib%s.a",dirs->items[i],name);
  if(access(p,R_OK)==0){bool ok=push(in,p);free(p);return ok;}
  free(p);
 }
 fprintf(stderr,"ArchLD: cannot find static archive -l%s (supply -L directory)\n",name);
 return false;
}
int main(int argc,char**argv){
 const char *out="a.out",*entry="_start",*map=NULL,*cat=NULL,*target="ELF64";
 uint64_t base=0x401000;bool has_base=false;
 Strings inputs={0},dirs={0};
 int rc=2;
 bool endopts=false;
 /* Resolve catalogue relative to this executable, not the caller's CWD. */
 char defaultcat[PATH_MAX];
 const char*slash=strrchr(argv[0],'/');
 if(slash){size_t n=(size_t)(slash-argv[0]);if(n>sizeof defaultcat-45)n=sizeof defaultcat-45;memcpy(defaultcat,argv[0],n);defaultcat[n]=0;strcat(defaultcat,"/../targets/ArchBFDCatalogue.json");}
 else snprintf(defaultcat,sizeof defaultcat,"targets/ArchBFDCatalogue.json");
 cat=defaultcat;
 if(!push(&dirs,"."))goto done;
 for(int i=1;i<argc;i++){
  const char*a=argv[i],*v=NULL;
  if(!endopts&&!strcmp(a,"--")){endopts=true;continue;}
  if(!endopts&&(!strcmp(a,"--help")||!strcmp(a,"-h"))){usage(stdout);rc=0;goto done;}
  if(!endopts&&!strcmp(a,"--version")){puts("ArchLD 0.3 - static ELF64 x86-64 + ELF32 i386 backends / ArchBFD descriptions");rc=0;goto done;}
  if(!endopts&&(!strcmp(a,"-static")||!strcmp(a,"--static")||!strcmp(a,"--start-group")||!strcmp(a,"--end-group")))continue;
  if(!endopts&&(!strcmp(a,"-o")||!strcmp(a,"--output")||!strcmp(a,"-e")||!strcmp(a,"--entry")||!strcmp(a,"--catalogue")||!strcmp(a,"--target")||!strcmp(a,"--base")||!strcmp(a,"--map")||!strcmp(a,"-L")||!strcmp(a,"-l"))){
   if(++i>=argc){fprintf(stderr,"ArchLD: missing value for %s\n",a);goto done;}v=argv[i];
  }
  if(!endopts&&(!strcmp(a,"-o")||!strcmp(a,"--output"))){out=v;continue;}
  if(!endopts&&(!strcmp(a,"-e")||!strcmp(a,"--entry"))){entry=v;continue;}
  if(!endopts&&!strcmp(a,"--catalogue")){cat=v;continue;}
  if(!endopts&&!strcmp(a,"--target")){target=v;continue;}
  if(!endopts&&!strcmp(a,"--base")){if(!parse_u64(v,&base)){fprintf(stderr,"ArchLD: invalid base\n");goto done;}has_base=true;continue;}
  if(!endopts&&!strcmp(a,"--map")){map=v;continue;}
  if(!endopts&&!strncmp(a,"-Map=",5)){map=a+5;continue;}
  if(!endopts&&!strcmp(a,"-L")){if(!push(&dirs,v))goto done;continue;}
  if(!endopts&&!strncmp(a,"-L",2)&&a[2]){if(!push(&dirs,a+2))goto done;continue;}
  if(!endopts&&!strcmp(a,"-l")){if(!add_lib(&inputs,&dirs,v))goto done;continue;}
  if(!endopts&&!strncmp(a,"-l",2)&&a[2]){if(!add_lib(&inputs,&dirs,a+2))goto done;continue;}
  if(!endopts&&a[0]=='-'){fprintf(stderr,"ArchLD: unsupported option: %s\n",a);goto done;}
  if(!push(&inputs,a))goto done;
 }
 if(!inputs.n){usage(stderr);goto done;}
 if(!has_base&&strcmp(target,"ELF32")==0)base=0x08049000;
 ArchLD*ld=archld_new(cat,target,base);
 if(!ld){fprintf(stderr,"ArchLD: out of memory\n");rc=1;goto done;}
 bool ok=!archld_error(ld)[0];
 for(size_t i=0;i<inputs.n&&ok;i++)ok=archld_add(ld,inputs.items[i]);
 if(ok)ok=archld_link(ld);
 uint64_t e=0;
 if(ok&&!archld_symbol(ld,entry,&e)){
  fprintf(stderr,"ArchLD: entry point '%s' undefined\n",entry);ok=false;
 }
 if(ok)ok=archld_write_elf(ld,out,e);
 if(ok&&map&&!archld_write_map_file(ld,map)){fprintf(stderr,"ArchLD: unable to write map '%s'\n",map);ok=false;}
 if(!ok&&archld_error(ld)[0])fprintf(stderr,"ArchLD: %s\n",archld_error(ld));
 if(ok)fprintf(stdout,"ArchLD: linked %s (entry %s = 0x%"PRIx64", %zu bytes in memory)\n",out,entry,e,archld_image_size(ld));
 archld_free(ld);
 rc=ok?0:1;
 done:clear(&inputs);clear(&dirs);return rc;
}

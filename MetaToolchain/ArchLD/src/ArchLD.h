/*2:*/
#line 37 "src/ArchLD.w"

#ifndef ARCHTOOLCHAIN_ARCHLD_H
#define ARCHTOOLCHAIN_ARCHLD_H
#include <stdbool.h> 
#include <stddef.h> 
#include <stdint.h> 
#include <stdio.h> 
#ifdef __cplusplus
extern"C"{
#endif
#line 47 "src/ArchLD.w"
 typedef struct ArchLD ArchLD;




ArchLD*archld_new(const char*catalogue,const char*target,uint64_t base);
bool archld_add(ArchLD*ld,const char*object_path);


bool archld_add_memory(ArchLD*ld,const char*name,const void*data,size_t size);




typedef int(*ArchLDResolveSymbol)(void*user,const char*name,uint64_t*address,
char*message,size_t capacity);
bool archld_define_symbol(ArchLD*ld,const char*name,uint64_t address);
bool archld_set_symbol_resolver(ArchLD*ld,ArchLDResolveSymbol resolver,void*user);
bool archld_link(ArchLD*ld);
bool archld_write_elf(ArchLD*ld,const char*path,uint64_t entry);
bool archld_write_map_file(const ArchLD*ld,const char*path);
size_t archld_global_count(const ArchLD*ld);
bool archld_global_at(const ArchLD*ld,size_t index,const char**name,uint64_t*address,uint64_t*size);

const uint8_t*archld_image(const ArchLD*ld);
size_t archld_image_size(const ArchLD*ld);
uint64_t archld_image_base(const ArchLD*ld);
bool archld_symbol(const ArchLD*ld,const char*name,uint64_t*address);
void archld_map(const ArchLD*ld,FILE*stream);
const char*archld_error(const ArchLD*ld);
void archld_free(ArchLD*ld);
#ifdef __cplusplus
}
#endif
#line 81 "src/ArchLD.w"
#endif
#line 82 "src/ArchLD.w"



/*:2*/

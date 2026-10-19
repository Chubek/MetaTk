#include "MetaAS.h"
#include <stdio.h>
#include <string.h>
#define CHECK(c) do { if (!(c)) { fprintf(stderr,"line %d: %s\n",__LINE__,mas_error(context)); return 1; } } while (0)
int main(int argc,char **argv) {
    mas_context *context=mas_create();
    mas_image *image=NULL;
    const unsigned char *bytes=NULL;
    const char *text=NULL,*name=NULL;
    size_t size=0;
    uint64_t address=0;
    CHECK(argc==2 && context);
    CHECK(mas_assemble(context,"ret",3,NULL,&image)==MAS_ERROR && !image);
    CHECK(mas_load(context,argv[1])==MAS_OK);
    CHECK(mas_assemble(context,"mov r1, -1",10,"c.asm",&image)==MAS_OK);
    CHECK(mas_image_section_count(image)==1);
    CHECK(mas_image_section(image,0,&name,&bytes,&size,&address)==MAS_OK);
    CHECK(!strcmp(name,".text") && size==2 && address==0 && bytes[0]==0x11 && bytes[1]==0xff);
    CHECK(mas_image_json(context,image,&text,&size)==MAS_OK && strstr(text,"MetaAS"));
    CHECK(mas_image_binary(context,image,&bytes,&size)==MAS_OK && size==2);
    CHECK(mas_assemble(context,NULL,1,NULL,NULL)==MAS_INVALID_ARGUMENT);
    mas_destroy(context);
    context=mas_create();
    CHECK(context && mas_image_binary(context,image,&bytes,&size)==MAS_OK);
    mas_image_destroy(image);
    CHECK(mas_load(context,argv[1])==MAS_OK);
    CHECK(mas_assemble(context,NULL,0,NULL,&image)==MAS_OK);
    CHECK(mas_image_binary(context,image,&bytes,&size)==MAS_OK && size==0);
    mas_image_destroy(image);
    CHECK(mas_set_limits(context,1,1)==MAS_OK);
    CHECK(mas_assemble(context,"ret",3,NULL,&image)==MAS_ERROR && !image);
    mas_destroy(context);
    mas_destroy(NULL); mas_image_destroy(NULL);
    return 0;
}

#include "Driver.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void qs_driver_output(int64_t value,void *user) {
    FILE *stream=user?(FILE *)user:stdout;
    fprintf(stream,"%" PRId64 "\n",value);
}
static char *read_source(FILE *stream,size_t *length) {
    size_t capacity=4096;
    char *buffer=malloc(capacity);
    *length=0;
    if (!buffer) return NULL;
    for (;;) {
        size_t got=fread(buffer+*length,1,capacity-*length,stream);
        *length+=got;
        if (ferror(stream)) { free(buffer); return NULL; }
        if (feof(stream)) return buffer;
        if (*length==capacity) {
            char *grown;
            if (capacity>=64u*1024u*1024u) { free(buffer); return NULL; }
            capacity*=2;
            grown=realloc(buffer,capacity);
            if (!grown) { free(buffer); return NULL; }
            buffer=grown;
        }
    }
}
int qs_driver_main(int argc,char **argv) {
    qs_context *context;
    qs_program *program;
    char *source;
    size_t length;
    FILE *input,*output=stdout;
    qs_format format=QS_IL;
    int status=0,run=0,check=0;
    if (argc==2&&strcmp(argv[1],"--help")==0) {
        puts("Usage: qscript --repl\n       qscript run|check|ir|encode|native|object|build FILE [-o OUTPUT]\nUse - for standard input. Binary formats require -o."); return 0;
    }
    context=qs_create(); if (!context) { fputs("qscript: out of memory\n",stderr); return 1; }
    qs_set_output(context,qs_driver_output,stdout);
    if (argc==2&&strcmp(argv[1],"--repl")==0) { status=qs_repl(context); qs_destroy(context); return status; }
    if (argc!=3&&!(argc==5&&strcmp(argv[3],"-o")==0)) { fputs("qscript: use --help for usage\n",stderr); qs_destroy(context); return 2; }
    if (!strcmp(argv[1],"run")) run=1;
    else if (!strcmp(argv[1],"check")) check=1;
    else if (!strcmp(argv[1],"ir")) format=QS_IL;
    else if (!strcmp(argv[1],"encode")) format=QS_BYTECODE;
    else if (!strcmp(argv[1],"native")) format=QS_NATIVE;
    else if (!strcmp(argv[1],"object")) format=QS_OBJECT;
    else if (!strcmp(argv[1],"build")) format=QS_EXECUTABLE;
    else { fprintf(stderr,"qscript: unknown command: %s\n",argv[1]); qs_destroy(context); return 2; }
    if ((!run&&!check&&format!=QS_IL&&format!=QS_NATIVE&&argc!=5)||((run||check)&&argc==5)) {
        fputs("qscript: binary output requires -o; run/check do not accept -o\n",stderr); qs_destroy(context); return 2;
    }
    input=strcmp(argv[2],"-")?fopen(argv[2],"rb"):stdin;
    if (!input) { perror(argv[2]); qs_destroy(context); return 1; }
    source=read_source(input,&length);
    if (input!=stdin) fclose(input);
    if (!source) { fputs("qscript: input read failed or size limit exceeded\n",stderr); qs_destroy(context); return 1; }
    program=qs_compile(context,source,length); free(source);
    if (!program) { fprintf(stderr,"%s\n",qs_error(context)); qs_destroy(context); return 1; }
    if (run) {
        int64_t result;
        if (qs_run(context,program,&result)!=QS_OK) status=1;
        else printf("result %" PRId64 "\n",result);
    } else if (check) puts("ok");
    else {
        const unsigned char *data; size_t size;
        if (qs_emit(context,program,format,&data,&size)!=QS_OK) status=1;
        else {
            if (argc==5) output=fopen(argv[4],"wb");
            if (!output) { perror(argv[4]); status=1; }
            else {
                if (fwrite(data,1,size,output)!=size) { fputs("qscript: output write failed\n",stderr); status=1; }
                if (output!=stdout) { if (fclose(output)) status=1; } else if (fflush(output)) status=1;
            }
        }
    }
    if (status&&*qs_error(context)) fprintf(stderr,"%s\n",qs_error(context));
    qs_program_destroy(program); qs_destroy(context); return status;
}

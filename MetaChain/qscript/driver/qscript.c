#include "Driver.h"
#include "qscript/include/QScript-Backend.h"
#include "qscript/include/QScript-Diagnostics.h"
#include "qscript/include/QScript-Grammar.h"
#include "qscript/include/QScript-Library.h"
#include "qscript/include/QScript-Module.h"
#include "qscript/include/QScript-Runtime.h"
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void qs_driver_output(int64_t value,void *user) {
    FILE *stream=user?(FILE *)user:stdout;
    fprintf(stream,"%" PRId64 "\n",value);
}
char *qs_driver_read(FILE *stream,size_t *length) {
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
static char *read_path(const char *path,size_t *length) {
    FILE *input=strcmp(path,"-")?fopen(path,"rb"):stdin;
    char *text;
    if (!input) { perror(path); return NULL; }
    text=qs_driver_read(input,length);
    if (input!=stdin) fclose(input);
    if (!text) fprintf(stderr,"qscript: %s: read failed or size limit exceeded\n",path);
    return text;
}
static void usage(void) {
    puts("Usage: qscript [OPTIONS] COMMAND ...\n"
         "\n"
         "Commands:\n"
         "  run FILE...              compile and run; prints output and `result N`\n"
         "  check FILE...            parse and check only\n"
         "  ir|native FILE...        print MetaIL / x86-64 assembly\n"
         "  encode|object|build FILE... -o OUT   write bytecode / ELF object / executable\n"
         "  call FILE... -- FUNC [ARG...]        call one function and print its result\n"
         "  report FILE...           backend report (blocks, critical paths, makespan)\n"
         "  ast FILE                 print the QScript AST\n"
         "  tree FILE                print the grammar match tree\n"
         "  grammar                  print the QScript grammar (Raku-style)\n"
         "  parse GRAMMAR INPUT [RULE]   parse INPUT with a grammar file, print the match tree\n"
         "  libs                     list the embedded standard library\n"
         "  lib NAME                 print a standard library source\n"
         "  --repl                   interactive session\n"
         "\n"
         "Options (before the command):\n"
         "  -L DIR        add a library directory for `use` (repeatable)\n"
         "  --no-stdlib   do not resolve `use` against the embedded standard library\n"
         "  --jit         run with MetaJIT instead of the interpreter\n"
         "  --stats       after run, print steps to standard error\n"
         "  --sched NAME  scheduler: list (default), cycle or static\n"
         "  -O0           disable MetaIL optimization\n"
         "  --steps N     execution step limit\n"
         "\n"
         "Several FILEs compile jointly as one module (earlier files are a prelude\n"
         "for later ones). Use - for standard input (alone).");
}
/* Call only after the producing call has returned (arguments are evaluated
   in unspecified order, so never pass the producer and its outputs together). */
static int print_text(qs_context *context,qs_status status,const char *text,size_t length) {
    if (status!=QS_OK) { fprintf(stderr,"%s\n",qs_error(context)); return 1; }
    fwrite(text,1,length,stdout);
    if (!length||text[length-1]!='\n') putchar('\n');
    return 0;
}
static int single_source(qs_context *context,const char *command,const char *path) {
    size_t length,text_length=0; const char *text=NULL; int status;
    char *source=read_path(path,&length);
    if (!source) return 1;
    qs_status produced=!strcmp(command,"ast")?qs_ast_dump(context,source,length,&text,&text_length)
                                             :qs_match_tree_dump(context,source,length,&text,&text_length);
    status=print_text(context,produced,text,text_length);
    free(source);
    return status;
}
static int parse_with_grammar(qs_context *context,int argc,char **argv) {
    size_t glength,ilength,text_length; char *gsource,*input; qs_grammar *grammar; qs_match *match; const char *text;
    int status=1;
    if (argc<2||argc>3) { fputs("qscript: parse GRAMMAR INPUT [RULE]\n",stderr); return 2; }
    gsource=read_path(argv[0],&glength);
    if (!gsource) return 1;
    input=read_path(argv[1],&ilength);
    if (!input) { free(gsource); return 1; }
    grammar=qs_grammar_compile(context,gsource,glength);
    if (!grammar) fprintf(stderr,"%s: %s\n",argv[0],qs_error(context));
    else {
        match=qs_grammar_parse(context,grammar,input,ilength,argc==3?argv[2]:NULL);
        if (!match) fprintf(stderr,"%s: %s\n",argv[1],qs_error(context));
        else {
            qs_status produced=qs_match_dump(context,match,&text,&text_length);
            status=print_text(context,produced,text,text_length);
            qs_match_destroy(match);
        }
        qs_grammar_destroy(grammar);
    }
    free(gsource); free(input);
    return status;
}
int qs_driver_main(int argc,char **argv) {
    qs_context *context;
    qs_module *module;
    qs_program *program;
    FILE *output=stdout;
    qs_format format=QS_IL;
    int status=0,run=0,check=0,report=0,call=0,stats=0,inputs=0,first=1,k;
    const char *outfile=NULL,*command;
    int call_at=-1;
    if (argc==2&&(!strcmp(argv[1],"--help")||!strcmp(argv[1],"-h"))) { usage(); return 0; }
    context=qs_create(); if (!context) { fputs("qscript: out of memory\n",stderr); return 1; }
    qs_set_output(context,qs_driver_output,stdout);
    /* Options. */
    while (first<argc&&argv[first][0]=='-'&&strcmp(argv[first],"-")&&strcmp(argv[first],"--repl")) {
        const char *o=argv[first];
        if (!strcmp(o,"-L")&&first+1<argc) { qs_add_library_path(context,argv[first+1]); first+=2; }
        else if (!strncmp(o,"-L",2)&&o[2]) { qs_add_library_path(context,o+2); ++first; }
        else if (!strcmp(o,"--no-stdlib")) { qs_set_stdlib(context,0); ++first; }
        else if (!strcmp(o,"--jit")) { qs_set_engine(context,QS_ENGINE_JIT); ++first; }
        else if (!strcmp(o,"--stats")) { stats=1; ++first; }
        else if (!strcmp(o,"-O0")) { qs_set_optimize(context,0); ++first; }
        else if (!strcmp(o,"--sched")&&first+1<argc) {
            if (qs_set_scheduler_name(context,argv[first+1])!=QS_OK) { fprintf(stderr,"qscript: unknown scheduler: %s\n",argv[first+1]); qs_destroy(context); return 2; }
            first+=2;
        } else if (!strcmp(o,"--steps")&&first+1<argc) {
            char *end; unsigned long long n; errno=0; n=strtoull(argv[first+1],&end,10);
            if (errno||*end||!n) { fprintf(stderr,"qscript: invalid step limit: %s\n",argv[first+1]); qs_destroy(context); return 2; }
            qs_set_step_limit(context,(size_t)n); first+=2;
        } else { fprintf(stderr,"qscript: unknown option: %s\n",o); qs_destroy(context); return 2; }
    }
    if (first<argc&&!strcmp(argv[first],"--repl")&&first+1==argc) { status=qs_repl(context); qs_destroy(context); return status; }
    if (first>=argc) { fputs("qscript: use --help for usage\n",stderr); qs_destroy(context); return 2; }
    command=argv[first];
    /* Commands that do not compile a program. */
    if (!strcmp(command,"grammar")) { fputs(qs_grammar_qscript_source(),stdout); qs_destroy(context); return 0; }
    if (!strcmp(command,"libs")) {
        size_t n=qs_stdlib_count(),i;
        for (i=0;i<n;++i) puts(qs_stdlib_name(i));
        qs_destroy(context); return 0;
    }
    if (!strcmp(command,"lib")) {
        size_t length; const char *text=first+1<argc?qs_stdlib_source(argv[first+1],&length):NULL;
        if (!text) { fprintf(stderr,"qscript: no such library: %s\n",first+1<argc?argv[first+1]:"(none)"); qs_destroy(context); return 1; }
        fwrite(text,1,length,stdout); qs_destroy(context); return 0;
    }
    if (!strcmp(command,"ast")||!strcmp(command,"tree")) {
        if (first+2!=argc) { fprintf(stderr,"qscript: %s takes one FILE\n",command); qs_destroy(context); return 2; }
        status=single_source(context,command,argv[first+1]); qs_destroy(context); return status;
    }
    if (!strcmp(command,"parse")) { status=parse_with_grammar(context,argc-first-1,argv+first+1); qs_destroy(context); return status; }
    if (argc-first<2) { fputs("qscript: use --help for usage\n",stderr); qs_destroy(context); return 2; }
    if (!strcmp(command,"run")) run=1;
    else if (!strcmp(command,"check")) check=1;
    else if (!strcmp(command,"report")) report=1;
    else if (!strcmp(command,"call")) call=1;
    else if (!strcmp(command,"ir")) format=QS_IL;
    else if (!strcmp(command,"encode")) format=QS_BYTECODE;
    else if (!strcmp(command,"native")) format=QS_NATIVE;
    else if (!strcmp(command,"object")) format=QS_OBJECT;
    else if (!strcmp(command,"build")) format=QS_EXECUTABLE;
    else { fprintf(stderr,"qscript: unknown command: %s\n",command); qs_destroy(context); return 2; }
    inputs=argc-first-1;
    if (call) {
        for (k=first+1;k<argc;++k) if (!strcmp(argv[k],"--")) { call_at=k; break; }
        if (call_at<0||call_at+1>=argc) { fputs("qscript: call FILE... -- FUNC [ARG...]\n",stderr); qs_destroy(context); return 2; }
        inputs=call_at-first-1;
    } else if (argc-first>=4&&strcmp(argv[argc-2],"-o")==0) { outfile=argv[argc-1]; inputs-=2; }
    if (inputs<1) { fputs("qscript: no input files\n",stderr); qs_destroy(context); return 2; }
    if ((!run&&!check&&!report&&!call&&format!=QS_IL&&format!=QS_NATIVE&&!outfile)||((run||check||report||call)&&outfile)) {
        fputs("qscript: binary output requires -o; run/check/report/call do not accept -o\n",stderr); qs_destroy(context); return 2;
    }
    for (k=0;k<inputs;++k) if (!strcmp(argv[first+1+k],"-")&&inputs>1) {
        fputs("qscript: - (stdin) cannot be combined with other inputs\n",stderr); qs_destroy(context); return 2;
    }
    module=qs_module_create(); if (!module) { fputs("qscript: out of memory\n",stderr); qs_destroy(context); return 1; }
    for (k=0;k<inputs;++k) {
        const char *path=argv[first+1+k];
        size_t length; char *source=read_path(path,&length);
        if (!source) { qs_module_destroy(module); qs_destroy(context); return 1; }
        if (qs_module_add_source(module,source,length,strcmp(path,"-")?path:"<stdin>")!=QS_OK) {
            fputs("qscript: out of memory\n",stderr); free(source); qs_module_destroy(module); qs_destroy(context); return 1;
        }
        free(source);
    }
    if (qs_module_compile(context,module,&program)!=QS_OK) {
        fprintf(stderr,"%s\n",qs_error(context)); qs_module_destroy(module); qs_destroy(context); return 1;
    }
    qs_module_destroy(module);
    if (run) {
        qs_run_info info;
        if (qs_run_ex(context,program,&info)!=QS_OK) status=1;
        else printf("result %" PRId64 "\n",info.result);
        if (stats) fprintf(stderr,"steps %zu, outputs %zu, instructions %zu, engine %s\n",info.steps,info.outputs,
            qs_program_size(program),qs_get_engine(context)==QS_ENGINE_JIT?"jit":"interpreter");
    } else if (check) puts("ok");
    else if (call) {
        int64_t args[64],result; size_t count=0;
        for (k=call_at+2;k<argc;++k) {
            char *end; long long v; errno=0; v=strtoll(argv[k],&end,0);
            if (errno||*end||count==64) { fprintf(stderr,"qscript: invalid argument: %s\n",argv[k]); status=2; break; }
            args[count++]=(int64_t)v;
        }
        if (!status) {
            if (qs_call(context,program,argv[call_at+1],args,count,&result)!=QS_OK) status=1;
            else printf("%" PRId64 "\n",result);
        }
    } else if (report) {
        const char *text; size_t length;
        qs_status produced=qs_backend_report(context,program,&text,&length);
        status=print_text(context,produced,text,length);
    } else {
        const unsigned char *data; size_t size;
        if (qs_emit(context,program,format,&data,&size)!=QS_OK) status=1;
        else {
            if (outfile) output=fopen(outfile,"wb");
            if (!output) { perror(outfile?outfile:"stdout"); status=1; }
            else {
                if (fwrite(data,1,size,output)!=size) { fputs("qscript: output write failed\n",stderr); status=1; }
                if (output!=stdout) { if (fclose(output)) status=1; } else if (fflush(output)) status=1;
            }
        }
    }
    if (status==1&&*qs_error(context)) fprintf(stderr,"%s\n",qs_error(context));
    qs_program_destroy(program); qs_destroy(context); return status;
}

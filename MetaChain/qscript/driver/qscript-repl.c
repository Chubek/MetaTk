#include "Driver.h"
#include "qscript/include/QScript-Library.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Declarations (fn, sub, const, use) entered at the prompt are remembered
   and prepended to every later line; statements run as a fresh program each
   time, so variables do not persist between lines. */
static int is_declaration(const char *line) {
    while (*line==' '||*line=='\t') ++line;
    return !strncmp(line,"fn ",3)||!strncmp(line,"sub ",4)||!strncmp(line,"const ",6)||!strncmp(line,"use ",4);
}
static char *join(const char *a,size_t alen,const char *b,size_t blen) {
    char *r=malloc(alen+blen+2);
    if (!r) return NULL;
    memcpy(r,a,alen); r[alen]='\n'; memcpy(r+alen+1,b,blen); r[alen+blen+1]='\0';
    return r;
}
int qs_repl(qs_context *context) {
    size_t capacity=4096,decl_length=0;
    char *line=malloc(capacity),*decls=malloc(1);
    if (!line||!decls) { free(line); free(decls); return 1; }
    decls[0]='\0';
    for (;;) {
        size_t size=0; int c;
        fputs("qscript> ",stdout); fflush(stdout);
        while ((c=getchar())!=EOF&&c!='\n') {
            if (size+1>=capacity) {
                char *grown;
                if (capacity>=1024u*1024u) { fputs("qscript: REPL line limit exceeded\n",stderr); free(line); free(decls); return 1; }
                capacity*=2; grown=realloc(line,capacity);
                if (!grown) { free(line); free(decls); return 1; } line=grown;
            }
            line[size++]=(char)c;
        }
        line[size]='\0';
        if (c==EOF&&!size) break;
        if (!strcmp(line,":quit")||!strcmp(line,":q")) break;
        if (!strcmp(line,":help")) {
            puts("Enter a statement or a whole program on one line, e.g. return 6 * 7;\n"
                 "fn/sub/const/use lines are remembered for later lines; variables are not.\n"
                 ":decls shows remembered declarations, :reset forgets them, :libs lists\n"
                 "the standard library, :quit exits.");
            continue;
        }
        if (!strcmp(line,":decls")) { fputs(decls,stdout); if (decl_length) putchar('\n'); continue; }
        if (!strcmp(line,":reset")) { decls[0]='\0'; decl_length=0; continue; }
        if (!strcmp(line,":libs")) {
            size_t i,n=qs_stdlib_count();
            for (i=0;i<n;++i) printf("%s%s",i?" ":"",qs_stdlib_name(i));
            putchar('\n'); continue;
        }
        if (size) {
            char *program=join(decls,decl_length,line,size);
            if (!program) { free(line); free(decls); return 1; }
            if (is_declaration(line)) {
                qs_program *p=qs_compile(context,program,decl_length+size+1);
                if (p) { qs_program_destroy(p); free(decls); decls=program; decl_length+=size+1; program=NULL; }
                else fprintf(stderr,"%s\n",qs_error(context));
            } else {
                int64_t result;
                if (qs_eval(context,program,decl_length+size+1,&result)==QS_OK) printf("%" PRId64 "\n",result);
                else fprintf(stderr,"%s\n",qs_error(context));
            }
            free(program);
        }
        if (c==EOF) break;
    }
    free(line); free(decls); return ferror(stdin)?1:0;
}
#ifdef QSCRIPT_REPL_STANDALONE
int main(void) {
    qs_context *context=qs_create(); int status;
    if (!context) return 1;
    qs_set_output(context,qs_driver_output,stdout);
    status=qs_repl(context); qs_destroy(context); return status;
}
#endif

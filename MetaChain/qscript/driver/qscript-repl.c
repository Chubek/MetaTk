#include "Driver.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int qs_repl(qs_context *context) {
    size_t capacity=4096;
    char *line=malloc(capacity);
    if (!line) return 1;
    /* Each line is an independent program. No hidden persistent register state. */
    for (;;) {
        size_t size=0; int c;
        fputs("qscript> ",stdout); fflush(stdout);
        while ((c=getchar())!=EOF&&c!='\n') {
            if (size+1>=capacity) {
                char *grown;
                if (capacity>=1024u*1024u) { fputs("qscript: REPL line limit exceeded\n",stderr); free(line); return 1; }
                capacity*=2; grown=realloc(line,capacity);
                if (!grown) { free(line); return 1; } line=grown;
            }
            line[size++]=(char)c;
        }
        line[size]='\0';
        if (c==EOF&&!size) break;
        if (!strcmp(line,":quit")||!strcmp(line,":q")) break;
        if (!strcmp(line,":help")) { puts("Enter a complete program on one line, e.g. return 6 * 7;\n:quit exits. Programs do not share variables."); continue; }
        if (size) {
            int64_t result;
            if (qs_eval(context,line,size,&result)==QS_OK) printf("%" PRId64 "\n",result);
            else fprintf(stderr,"%s\n",qs_error(context));
        }
        if (c==EOF) break;
    }
    free(line); return ferror(stdin)?1:0;
}
#ifdef QSCRIPT_REPL_STANDALONE
int main(void) {
    qs_context *context=qs_create(); int status;
    if (!context) return 1;
    qs_set_output(context,qs_driver_output,stdout);
    status=qs_repl(context); qs_destroy(context); return status;
}
#endif

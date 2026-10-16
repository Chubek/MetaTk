#ifndef QSCRIPT_DRIVER_H
#define QSCRIPT_DRIVER_H
#include "qscript/QScript.h"
#include <stdio.h>
int qs_driver_main(int argc,char **argv);
int qs_repl(qs_context *);
void qs_driver_output(int64_t value,void *user);
char *qs_driver_read(FILE *stream,size_t *length);
#endif

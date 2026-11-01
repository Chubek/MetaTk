#define MULIB_IMPL_SEXPR
#define MULIB_IMPL_MANPAGE
#define MULIB_IMPL_MICROTCL
#define MULIB_IMPL_ARGPARSE
#define MULIB_IMPL_STACK
#include "mulib-sexpr.h"
#include "mulib-manpage.h"
#include "mulib-microtcl.h"
#include "mulib-argparse.h"
#include "mulib-stack.h"
#include <stdio.h>
int main(int argc,char **argv){
    mulib_sexpr_t *sexpr=mulib_sexpr_cont();
    mulib_manpage_t *manpage=mulib_manpage_linux();
    mulib_microtcl_t *microtcl=mulib_microtcl_loadall();
    mulib_argparse_t *args=mulib_argparse_load();
    mulib_stack_t *stack=mulib_stack_create();
    if (!sexpr||!manpage||!microtcl||!args||!stack) return 1;
    (void)argc;(void)argv;
    printf("MuLIB ready\n");
    mulib_stack_destroy(stack);mulib_argparse_destroy(args);
    mulib_microtcl_destroy(microtcl);mulib_manpage_destroy(manpage);
    mulib_sexpr_destroy(sexpr);return 0;
}

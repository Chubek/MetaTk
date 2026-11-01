#define MULIB_IMPL_MIR
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_CCWEAVE
#include "mulib-ccweave.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(void) {
    mulib_mir_instruction_t code[] = {
        { MULIB_MIR_CONST, MULIB_MIR_I64, MULIB_MIR_NONE, MULIB_MIR_NONE, 42 },
        { MULIB_MIR_RETURN, MULIB_MIR_VOID, 0, MULIB_MIR_NONE, 0 }
    };
    mulib_mir_program_t program = { code, 2, 0 };
    char *source = NULL;
    assert(mulib_ccweave_generate(&program, "mulib_generated_answer", 2, 8192, &source, NULL) == MULIB_MIR_OK);
    assert(strstr(source, "INT64_C(42)"));
    assert(fputs(source, stdout) >= 0);
    free(source);
    return 0;
}

#define MULIB_IMPL_MIR
#define MULIB_IMPL_MINIVM
#define MULIB_IMPL_CMACHINE
#include "mulib-cmachine.h"
#include <assert.h>
int main(void) {
    mulib_mir_instruction_t code[] = {
        { MULIB_MIR_PARAM, MULIB_MIR_I64, MULIB_MIR_NONE, MULIB_MIR_NONE, 0 },
        { MULIB_MIR_CONST, MULIB_MIR_I64, MULIB_MIR_NONE, MULIB_MIR_NONE, 7 },
        { MULIB_MIR_MUL, MULIB_MIR_I64, 0, 1, 0 },
        { MULIB_MIR_RETURN, MULIB_MIR_VOID, 2, MULIB_MIR_NONE, 0 }
    };
    mulib_mir_program_t program = { code, 4, 1 };
    mulib_cmachine_t machine = { 0 };
    mulib_minivm_result_t result;
    int64_t argument = 6;
    assert(mulib_cmachine_lower(&program, &argument, 1, 4, 32, &machine, NULL) == MULIB_MIR_OK);
    argument = 100; /* Lowering copied the argument into PUSH instructions. */
    assert(mulib_cmachine_run(&machine, NULL, &result) == MULIB_MINIVM_OK);
    assert(result.value == 42);
    mulib_cmachine_free(&machine);
    return 0;
}

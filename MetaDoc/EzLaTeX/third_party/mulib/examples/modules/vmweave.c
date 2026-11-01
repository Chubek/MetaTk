#define MULIB_IMPL_MIR
#define MULIB_IMPL_MINIVM
#define MULIB_IMPL_CMACHINE
#define MULIB_IMPL_VMWEAVE
#include "mulib-vmweave.h"
#include "mulib-cmachine.h"
#include <assert.h>
int main(void) {
    mulib_mir_instruction_t code[] = {
        { MULIB_MIR_CONST, MULIB_MIR_I64, MULIB_MIR_NONE, MULIB_MIR_NONE, 6 },
        { MULIB_MIR_CONST, MULIB_MIR_I64, MULIB_MIR_NONE, MULIB_MIR_NONE, 7 },
        { MULIB_MIR_MUL, MULIB_MIR_I64, 0, 1, 0 },
        { MULIB_MIR_RETURN, MULIB_MIR_VOID, 2, MULIB_MIR_NONE, 0 }
    };
    mulib_mir_program_t input = { code, 4, 0 }, output;
    mulib_mir_buffer_t optimized = { 0 };
    mulib_cmachine_t machine = { 0 };
    mulib_minivm_result_t result;
    assert(mulib_vmweave_optimize(&input, 4, &optimized, NULL) == MULIB_MIR_OK);
    output = mulib_mir_view(&optimized);
    assert(output.count == 2 && output.code[0].immediate == 42);
    assert(mulib_cmachine_lower(&output, NULL, 0, 4, 32, &machine, NULL) == MULIB_MIR_OK);
    assert(mulib_cmachine_run(&machine, NULL, &result) == MULIB_MINIVM_OK && result.value == 42);
    mulib_cmachine_free(&machine); mulib_mir_buffer_free(&optimized);
    return 0;
}

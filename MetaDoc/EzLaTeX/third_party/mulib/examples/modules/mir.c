#define MULIB_IMPL_MIR
#include "mulib-mir.h"
#include <assert.h>
int main(void) {
    mulib_mir_buffer_t buffer = { 0 };
    mulib_mir_instruction_t literal = { MULIB_MIR_CONST, MULIB_MIR_I64, MULIB_MIR_NONE, MULIB_MIR_NONE, 42 };
    mulib_mir_instruction_t ret = { MULIB_MIR_RETURN, MULIB_MIR_VOID, 0, MULIB_MIR_NONE, 0 };
    mulib_mir_program_t program;
    mulib_mir_diagnostic_t diagnostic;
    assert(mulib_mir_buffer_init(&buffer, 2, 0) == MULIB_MIR_OK);
    assert(mulib_mir_append(&buffer, literal) == MULIB_MIR_OK);
    assert(mulib_mir_append(&buffer, ret) == MULIB_MIR_OK);
    program = mulib_mir_view(&buffer);
    assert(mulib_mir_validate(&program, 2, &diagnostic) == MULIB_MIR_OK);
    buffer.code[1].a = 1; /* A self-reference is rejected, not executed. */
    assert(mulib_mir_validate(&program, 2, &diagnostic) == MULIB_MIR_BAD_REFERENCE);
    assert(diagnostic.record == 1);
    mulib_mir_buffer_free(&buffer);
    return 0;
}

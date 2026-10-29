#define MULIB_IMPL_MINIVM
#include "mulib-minivm.h"
#include <assert.h>

int main(void) {
    /* Accumulate 5 + 4 + 3 + 2 + 1 using two locals and an absolute branch. */
    const mulib_minivm_instruction_t code[] = {
        { MULIB_MINIVM_PUSH, 5 }, { MULIB_MINIVM_STORE, 0 },
        { MULIB_MINIVM_LOAD, 1 }, { MULIB_MINIVM_LOAD, 0 },
        { MULIB_MINIVM_ADD, 0 }, { MULIB_MINIVM_STORE, 1 },
        { MULIB_MINIVM_LOAD, 0 }, { MULIB_MINIVM_PUSH, 1 },
        { MULIB_MINIVM_SUB, 0 }, { MULIB_MINIVM_DUP, 0 },
        { MULIB_MINIVM_STORE, 0 }, { MULIB_MINIVM_JNZ, 2 },
        { MULIB_MINIVM_LOAD, 1 }, { MULIB_MINIVM_HALT, 0 }
    };
    const mulib_minivm_program_t program = { code, sizeof(code) / sizeof(code[0]), 2, NULL, 0 };
    mulib_minivm_result_t result;
    assert(mulib_minivm_run(&program, NULL, &result) == MULIB_MINIVM_OK);
    assert(result.value == 15 && result.stack_size == 1 && result.pc == 13);
    return 0;
}

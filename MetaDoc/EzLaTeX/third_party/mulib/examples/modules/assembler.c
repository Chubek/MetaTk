#define MULIB_IMPL_ASSEMBLER
#define MULIB_IMPL_DSYMSPEC
#define MULIB_IMPL_EMITTER
#define MULIB_IMPL_LINKER
#define MULIB_IMPL_JSON
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_JIT
#define MULIB_IMPL_DYNCALL
#include "mulib-assembler.h"
#include "mulib-jit.h"
#include <assert.h>
#include <string.h>

int main(void)
{
    unsigned char code[32];
    mulib_emitter_t emitter;
    mulib_dsymspec_t *spec = mulib_dsymspec_load_builtin("amd64", NULL);
    mulib_linker_t *linker = mulib_linker_create(1, 1);
    mulib_assembler_operand_t operands[2];
    mulib_jit_t *jit = NULL;
    int64_t result;
    assert(spec && linker);
    assert(mulib_emitter_init(&emitter, code, sizeof(code), MULIB_EMITTER_LITTLE) == 0);
    operands[0] = mulib_assembler_symbol("answer", 0);
    assert(mulib_assembler_emit(spec, &emitter, linker, "jmp", operands, 1, NULL) == 0);
    assert(mulib_assembler_emit(spec, &emitter, linker, "nop", NULL, 0, NULL) == 0);
    assert(mulib_linker_define(linker, "answer", emitter.size, 0) == 0);
    operands[0] = mulib_assembler_reg("rax"); operands[1] = mulib_assembler_imm(42);
    assert(mulib_assembler_emit(spec, &emitter, linker, "mov", operands, 2, NULL) == 0);
    assert(mulib_assembler_emit(spec, &emitter, linker, "ret", NULL, 0, NULL) == 0);
    assert(mulib_linker_apply(linker, &emitter, 0, NULL) == 0);
    assert(!memcmp(code, "\xe9\x01\0\0\0\x90\x48\xb8\x2a", 9));
    if (mulib_jit_supported()) {
        assert(mulib_jit_create(&jit, emitter.size) == 0);
        assert(mulib_jit_write(jit, 0, code, emitter.size) == 0);
        assert(mulib_jit_seal(jit) == 0);
        assert(mulib_jit_run_i64_0(jit, &result) == 0 && result == 42);
        assert(mulib_jit_destroy(jit) == 0);
    }
    mulib_linker_destroy(linker);
    mulib_dsymspec_destroy(spec);
    return 0;
}

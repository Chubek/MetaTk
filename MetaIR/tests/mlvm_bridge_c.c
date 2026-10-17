#include "MLVM/include/MLVM-Bridge.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int mlvm_bridge_metair_tests(void);
int mlvm_bridge_metair_run(const unsigned char *, size_t, size_t, int64_t);

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "bridge C line %d: %s (%s)\n", __LINE__, #x, mlvm_error(c)); goto fail; } } while (0)

int main(void) {
    mlvm_context *c = mlvm_create(), *other = NULL;
    mlvm_module *m = NULL, *optimized = NULL, *copy = NULL, *bad = NULL;
    mlvm_buffer bytes = {NULL, 0}, text = {NULL, 0}, json = {NULL, 0};
    unsigned char *corrupt = NULL;
    int64_t result = 0;
    int engine;
    const char *source = "module 1 { const r0 0; jz r0 4; const r1 42; jmp 5; const r1 42; mov r2 r1; ret r2; }";
    CHECK(c);
    CHECK(mlvm_parse(c, source, strlen(source), &m) == MLVM_OK);
    CHECK(mlvm_module_instruction_count(m) == 7 && mlvm_module_instruction_count(NULL) == 0);
    CHECK(mlvm_export_mcil(c, m, &bytes) == MLVM_OK);
    CHECK(bytes.size == 12 + 7 * 12 && !memcmp(bytes.data, "MCIL\1\0\0\0", 8));
    CHECK(bytes.data[bytes.size] == 0 && bytes.data[bytes.size - 12] == 23);
    CHECK(mlvm_bridge_metair_run(bytes.data, bytes.size, 100, 42));
    CHECK(mlvm_import_mcil(c, bytes.data, bytes.size, &copy) == MLVM_OK);
    bytes.data[0] = 0; /* The import is an owning copy, not a borrowed byte span. */
    CHECK(mlvm_run(c, copy, 100, &result) == MLVM_OK && result == 42);
    bytes.data[0] = 'M';
    CHECK(mlvm_analyze(c, m, &json) == MLVM_OK && json.data[json.size] == 0);
    CHECK(strstr((const char *)json.data, "\"instruction_count\":7"));
    mlvm_buffer_destroy(&json);

    for (engine = MLVM_INTERPRETER; engine <= MLVM_JIT; ++engine) {
        int mode;
        CHECK(mlvm_set_engine(c, engine) == MLVM_OK);
        CHECK(mlvm_run(c, m, 100, &result) == MLVM_OK && result == 42);
        for (mode = MLVM_OPT_NONE; mode <= MLVM_OPT_GLOBAL; ++mode) {
            CHECK(mlvm_optimize(c, m, mode, &optimized) == MLVM_OK);
            CHECK(mlvm_run(c, optimized, 100, &result) == MLVM_OK && result == 42);
            CHECK(mlvm_text(c, optimized, &text) == MLVM_OK);
            CHECK(strstr((const char *)text.data, mode == MLVM_OPT_GLOBAL ? "const r2 42;" : "mov r2 r1;"));
            mlvm_buffer_destroy(&text);
            mlvm_module_destroy(optimized); optimized = NULL;
        }
    }
    CHECK(mlvm_text(c, m, &text) == MLVM_OK && strstr((const char *)text.data, "mov r2 r1;"));
    mlvm_buffer_destroy(&text);

    corrupt = (unsigned char *)malloc(bytes.size + 1); CHECK(corrupt);
    {
        size_t i;
        const unsigned char unsupported[] = {0, 17, 18, 19, 20, 21, 22, 24, 255};
        for (i = 0; i < sizeof(unsupported); ++i) {
            memcpy(corrupt, bytes.data, bytes.size); corrupt[12] = unsupported[i];
            CHECK(mlvm_import_mcil(c, corrupt, bytes.size, &bad) == MLVM_ERROR && !bad);
            CHECK(strstr(mlvm_error(c), "unsupported MetaIL opcode"));
        }
    }
    memcpy(corrupt, bytes.data, bytes.size); corrupt[5] = 1;
    CHECK(mlvm_import_mcil(c, corrupt, bytes.size, &bad) == MLVM_ERROR && !bad && strstr(mlvm_error(c), "header"));
    memcpy(corrupt, bytes.data, bytes.size); corrupt[4] = 2;
    CHECK(mlvm_import_mcil(c, corrupt, bytes.size, &bad) == MLVM_ERROR && !bad);
    memcpy(corrupt, bytes.data, bytes.size); memset(corrupt + 8, 255, 4);
    CHECK(mlvm_import_mcil(c, corrupt, bytes.size, &bad) == MLVM_ERROR && !bad && strstr(mlvm_error(c), "length"));
    memcpy(corrupt, bytes.data, bytes.size); corrupt[bytes.size] = 0;
    CHECK(mlvm_import_mcil(c, corrupt, bytes.size + 1, &bad) == MLVM_ERROR && !bad);
    CHECK(mlvm_import_mcil(c, bytes.data, bytes.size - 1, &bad) == MLVM_ERROR && !bad);
    memcpy(corrupt, bytes.data, bytes.size); corrupt[bytes.size - 10] = 1; /* HALT unused b */
    CHECK(mlvm_import_mcil(c, corrupt, bytes.size, &bad) == MLVM_ERROR && !bad && strstr(mlvm_error(c), "noncanonical"));
    memcpy(corrupt, bytes.data, 12); memset(corrupt + 8, 0, 4); corrupt[8] = 1;
    memset(corrupt + 12, 0, 12); corrupt[12] = 23; /* HALT r0 without a definition */
    CHECK(mlvm_import_mcil(c, corrupt, 24, &bad) == MLVM_ERROR && !bad && strstr(mlvm_error(c), "undefined register"));

    CHECK(mlvm_set_engine(c, 99) == MLVM_INVALID_ARGUMENT && strstr(mlvm_error(c), "engine"));
    CHECK(mlvm_set_host_step_limit(c, 0) == MLVM_INVALID_ARGUMENT);
    CHECK(mlvm_optimize(c, m, 99, &bad) == MLVM_INVALID_ARGUMENT && !bad);
    result = 999;
    CHECK(mlvm_run(c, m, 0, &result) == MLVM_INVALID_ARGUMENT && result == 0);
    CHECK(mlvm_parse(c, NULL, 1, &bad) == MLVM_INVALID_ARGUMENT && !bad);
    CHECK(mlvm_parse(c, "", SIZE_MAX, &bad) == MLVM_ERROR && !bad);
    CHECK(mlvm_import_mcil(c, NULL, 0, &bad) == MLVM_ERROR && !bad);
    CHECK(mlvm_parse(c, "", 0, &bad) == MLVM_ERROR && !bad);
    CHECK(mlvm_run(c, NULL, 10, &result) == MLVM_INVALID_ARGUMENT && result == 0);
    CHECK(mlvm_export_mcil(c, NULL, &text) == MLVM_INVALID_ARGUMENT && !text.data && !text.size);
    CHECK(mlvm_analyze(c, m, NULL) == MLVM_INVALID_ARGUMENT);
    CHECK(mlvm_set_host_step_limit(c, 1) == MLVM_OK);
    CHECK(mlvm_run(c, m, 100, &result) == MLVM_ERROR && result == 0 && strstr(mlvm_error(c), "step limit"));
    CHECK(mlvm_set_host_step_limit(c, 10000000) == MLVM_OK);
    CHECK(mlvm_run(c, m, 100, &result) == MLVM_OK && result == 42 && !*mlvm_error(c));
    {
        const char *trap = "module 1 { const r0 1; const r1 0; div r0 r0 r1; ret r0; }";
        CHECK(mlvm_parse(c, trap, strlen(trap), &bad) == MLVM_OK);
        CHECK(mlvm_run(c, bad, 10, &result) == MLVM_ERROR && result == 0 && strstr(mlvm_error(c), "invalid division"));
        mlvm_module_destroy(bad); bad = NULL;
        CHECK(mlvm_parse(c, "module 1 { jmp 0; }", strlen("module 1 { jmp 0; }"), &bad) == MLVM_OK);
        CHECK(mlvm_run(c, bad, 2, &result) == MLVM_ERROR && strstr(mlvm_error(c), "MLVM: step limit exceeded"));
        mlvm_module_destroy(bad); bad = NULL;
    }
    CHECK(mlvm_bridge_metair_tests());
    mlvm_destroy(c); c = NULL;
    CHECK(bytes.data[0] == 'M' && mlvm_module_instruction_count(copy) == 7);
    other = mlvm_create(); CHECK(other);
    CHECK(mlvm_run(other, copy, 100, &result) == MLVM_OK && result == 42);
    CHECK(mlvm_export_mcil(other, copy, &text) == MLVM_OK && text.size == bytes.size && !memcmp(text.data, bytes.data, text.size));
    mlvm_buffer_destroy(&text);
    bad = (mlvm_module *)1;
    CHECK(mlvm_parse(NULL, "", 0, &bad) == MLVM_INVALID_ARGUMENT && !bad);
    CHECK(mlvm_text(NULL, m, &text) == MLVM_INVALID_ARGUMENT && !text.data && !text.size);
    CHECK(mlvm_run(NULL, m, 10, &result) == MLVM_INVALID_ARGUMENT && result == 0);
    mlvm_destroy(other);
    mlvm_module_destroy(m); mlvm_module_destroy(copy);
    mlvm_buffer_destroy(&bytes); mlvm_buffer_destroy(&bytes);
    mlvm_destroy(NULL); mlvm_module_destroy(NULL); mlvm_buffer_destroy(NULL);
    free(corrupt);
    puts("MLVM C/QScript/MetaIR bridge passed");
    return 0;
fail:
    free(corrupt);
    mlvm_buffer_destroy(&bytes); mlvm_buffer_destroy(&text); mlvm_buffer_destroy(&json);
    mlvm_module_destroy(m); mlvm_module_destroy(copy); mlvm_module_destroy(optimized); mlvm_module_destroy(bad);
    mlvm_destroy(c); mlvm_destroy(other);
    return 1;
}

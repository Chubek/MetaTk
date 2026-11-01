/* Define MULIB_RUNTIME_BINDING_FIXTURE to build the optional native test .so.
 * Otherwise this is a self-contained C test. With three command-line arguments
 * it also writes Python source, Ruby source, and uses the given library path.
 * No Python/Ruby interpreter is needed for the default test run. */
#ifdef MULIB_RUNTIME_BINDING_FIXTURE
#include <stddef.h>
int mulib_runtime_add(int a, int b) { return a + b; }
const char *mulib_runtime_text(const char *s) { return s; }
const char *mulib_runtime_null(void) { return NULL; }
void mulib_runtime_nothing(void) { }
void *mulib_runtime_pointer(void *p) { return p; }
#define MULIB_RUNTIME_ID(name, type) type mulib_runtime_##name(type value) { return value; }
MULIB_RUNTIME_ID(schar, signed char)
MULIB_RUNTIME_ID(uchar, unsigned char)
MULIB_RUNTIME_ID(short, short)
MULIB_RUNTIME_ID(ushort, unsigned short)
MULIB_RUNTIME_ID(int, int)
MULIB_RUNTIME_ID(uint, unsigned int)
MULIB_RUNTIME_ID(long, long)
MULIB_RUNTIME_ID(ulong, unsigned long)
MULIB_RUNTIME_ID(llong, long long)
MULIB_RUNTIME_ID(ullong, unsigned long long)
MULIB_RUNTIME_ID(float, float)
MULIB_RUNTIME_ID(double, double)
#undef MULIB_RUNTIME_ID
#else
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_VEC
#define MULIB_IMPL_MAP
#define MULIB_IMPL_PYBIND
#define MULIB_IMPL_RUBYBIND
#define MULIB_IMPL_MINIVM
#define MULIB_IMPL_MINISH
#include "mulib-rubybind.h"
#include "mulib-minivm.h"
#include "mulib-minish.h"
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef mulib_bind_status_t (*generator_fn)(const char *, const mulib_bind_function_t *,
    size_t, char **, size_t *, mulib_bind_error_t *);

static void test_bindings(void) {
    const generator_fn generators[] = { mulib_pybind_generate, mulib_rubybind_generate };
    mulib_bind_type_t args[] = { MULIB_BIND_INT, MULIB_BIND_CSTRING };
    mulib_bind_function_t f[2] = {
        { "invoke", "native'\"\\\n#{raise 'injection'}", MULIB_BIND_INT, args, 2 },
        { "other", NULL, MULIB_BIND_VOID, NULL, 0 }
    };
    mulib_bind_error_t error;
    char *source;
    size_t length, g;
    for (g = 0; g < sizeof(generators) / sizeof(generators[0]); ++g) {
        f[0].symbol = "native'\"\\\n#{raise 'injection'}";
        assert(generators[g]("./lib'\"\\\n#{abort}.so", f, 2, &source, &length, &error) == MULIB_BIND_OK);
        assert(source && strlen(source) == length && error.status == MULIB_BIND_OK);
        assert(strstr(source, "\\x27\\x22\\x5c\\x0a"));
        assert(strstr(source, "\\x6e\\x61\\x74\\x69\\x76\\x65\\x27\\x22"));
        assert(!strstr(source, "#{abort}") && !strstr(source, "#{raise"));
        free(source);
        assert(generators[g]("library", NULL, 0, &source, &length, NULL) == MULIB_BIND_OK);
        assert(source && length); free(source);
        source = (char *)f; length = 999;
        assert(generators[g]("", f, 2, &source, &length, NULL) == MULIB_BIND_INVALID_ARGUMENT);
        assert(!source && !length);
        assert(generators[g]("library", f, 2, NULL, NULL, NULL) == MULIB_BIND_INVALID_ARGUMENT);
        f[0].name = "bad-name";
        assert(generators[g]("library", f, 2, &source, NULL, &error) == MULIB_BIND_INVALID_NAME);
        assert(!source && error.function_index == 0 && error.argument_index == SIZE_MAX);
        f[0].name = "_mulib_library";
        assert(generators[g]("library", f, 2, &source, NULL, NULL) == MULIB_BIND_INVALID_NAME);
        f[0].name = "class";
        assert(generators[g]("library", f, 2, &source, NULL, NULL) == MULIB_BIND_INVALID_NAME);
        f[0].name = "other";
        assert(generators[g]("library", f, 2, &source, NULL, &error) == MULIB_BIND_DUPLICATE_NAME);
        assert(error.function_index == 1);
        f[0].name = "invoke";
        f[0].result = (mulib_bind_type_t)-1;
        assert(generators[g]("library", f, 2, &source, NULL, NULL) == MULIB_BIND_INVALID_TYPE);
        f[0].result = MULIB_BIND_INT;
        args[1] = MULIB_BIND_VOID;
        assert(generators[g]("library", f, 2, &source, NULL, &error) == MULIB_BIND_INVALID_TYPE);
        assert(error.function_index == 0 && error.argument_index == 1);
        args[1] = (mulib_bind_type_t)999;
        assert(generators[g]("library", f, 2, &source, NULL, NULL) == MULIB_BIND_INVALID_TYPE);
        args[1] = MULIB_BIND_CSTRING;
        f[0].argument_count = MULIB_BIND_MAX_ARGUMENTS + 1;
        assert(generators[g]("library", f, 2, &source, NULL, NULL) == MULIB_BIND_LIMIT);
        f[0].argument_count = 2;
        f[0].arguments = NULL;
        assert(generators[g]("library", f, 2, &source, NULL, NULL) == MULIB_BIND_INVALID_ARGUMENT);
        f[0].arguments = args;
        f[0].symbol = "";
        assert(generators[g]("library", f, 2, &source, NULL, NULL) == MULIB_BIND_INVALID_ARGUMENT);
        f[0].symbol = NULL;
        assert(generators[g]("library", f, MULIB_BIND_MAX_FUNCTIONS + 1,
                             &source, NULL, NULL) == MULIB_BIND_LIMIT);
        assert(generators[g]("library", NULL, 1, &source, NULL, NULL) == MULIB_BIND_INVALID_ARGUMENT);
    }
    {
        char long_text[MULIB_BIND_MAX_TEXT + 2], name[MULIB_BIND_MAX_NAME + 2];
        memset(long_text, 'x', sizeof(long_text) - 1); long_text[sizeof(long_text) - 1] = 0;
        assert(mulib_pybind_generate(long_text, NULL, 0, &source, NULL, NULL) == MULIB_BIND_INVALID_ARGUMENT);
        assert(mulib_rubybind_generate(long_text, NULL, 0, &source, NULL, NULL) == MULIB_BIND_INVALID_ARGUMENT);
        long_text[MULIB_BIND_MAX_TEXT] = 0;
        assert(mulib_pybind_generate(long_text, NULL, 0, &source, NULL, NULL) == MULIB_BIND_OK); free(source);
        memset(name, 'a', sizeof(name) - 1); name[sizeof(name) - 1] = 0; f[0].name = name;
        assert(mulib_bind_validate(f, 1, NULL) == MULIB_BIND_INVALID_NAME);
        name[MULIB_BIND_MAX_NAME] = 0;
        assert(mulib_bind_validate(f, 1, NULL) == MULIB_BIND_OK);
    }
    f[0].name = "__file__";
    assert(mulib_pybind_generate("library", f, 1, &source, NULL, NULL) == MULIB_BIND_INVALID_NAME);
    f[0].name = "Uppercase";
    assert(mulib_rubybind_generate("library", f, 1, &source, NULL, NULL) == MULIB_BIND_INVALID_NAME);
    f[0].name = "singleton_method_added";
    assert(mulib_rubybind_generate("library", f, 1, &source, NULL, NULL) == MULIB_BIND_INVALID_NAME);
    {
        mulib_bind_type_t many_arguments[MULIB_BIND_MAX_ARGUMENTS];
        char names[MULIB_BIND_MAX_FUNCTIONS][16];
        mulib_bind_function_t many[MULIB_BIND_MAX_FUNCTIONS];
        size_t i;
        for (i = 0; i < MULIB_BIND_MAX_ARGUMENTS; ++i) many_arguments[i] = MULIB_BIND_INT;
        f[0].name = "full"; f[0].symbol = NULL;
        f[0].arguments = many_arguments; f[0].argument_count = MULIB_BIND_MAX_ARGUMENTS;
        for (g = 0; g < 2; ++g) {
            assert(generators[g]("library", f, 1, &source, &length, NULL) == MULIB_BIND_OK);
            free(source);
        }
        for (i = 0; i < MULIB_BIND_MAX_FUNCTIONS; ++i) {
            int n = snprintf(names[i], sizeof(names[i]), "function%zu", i);
            assert(n > 0 && (size_t)n < sizeof(names[i]));
            many[i] = (mulib_bind_function_t){ names[i], NULL, MULIB_BIND_VOID, NULL, 0 };
        }
        for (g = 0; g < 2; ++g) {
            assert(generators[g]("library", many, MULIB_BIND_MAX_FUNCTIONS,
                                 &source, &length, NULL) == MULIB_BIND_OK);
            free(source);
        }
    }
    assert(!strcmp(mulib_bind_status_string(MULIB_BIND_OK), "success"));
}

static void test_binding_ruby_numbered_names(void) {
    const char *const accepted[] = { "_0", "_10", "_01", "_1x", "_9_", "__1", "a_1" };
    mulib_bind_function_t functions[] = {
        { "first", "native", MULIB_BIND_INT, NULL, 0 },
        { NULL, "native", MULIB_BIND_INT, NULL, 0 }
    };
    mulib_bind_error_t error;
    char *source;
    size_t i, length;
    for (i = 1; i <= 9; ++i) {
        char name[] = { '_', (char)('0' + i), '\0' };
        functions[1].name = name;
        assert(mulib_bind_validate(functions, 2, NULL) == MULIB_BIND_OK);
        /* These are ordinary Python identifiers, but reserved by Ruby. */
        assert(mulib_pybind_generate("library", functions, 2, &source, &length, NULL) == MULIB_BIND_OK);
        free(source);
        source = (char *)functions; length = SIZE_MAX;
        assert(mulib_rubybind_generate("library", functions, 2, &source, &length, &error) == MULIB_BIND_INVALID_NAME);
        assert(!source && !length && error.status == MULIB_BIND_INVALID_NAME);
        assert(error.function_index == 1 && error.argument_index == SIZE_MAX);
    }
    for (i = 0; i < sizeof(accepted) / sizeof(accepted[0]); ++i) {
        functions[1].name = accepted[i];
        assert(mulib_rubybind_generate("library", functions, 2, &source, &length, &error) == MULIB_BIND_OK);
        assert(source && length && error.status == MULIB_BIND_OK);
        free(source);
    }
}

static void test_binding_native_symbol_bytes(void) {
    static const char hex[] = "0123456789abcdef";
    unsigned char symbol[256];
    mulib_bind_function_t function = { "raw_native", "\xff", MULIB_BIND_INT, NULL, 0 };
    char *source;
    const char *lookup, *path;
    size_t i, length;
    assert(mulib_pybind_generate("lib-\xff.so", &function, 1, &source, &length, NULL) == MULIB_BIND_OK);
    assert(strstr(source, "raw_native = _mulib_library[b'\\xff']\n"));
    path = strstr(source, "_mulib_ctypes.CDLL(_mulib_os.fsdecode(b'");
    assert(path && strstr(path, "\\xff"));
    assert(!strstr(source, "_mulib_library[_mulib_os.fsdecode("));
    free(source);
    /* Byte-for-byte projection of every possible non-NUL native symbol byte.
     * Do not replace arbitrary native symbols with a UTF-8-only contract. */
    for (i = 1; i <= 255; ++i) symbol[i - 1] = (unsigned char)i;
    symbol[255] = 0;
    function.symbol = (const char *)symbol;
    assert(mulib_pybind_generate("library", &function, 1, &source, &length, NULL) == MULIB_BIND_OK);
    lookup = strstr(source, "raw_native = _mulib_library[b'");
    assert(lookup);
    lookup += strlen("raw_native = _mulib_library[b'");
    for (i = 1; i <= 255; ++i) {
        assert(lookup[0] == '\\' && lookup[1] == 'x');
        assert(lookup[2] == hex[i >> 4] && lookup[3] == hex[i & 15]);
        lookup += 4;
    }
    assert(!strncmp(lookup, "']\n", 3));
    for (i = 0; i < length; ++i) assert((unsigned char)source[i] < 128);
    free(source);
}

static void emit_bindings(const char *python, const char *ruby, const char *library) {
    const mulib_bind_type_t pair[] = { MULIB_BIND_INT, MULIB_BIND_INT };
    const mulib_bind_type_t scalar[] = {
        MULIB_BIND_SCHAR, MULIB_BIND_UCHAR, MULIB_BIND_SHORT, MULIB_BIND_USHORT,
        MULIB_BIND_INT, MULIB_BIND_UINT, MULIB_BIND_LONG, MULIB_BIND_ULONG,
        MULIB_BIND_LLONG, MULIB_BIND_ULLONG, MULIB_BIND_FLOAT, MULIB_BIND_DOUBLE,
        MULIB_BIND_POINTER, MULIB_BIND_CSTRING
    };
    const char *const names[] = {
        "schar", "uchar", "short", "ushort", "int", "uint", "long", "ulong",
        "llong", "ullong", "float", "double", "pointer", "text"
    };
    mulib_bind_function_t functions[17];
    char symbols[14][64];
    const generator_fn generators[] = { mulib_pybind_generate, mulib_rubybind_generate };
    const char *paths[2];
    size_t i;
    paths[0] = python; paths[1] = ruby;
    functions[0] = (mulib_bind_function_t){ "add", "mulib_runtime_add", MULIB_BIND_INT, pair, 2 };
    functions[1] = (mulib_bind_function_t){ "nothing", "mulib_runtime_nothing", MULIB_BIND_VOID, NULL, 0 };
    functions[2] = (mulib_bind_function_t){ "null", "mulib_runtime_null", MULIB_BIND_CSTRING, NULL, 0 };
    for (i = 0; i < 14; ++i) {
        int n = snprintf(symbols[i], sizeof(symbols[i]), "mulib_runtime_%s", names[i]);
        assert(n > 0 && (size_t)n < sizeof(symbols[i]));
        functions[i + 3] = (mulib_bind_function_t){ names[i], symbols[i], scalar[i], scalar + i, 1 };
    }
    for (i = 0; i < 2; ++i) {
        char *source;
        size_t length;
        FILE *file;
        assert(generators[i](library, functions, 17, &source, &length, NULL) == MULIB_BIND_OK);
        file = fopen(paths[i], "wb"); assert(file);
        assert(fwrite(source, 1, length, file) == length);
        assert(fclose(file) == 0);
        free(source);
    }
}

static mulib_minivm_result_t run_vm(const mulib_minivm_instruction_t *code, size_t count,
                                  size_t locals, mulib_minivm_status_t expected) {
    mulib_minivm_program_t program = { code, count, locals, NULL, 0 };
    mulib_minivm_result_t result;
    assert(mulib_minivm_run(&program, NULL, &result) == expected);
    assert(result.status == expected);
    return result;
}
static void binary_vm(mulib_minivm_opcode_t op, int64_t left, int64_t right,
                      mulib_minivm_status_t status, int64_t expected) {
    const mulib_minivm_instruction_t code[] = {
        { MULIB_MINIVM_PUSH, left }, { MULIB_MINIVM_PUSH, right },
        { op, 0 }, { MULIB_MINIVM_HALT, 0 }
    };
    mulib_minivm_result_t r = run_vm(code, 4, 0, status);
    if (status == MULIB_MINIVM_OK) assert(r.value == expected && r.stack_size == 1 && r.steps == 4);
    else assert(r.pc == 2 && r.steps == 3 && r.stack_size == 2 && r.value == right);
}
static int vm_host(void *user, const int64_t *args, size_t count, int64_t *out) {
    int status = *(int *)user;
    assert(count == 2 && args[0] == 7 && args[1] == 9);
    *out = 79;
    return status;
}
static int vm_host_zero(void *user, const int64_t *args, size_t count, int64_t *out) {
    (void)args;
    assert(count == 0);
    ++*(int *)user;
    *out = 123;
    return 0;
}
static void test_vm(void) {
    mulib_minivm_instruction_t code[8] = { { MULIB_MINIVM_HALT, 0 } };
    mulib_minivm_program_t program = { code, 1, 0, NULL, 0 };
    mulib_minivm_limits_t limits = { 0, 0, 10 };
    mulib_minivm_result_t result;
    size_t i;
    const mulib_minivm_opcode_t underflows[] = {
        MULIB_MINIVM_DROP, MULIB_MINIVM_DUP, MULIB_MINIVM_SWAP,
        MULIB_MINIVM_ADD, MULIB_MINIVM_SUB, MULIB_MINIVM_MUL, MULIB_MINIVM_DIV,
        MULIB_MINIVM_MOD, MULIB_MINIVM_NEG, MULIB_MINIVM_EQ, MULIB_MINIVM_LT,
        MULIB_MINIVM_LE, MULIB_MINIVM_NOT, MULIB_MINIVM_JZ, MULIB_MINIVM_JNZ
    };
    binary_vm(MULIB_MINIVM_ADD, 20, 22, MULIB_MINIVM_OK, 42);
    binary_vm(MULIB_MINIVM_ADD, INT64_MAX, 1, MULIB_MINIVM_OVERFLOW, 0);
    binary_vm(MULIB_MINIVM_ADD, INT64_MIN, -1, MULIB_MINIVM_OVERFLOW, 0);
    binary_vm(MULIB_MINIVM_SUB, INT64_MIN, 1, MULIB_MINIVM_OVERFLOW, 0);
    binary_vm(MULIB_MINIVM_SUB, INT64_MAX, -1, MULIB_MINIVM_OVERFLOW, 0);
    binary_vm(MULIB_MINIVM_MUL, INT64_MIN, -1, MULIB_MINIVM_OVERFLOW, 0);
    binary_vm(MULIB_MINIVM_MUL, -1, INT64_MIN, MULIB_MINIVM_OVERFLOW, 0);
    binary_vm(MULIB_MINIVM_MUL, INT64_MAX, 2, MULIB_MINIVM_OVERFLOW, 0);
    binary_vm(MULIB_MINIVM_MUL, -3037000500LL, -3037000500LL, MULIB_MINIVM_OVERFLOW, 0);
    binary_vm(MULIB_MINIVM_MUL, -3, 7, MULIB_MINIVM_OK, -21);
    binary_vm(MULIB_MINIVM_MUL, -3, -7, MULIB_MINIVM_OK, 21);
    binary_vm(MULIB_MINIVM_MUL, INT64_MIN, 1, MULIB_MINIVM_OK, INT64_MIN);
    binary_vm(MULIB_MINIVM_MUL, 0, INT64_MIN, MULIB_MINIVM_OK, 0);
    binary_vm(MULIB_MINIVM_DIV, -7, 3, MULIB_MINIVM_OK, -2);
    binary_vm(MULIB_MINIVM_MOD, -7, 3, MULIB_MINIVM_OK, -1);
    binary_vm(MULIB_MINIVM_DIV, INT64_MIN, -1, MULIB_MINIVM_OVERFLOW, 0);
    binary_vm(MULIB_MINIVM_MOD, INT64_MIN, -1, MULIB_MINIVM_OVERFLOW, 0);
    binary_vm(MULIB_MINIVM_DIV, 0, 0, MULIB_MINIVM_DIVIDE_BY_ZERO, 0);
    binary_vm(MULIB_MINIVM_MOD, 1, 0, MULIB_MINIVM_DIVIDE_BY_ZERO, 0);
    binary_vm(MULIB_MINIVM_EQ, 7, 7, MULIB_MINIVM_OK, 1);
    binary_vm(MULIB_MINIVM_LT, -1, 0, MULIB_MINIVM_OK, 1);
    binary_vm(MULIB_MINIVM_LE, 8, 7, MULIB_MINIVM_OK, 0);
    for (i = 0; i < sizeof(underflows) / sizeof(underflows[0]); ++i) {
        code[0].opcode = underflows[i]; code[0].operand = 0;
        result = run_vm(code, 1, 0, MULIB_MINIVM_STACK_UNDERFLOW);
        assert(result.pc == 0 && result.steps == 1 && !result.stack_size);
    }
    code[0] = (mulib_minivm_instruction_t){ MULIB_MINIVM_HALT, 0 };
    assert(mulib_minivm_run(&program, &limits, &result) == MULIB_MINIVM_OK);
    limits.steps = 0;
    assert(mulib_minivm_run(&program, &limits, &result) == MULIB_MINIVM_STEP_LIMIT && result.steps == 0);
    limits.steps = 7; code[0].opcode = MULIB_MINIVM_JMP;
    assert(mulib_minivm_run(&program, &limits, &result) == MULIB_MINIVM_STEP_LIMIT);
    assert(result.pc == 0 && result.steps == 7);
    code[0].opcode = MULIB_MINIVM_PUSH;
    assert(mulib_minivm_run(&program, &limits, &result) == MULIB_MINIVM_STACK_OVERFLOW);
    limits.stack = SIZE_MAX;
    assert(mulib_minivm_run(&program, &limits, &result) == MULIB_MINIVM_RESOURCE_LIMIT);
    limits.stack = 1; program.local_count = 1;
    assert(mulib_minivm_run(&program, &limits, &result) == MULIB_MINIVM_RESOURCE_LIMIT);
    program.local_count = 0;
    assert(mulib_minivm_run(&program, NULL, &result) == MULIB_MINIVM_FELL_OFF_END);
    assert(result.pc == 1 && result.steps == 1 && result.stack_size == 1);
    code[1] = (mulib_minivm_instruction_t){ MULIB_MINIVM_DUP, 0 }; program.code_count = 2;
    assert(mulib_minivm_run(&program, &limits, &result) == MULIB_MINIVM_STACK_OVERFLOW);
    assert(result.pc == 1 && result.stack_size == 1);
    code[0].opcode = MULIB_MINIVM_LOAD; code[0].operand = -1;
    run_vm(code, 1, 1, MULIB_MINIVM_BAD_LOCAL);
    code[0].opcode = MULIB_MINIVM_STORE; code[0].operand = 1;
    run_vm(code, 1, 1, MULIB_MINIVM_BAD_LOCAL);
    code[0].operand = 0;
    run_vm(code, 1, 1, MULIB_MINIVM_STACK_UNDERFLOW);
    code[0].opcode = MULIB_MINIVM_JMP; code[0].operand = -1;
    run_vm(code, 1, 0, MULIB_MINIVM_BAD_JUMP);
    code[0] = (mulib_minivm_instruction_t){ MULIB_MINIVM_PUSH, 1 };
    code[1] = (mulib_minivm_instruction_t){ MULIB_MINIVM_JZ, 2 };
    run_vm(code, 2, 0, MULIB_MINIVM_BAD_JUMP); /* Even untaken targets must be valid. */
    code[1] = (mulib_minivm_instruction_t){ MULIB_MINIVM_NEG, 0 };
    code[0].operand = INT64_MIN;
    result = run_vm(code, 2, 0, MULIB_MINIVM_OVERFLOW);
    assert(result.stack_size == 1 && result.value == INT64_MIN);
    code[0].opcode = (mulib_minivm_opcode_t)999;
    run_vm(code, 1, 0, MULIB_MINIVM_BAD_OPCODE);
    code[0].opcode = MULIB_MINIVM_CALL; code[0].operand = 0;
    run_vm(code, 1, 0, MULIB_MINIVM_BAD_HOST);
    assert(mulib_minivm_run(NULL, NULL, &result) == MULIB_MINIVM_INVALID_PROGRAM);
    program.code_count = 0;
    assert(mulib_minivm_run(&program, NULL, NULL) == MULIB_MINIVM_INVALID_PROGRAM);
    program.code_count = 1; program.host_count = 1;
    assert(mulib_minivm_run(&program, NULL, NULL) == MULIB_MINIVM_INVALID_PROGRAM);
    {
        int host_status = 0, calls = 0;
        mulib_minivm_host_t hosts[] = { { vm_host, &host_status, 2 }, { vm_host_zero, &calls, 0 } };
        const mulib_minivm_instruction_t host_code[] = {
            { MULIB_MINIVM_PUSH, 7 }, { MULIB_MINIVM_PUSH, 9 },
            { MULIB_MINIVM_CALL, 0 }, { MULIB_MINIVM_HALT, 0 }
        };
        mulib_minivm_program_t p = { host_code, 4, 0, hosts, 2 };
        assert(mulib_minivm_run(&p, NULL, &result) == MULIB_MINIVM_OK && result.value == 79);
        host_status = -17;
        assert(mulib_minivm_run(&p, NULL, &result) == MULIB_MINIVM_HOST_ERROR);
        assert(result.host_status == -17 && result.pc == 2 && result.stack_size == 2 && result.value == 9);
        hosts[0].argument_count = 3;
        assert(mulib_minivm_run(&p, NULL, &result) == MULIB_MINIVM_STACK_UNDERFLOW);
        hosts[0].function = NULL;
        assert(mulib_minivm_run(&p, NULL, &result) == MULIB_MINIVM_BAD_HOST);
        code[0] = (mulib_minivm_instruction_t){ MULIB_MINIVM_CALL, 1 };
        code[1] = (mulib_minivm_instruction_t){ MULIB_MINIVM_HALT, 0 };
        p.code = code; p.code_count = 2;
        assert(mulib_minivm_run(&p, NULL, &result) == MULIB_MINIVM_OK && result.value == 123 && calls == 1);
        limits.stack = 0;
        assert(mulib_minivm_run(&p, &limits, &result) == MULIB_MINIVM_STACK_OVERFLOW && calls == 1);
    }
    {
        const mulib_minivm_instruction_t sequence[] = {
            { MULIB_MINIVM_LOAD, 0 }, { MULIB_MINIVM_NOT, 0 },
            { MULIB_MINIVM_JZ, 11 }, { MULIB_MINIVM_PUSH, 6 },
            { MULIB_MINIVM_PUSH, 7 }, { MULIB_MINIVM_SWAP, 0 },
            { MULIB_MINIVM_DROP, 0 }, { MULIB_MINIVM_NEG, 0 },
            { MULIB_MINIVM_DUP, 0 }, { MULIB_MINIVM_STORE, 0 },
            { MULIB_MINIVM_JMP, 12 }, { (mulib_minivm_opcode_t)999, 0 },
            { MULIB_MINIVM_HALT, 0 }
        };
        result = run_vm(sequence, 13, 1, MULIB_MINIVM_OK);
        assert(result.value == -7 && result.pc == 12);
    }
    assert(!strcmp(mulib_minivm_status_string(MULIB_MINIVM_OK), "halted"));
}

static int capture(void *user, const char *text, size_t length) {
    return mulib_strbuf_append_n((mulib_strbuf_t *)user, text, length);
}
typedef struct expected_command {
    size_t argc;
    const char *argv[12];
    int calls, status;
} expected_command_t;
static int check_command(mulib_minish_t *shell, size_t argc, const char *const *argv, void *user) {
    expected_command_t *expected = (expected_command_t *)user;
    size_t i;
    (void)shell;
    assert(argc == expected->argc);
    for (i = 0; i < argc; ++i) assert(!strcmp(argv[i], expected->argv[i]));
    ++expected->calls;
    return expected->status;
}
static int recursive_command(mulib_minish_t *shell, size_t argc, const char *const *argv, void *user) {
    (void)argc; (void)argv; (void)user;
    assert(mulib_minish_execute(shell, "true", 4, NULL) == MULIB_MINISH_INVALID_ARGUMENT);
    assert(mulib_minish_set(shell, "from_callback", "yes") == MULIB_MINISH_OK);
    return 0;
}
static int fail_write(void *user, const char *text, size_t length) {
    (void)user; (void)text; (void)length;
    return 0;
}
static mulib_minish_status_t sh(mulib_minish_t *shell, const char *text, mulib_minish_error_t *error) {
    return mulib_minish_execute(shell, text, strlen(text), error);
}
static void test_shell(void) {
    mulib_minish_t shell;
    mulib_minish_error_t error;
    mulib_minish_limits_t limits;
    mulib_strbuf_t output;
    expected_command_t expected = { 8, { "record", "ab cd", "", "", "", "x$y", "a b", "a$who" }, 0, 17 };
    const char *const unsupported[] = {
        "echo a|b", "echo a&&b", "echo a>b", "echo a<b", "echo (x)",
        "echo `id`", "echo $(id)", "echo $1", "echo ${x:-y}", "echo $$", "echo $@"
    };
    size_t i;
    mulib_minish_init(&shell);
    mulib_strbuf_init(&output);
    mulib_minish_set_writer(&shell, capture, &output);
    assert(mulib_minish_register(&shell, "record", check_command, &expected) == MULIB_MINISH_OK);
    assert(sh(&shell, "record a\"b c\"'d' '' \"$empty\" $undefined 'x$y' a\\ b \"a\\$who\" # ignored", &error) == MULIB_MINISH_OK);
    assert(expected.calls == 1 && shell.last_status == 17);
    assert(sh(&shell, "echo $?; set who 'world with spaces'; echo hello\" $who\"", &error) == MULIB_MINISH_OK);
    assert(!strcmp(output.data, "17\nhello world with spaces\n"));
    assert(mulib_minish_set(&shell, "who", mulib_minish_get(&shell, "who")) == MULIB_MINISH_OK);
    assert(mulib_minish_set(&shell, "who", "a ; $(bad) | >") == MULIB_MINISH_OK);
    expected.argc = 2; expected.argv[1] = "a ; $(bad) | >";
    assert(sh(&shell, "record ${who}", &error) == MULIB_MINISH_OK && expected.calls == 2);
    assert(sh(&shell, "\n ; # empty\n echo x#y;echo '#ok';echo \\#ok;\\\n echo a\\\nb; echo \"c\\\nd\"", &error) == MULIB_MINISH_OK);
    assert(strstr(output.data, "x#y\n#ok\n#ok\nab\ncd\n"));
    assert(sh(&shell, "\\\n# continuation before comment\ntrue;\\\n;true", &error) == MULIB_MINISH_OK);
    assert(sh(&shell, "echo 'a\nb'; echo '$who'; echo $", &error) == MULIB_MINISH_OK);
    assert(strstr(output.data, "a\nb\n$who\n$\n"));
    assert(sh(&shell, "set wrong; echo $?; set 1bad value; echo $?; true extra; false", &error) == MULIB_MINISH_OK);
    assert(shell.last_status == 1 && strstr(output.data, "2\n2\n"));
    assert(sh(&shell, "unset who; unset absent", &error) == MULIB_MINISH_OK && !mulib_minish_get(&shell, "who"));
    assert(mulib_minish_set(&shell, "bad-name", "x") == MULIB_MINISH_INVALID_NAME);
    assert(mulib_minish_unset(&shell, "") == MULIB_MINISH_INVALID_NAME);
    assert(mulib_minish_register(&shell, "echo", check_command, &expected) == MULIB_MINISH_INVALID_NAME);
    assert(mulib_minish_register(&shell, "record", check_command, &expected) == MULIB_MINISH_OK && shell.registered.length == 1);
    {
        char name[MULIB_MINISH_MAX_NAME + 2];
        memset(name, 'a', sizeof(name) - 1); name[sizeof(name) - 1] = 0;
        assert(mulib_minish_set(&shell, name, "x") == MULIB_MINISH_INVALID_NAME);
        name[MULIB_MINISH_MAX_NAME] = 0;
        assert(mulib_minish_set(&shell, name, "x") == MULIB_MINISH_OK);
        assert(!strcmp(mulib_minish_get(&shell, name), "x"));
        assert(mulib_minish_unset(&shell, name) == MULIB_MINISH_OK);
    }
    assert(mulib_minish_register(&shell, "host-command", recursive_command, NULL) == MULIB_MINISH_OK);
    assert(sh(&shell, "host-command", &error) == MULIB_MINISH_OK);
    assert(mulib_minish_unregister(&shell, "host-command"));
    limits = shell.limits; limits.registered = 0;
    assert(mulib_minish_set_limits(&shell, &limits) == MULIB_MINISH_LIMIT);
    assert(mulib_minish_register(&shell, "recurse", recursive_command, NULL) == MULIB_MINISH_OK);
    assert(sh(&shell, "recurse", &error) == MULIB_MINISH_OK && !strcmp(mulib_minish_get(&shell, "from_callback"), "yes"));
    expected.status = -1; expected.argv[1] = "";
    assert(sh(&shell, "record ${absent}", &error) == MULIB_MINISH_CALLBACK_ERROR);
    assert(!shell.running && shell.last_status == 0);
    expected.status = 0; expected.argv[1] = "";
    assert(mulib_minish_unregister(&shell, "record") && !mulib_minish_unregister(&shell, "record"));
    assert(sh(&shell, "record", &error) == MULIB_MINISH_NOT_FOUND && error.offset == 0);
    assert(sh(&shell, "echo '", &error) == MULIB_MINISH_SYNTAX && error.offset == 5);
    assert(sh(&shell, "echo abc\\", &error) == MULIB_MINISH_SYNTAX && error.offset == 8);
    assert(sh(&shell, "echo ${who", &error) == MULIB_MINISH_SYNTAX && error.offset == 5);
    assert(sh(&shell, "echo ${}", &error) == MULIB_MINISH_SYNTAX);
    for (i = 0; i < sizeof(unsupported) / sizeof(unsupported[0]); ++i)
        assert(sh(&shell, unsupported[i], &error) == MULIB_MINISH_UNSUPPORTED);
    {
        const char text[] = "set nul changed;\0echo x";
        assert(mulib_minish_execute(&shell, text, sizeof(text) - 1, &error) == MULIB_MINISH_SYNTAX);
        assert(error.offset == 16 && !mulib_minish_get(&shell, "nul"));
    }
    mulib_minish_set_writer(&shell, fail_write, NULL);
    assert(sh(&shell, "echo x", &error) == MULIB_MINISH_IO_ERROR);
    mulib_minish_set_writer(&shell, capture, &output);
    assert(sh(&shell, "exit -1; exit 256; exit bad; exit 1 2", &error) == MULIB_MINISH_OK);
    assert(!shell.exit_requested && shell.last_status == 2);
    assert(sh(&shell, "exit 29; set never x", &error) == MULIB_MINISH_OK);
    assert(shell.exit_requested && shell.exit_status == 29 && !mulib_minish_get(&shell, "never"));
    assert(sh(&shell, "set never x", &error) == MULIB_MINISH_OK && !mulib_minish_get(&shell, "never"));
    shell.exit_requested = 0;
    assert(sh(&shell, "false;exit", &error) == MULIB_MINISH_OK && shell.exit_status == 1);
    mulib_minish_free(&shell);
    mulib_strbuf_free(&output);
    mulib_minish_init(&shell);
    mulib_strbuf_init(&output);
    mulib_minish_set_writer(&shell, capture, &output);
    limits = shell.limits; limits.commands = 1;
    assert(mulib_minish_set_limits(&shell, &limits) == MULIB_MINISH_OK);
    assert(sh(&shell, "set first yes;set second no", &error) == MULIB_MINISH_LIMIT);
    assert(error.command == 1 && error.offset == 14 && mulib_minish_get(&shell, "first") && !mulib_minish_get(&shell, "second"));
    limits = mulib_minish_default_limits(); limits.variables = 1;
    assert(mulib_minish_set_limits(&shell, &limits) == MULIB_MINISH_OK);
    assert(mulib_minish_set(&shell, "first", "ab") == MULIB_MINISH_OK);
    assert(mulib_minish_set(&shell, "second", "c") == MULIB_MINISH_LIMIT);
    limits.variables = 0;
    assert(mulib_minish_set_limits(&shell, &limits) == MULIB_MINISH_LIMIT);
    limits.variables = 1; limits.word_bytes = 4;
    assert(mulib_minish_set_limits(&shell, &limits) == MULIB_MINISH_OK);
    assert(sh(&shell, "echo 1234", &error) == MULIB_MINISH_OK);
    assert(sh(&shell, "echo 12345", &error) == MULIB_MINISH_LIMIT);
    assert(sh(&shell, "echo ${first}${first}x", &error) == MULIB_MINISH_LIMIT);
    limits.words = 2;
    assert(mulib_minish_set_limits(&shell, &limits) == MULIB_MINISH_OK);
    assert(sh(&shell, "echo a b", &error) == MULIB_MINISH_LIMIT);
    limits.registered = 0;
    assert(mulib_minish_set_limits(&shell, &limits) == MULIB_MINISH_OK);
    assert(mulib_minish_register(&shell, "test", check_command, &expected) == MULIB_MINISH_LIMIT);
    limits.input_bytes = 0;
    assert(mulib_minish_set_limits(&shell, &limits) == MULIB_MINISH_OK);
    assert(mulib_minish_execute(&shell, NULL, 0, &error) == MULIB_MINISH_OK);
    assert(sh(&shell, " ", &error) == MULIB_MINISH_LIMIT);
    assert(mulib_minish_execute(&shell, NULL, 1, &error) == MULIB_MINISH_INVALID_ARGUMENT);
    limits.word_bytes = SIZE_MAX;
    assert(mulib_minish_set_limits(&shell, &limits) == MULIB_MINISH_LIMIT);
    limits = mulib_minish_default_limits(); limits.commands = 0;
    assert(mulib_minish_set_limits(&shell, &limits) == MULIB_MINISH_OK);
    assert(sh(&shell, "true", &error) == MULIB_MINISH_LIMIT);
    limits.commands = 1; limits.words = 0;
    assert(mulib_minish_set_limits(&shell, &limits) == MULIB_MINISH_OK);
    assert(sh(&shell, "true", &error) == MULIB_MINISH_LIMIT);
    assert(sh(&shell, "\\\n# no words", &error) == MULIB_MINISH_OK);
    assert(mulib_minish_unset(&shell, "first") == MULIB_MINISH_OK);
    limits.variables = 0; limits.words = 2; limits.word_bytes = 0;
    assert(mulib_minish_set_limits(&shell, &limits) == MULIB_MINISH_OK);
    assert(mulib_minish_set(&shell, "zero", "") == MULIB_MINISH_LIMIT);
    assert(sh(&shell, "x", &error) == MULIB_MINISH_LIMIT);
#ifndef MULIB_MINISH_ENABLE_POSIX
    assert(mulib_minish_allow_process(&shell, 1) == MULIB_MINISH_UNSUPPORTED && !shell.allow_process);
#endif
    assert(mulib_minish_allow_process(&shell, 0) == MULIB_MINISH_OK);
    mulib_minish_free(&shell);
    mulib_strbuf_free(&output);
    assert(!strcmp(mulib_minish_status_string(MULIB_MINISH_OK), "success"));
}

static void test_shell_malformed_bytes(void) {
    static const char alphabet[] = "echo;set a b\n\t'\"\\$}{()#|&<>`?012_ \xc3\xa9";
    uint32_t random = UINT32_C(0x12345678);
    size_t iteration;
    for (iteration = 0; iteration < 512; ++iteration) {
        mulib_minish_t shell;
        mulib_minish_error_t error;
        mulib_minish_limits_t limits = { 64, 16, 8, 4, 4, 0 };
        mulib_minish_status_t status;
        mulib_strbuf_t output;
        char bytes[64];
        size_t i, length = iteration % (sizeof(bytes) + 1);
        mulib_minish_init(&shell);
        mulib_strbuf_init(&output);
        assert(mulib_minish_set_limits(&shell, &limits) == MULIB_MINISH_OK);
        mulib_minish_set_writer(&shell, capture, &output);
        for (i = 0; i < length; ++i) {
            random = random * UINT32_C(1664525) + UINT32_C(1013904223);
            bytes[i] = alphabet[(random >> 16) % sizeof(alphabet)]; /* includes NUL */
        }
        status = mulib_minish_execute(&shell, bytes, length, &error);
        assert(status == error.status);
        assert(error.offset <= length && error.command <= limits.commands && !error.system_error);
        assert(!shell.running);
        mulib_minish_free(&shell);
        mulib_strbuf_free(&output);
    }
}

#ifdef MULIB_MINISH_ENABLE_POSIX
static void test_process(const char *executable) {
    mulib_minish_t shell;
    mulib_minish_error_t error;
    mulib_strbuf_t command;
    const char *p;
    mulib_minish_init(&shell);
    mulib_strbuf_init(&command);
    assert(mulib_strbuf_putc(&command, '\''));
    for (p = executable; *p; ++p) {
        if (*p == '\'') assert(mulib_strbuf_append(&command, "'\\''"));
        else assert(mulib_strbuf_putc(&command, *p));
    }
    assert(mulib_strbuf_append(&command, "' --minish-child 'x;$(bad)' '' 'a b'"));
    assert(sh(&shell, command.data, &error) == MULIB_MINISH_NOT_FOUND);
    assert(mulib_minish_allow_process(&shell, 1) == MULIB_MINISH_OK);
    assert(sh(&shell, command.data, &error) == MULIB_MINISH_OK && shell.last_status == 29);
    assert(sh(&shell, "/mulib/missing/executable", &error) == MULIB_MINISH_PROCESS_ERROR);
    assert(error.system_error == ENOENT && shell.last_status == 29);
    mulib_minish_free(&shell);
    mulib_strbuf_free(&command);
}
#endif

int main(int argc, char **argv) {
    if (argc > 1 && !strcmp(argv[1], "--minish-child")) {
        assert(argc == 5 && !strcmp(argv[2], "x;$(bad)") && !strcmp(argv[3], "") && !strcmp(argv[4], "a b"));
        return 29;
    }
    test_bindings();
    test_binding_ruby_numbered_names();
    test_binding_native_symbol_bytes();
    test_vm();
    test_shell();
    test_shell_malformed_bytes();
#ifdef MULIB_MINISH_ENABLE_POSIX
    test_process(argv[0]);
#endif
    if (argc == 4) emit_bindings(argv[1], argv[2], argv[3]);
    else assert(argc == 1);
    return 0;
}
#endif /* MULIB_RUNTIME_BINDING_FIXTURE */

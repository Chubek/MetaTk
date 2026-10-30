#include "MetaVM.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(expr) do { ++checks; if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); exit(1); } } while (0)
#define OK(expr) CHECK((expr) == MVM_OK)
static mvm_module *compile(const char *source) {
    mvm_module *m = NULL;
    mvm_error e;
    mvm_status s = mvm_compile(source, strlen(source), 0, &m, &e);
    if (s != MVM_OK) fprintf(stderr, "line %zu: %s\n", e.line, e.message);
    OK(s); CHECK(m != NULL);
    return m;
}
static mvm_vm *new_vm(void) {
    mvm_error e;
    mvm_vm *vm = mvm_create(NULL, &e);
    CHECK(vm != NULL); CHECK(e.status == MVM_OK);
    return vm;
}
static mvm_value run(mvm_vm *vm, const mvm_module *m, mvm_engine engine) {
    mvm_error e;
    mvm_value v;
    mvm_status s = mvm_run(vm, m, 0, NULL, 0, engine, &v, &e);
    if (s != MVM_OK) fprintf(stderr, "run %zu:%zu: %s\n", e.function, e.instruction, e.message);
    OK(s);
    return v;
}
static const char factorial[] =
    ".function main 0 2\nconst r0 10\ncall r1 fact r0\nret r1\n.end\n"
    ".function fact 1 4\nconst r1 1\nle r2 r0 r1\njz r2 recurse\nret r1\n"
    "recurse:\nsub r0 r0 r1\ncall r3 fact r0\nadd r0 r0 r1\nmul r3 r0 r3\nret r3\n.end\n";

static void test_execution(void) {
    mvm_module *m = compile(factorial);
    mvm_vm *vm = new_vm();
    mvm_value a, b;
    mvm_error e;
    mvm_function_info info;
    uint32_t index;
    uint64_t steps;
    a = run(vm, m, MVM_ENGINE_BYTECODE); steps = mvm_steps(vm);
    b = run(vm, m, MVM_ENGINE_THREADED);
    CHECK(a.kind == MVM_INT && a.as.integer == 3628800);
    CHECK(mvm_value_equal(a, b)); CHECK(steps == mvm_steps(vm) && steps > 50);
    CHECK(mvm_module_function_count(m) == 2);
    OK(mvm_module_find(m, "fact", &index)); CHECK(index == 1);
    CHECK(mvm_module_find(m, "missing", &index) == MVM_NOT_FOUND);
    OK(mvm_module_function(m, 1, &info)); CHECK(info.arity == 1 && info.registers == 4);
    a = mvm_int(6);
    OK(mvm_run_bytecode(vm, m, 1, &a, 1, &a, &e)); /* aliased result/argument */
    CHECK(a.kind == MVM_INT && a.as.integer == 720);
    CHECK(mvm_run_threaded(vm, m, 1, NULL, 0, &b, &e) == MVM_INVALID);
    CHECK(b.kind == MVM_NIL);
    mvm_module_destroy(m); mvm_destroy(vm);
}
typedef struct arithmetic_case { mvm_opcode op; int64_t a, b, value; mvm_status status; } arithmetic_case;
static void test_arithmetic(void) {
    static const arithmetic_case cases[] = {
        {MVM_OP_ADD, INT64_MAX, 1, INT64_MIN, MVM_OK},
        {MVM_OP_SUB, INT64_MIN, 1, INT64_MAX, MVM_OK},
        {MVM_OP_MUL, INT64_MIN, -1, INT64_MIN, MVM_OK},
        {MVM_OP_DIV, INT64_MIN, -1, INT64_MIN, MVM_OK},
        {MVM_OP_MOD, INT64_MIN, -1, 0, MVM_OK},
        {MVM_OP_DIV, -7, 3, -2, MVM_OK}, {MVM_OP_MOD, -7, 3, -1, MVM_OK},
        {MVM_OP_DIV, 9, 0, 0, MVM_DIV_ZERO}, {MVM_OP_MOD, 9, 0, 0, MVM_DIV_ZERO},
        {MVM_OP_AND, 10, 6, 2, MVM_OK}, {MVM_OP_OR, 10, 6, 14, MVM_OK},
        {MVM_OP_XOR, 10, 6, 12, MVM_OK}, {MVM_OP_SHL, 1, 63, INT64_MIN, MVM_OK},
        {MVM_OP_SHR, INT64_MIN, 63, -1, MVM_OK}, {MVM_OP_SHR, -8, 2, -2, MVM_OK},
        {MVM_OP_SHR, -8, 0, -8, MVM_OK}, {MVM_OP_SHR, INT64_MAX, 63, 0, MVM_OK},
        {MVM_OP_SHL, 1, 64, 0, MVM_BOUNDS}, {MVM_OP_SHR, 1, -1, 0, MVM_BOUNDS},
        {MVM_OP_EQ, -2, -2, 1, MVM_OK}, {MVM_OP_EQ, -2, 2, 0, MVM_OK},
        {MVM_OP_LT, INT64_MIN, INT64_MAX, 1, MVM_OK}, {MVM_OP_LE, 3, 3, 1, MVM_OK}
    };
    size_t i;
    unsigned engine;
    mvm_vm *vm = new_vm();
    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        const arithmetic_case *c = &cases[i];
        char source[256];
        mvm_module *m;
        mvm_error e;
        int64_t folded = 0;
        CHECK(mvm_fold_integer_operation(mvm_opcode_name(c->op), c->a, c->b, &folded) == c->status);
        if (c->status == MVM_OK) CHECK(folded == c->value);
        (void)snprintf(source, sizeof(source), ".function main 0 3\nconst r0 %" PRId64
            "\nconst r1 %" PRId64 "\n%s r2 r0 r1\nret r2\n.end\n", c->a, c->b, mvm_opcode_name(c->op));
        m = compile(source);
        for (engine = 0; engine < 2; ++engine) {
            mvm_value result;
            CHECK(mvm_run(vm, m, 0, NULL, 0, (mvm_engine)engine, &result, &e) == c->status);
            if (c->status == MVM_OK) {
                if (c->op >= MVM_OP_EQ) CHECK(result.kind == MVM_BOOL && result.as.boolean == c->value);
                else CHECK(result.kind == MVM_INT && result.as.integer == c->value);
            } else {
                CHECK(result.kind == MVM_NIL);
                CHECK(e.function == 0 && e.instruction == 2 && e.status == c->status);
            }
        }
        {
            size_t changes;
            mvm_value result;
            OK(mvm_optimize(m, &changes, &e));
            CHECK(changes == (size_t)(c->status == MVM_OK));
            CHECK(mvm_run_threaded(vm, m, 0, NULL, 0, &result, &e) == c->status);
            if (c->status == MVM_OK) {
                if (c->op >= MVM_OP_EQ) CHECK(result.kind == MVM_BOOL && result.as.boolean == c->value);
                else CHECK(result.kind == MVM_INT && result.as.integer == c->value);
            }
        }
        mvm_module_destroy(m);
    }
    mvm_destroy(vm);
}
static void test_control_and_objects(void) {
    mvm_module *m = compile(
        ".function main 0 7\n"
        "const r0 2\narray r1 r0\nconst r2 0\nconst r3 40\nset r1 r2 r3\n"
        "get r4 r1 r2\nlen r5 r1\nadd r4 r4 r5\nmov r6 r4\n"
        "bool r0 0\nnot r0 r0\njz r0 bad\nnil r0\njz r0 good\n"
        "bad:\nconst r6 -1\njmp end\ngood:\nnop\nend:\nret r6\n.end\n");
    mvm_vm *vm = new_vm();
    unsigned engine;
    for (engine = 0; engine < 2; ++engine) {
        mvm_value v = run(vm, m, (mvm_engine)engine);
        CHECK(v.kind == MVM_INT && v.as.integer == 42);
    }
    CHECK(mvm_collect(vm) == 2);
    CHECK(!mvm_value_truthy(mvm_nil())); CHECK(!mvm_value_truthy(mvm_bool(0)));
    CHECK(mvm_value_truthy(mvm_int(0)));
    mvm_module_destroy(m); mvm_destroy(vm);
}
static void test_limits_and_traps(void) {
    static const struct { const char *body; mvm_status status; } cases[] = {
        {"nil r0\nconst r1 1\nadd r2 r0 r1\nret r2", MVM_TYPE},
        {"const r0 -1\narray r1 r0\nret r1", MVM_BOUNDS},
        {"const r0 1\narray r1 r0\nget r2 r1 r0\nret r2", MVM_BOUNDS},
        {"const r0 1\narray r1 r0\nset r1 r0 r0\nret r1", MVM_BOUNDS},
        {"const r0 0\nlen r1 r0\nret r1", MVM_TYPE},
        {"const r0 99999999\narray r1 r0\nret r1", MVM_LIMIT},
        {"loop:\njmp loop", MVM_LIMIT},
        {"call r0 main r0\nret r0", MVM_LIMIT}
    };
    mvm_limits limits = mvm_default_limits();
    mvm_error e;
    mvm_vm *vm;
    size_t i;
    unsigned engine;
    limits.max_steps = 50; limits.max_frames = 8; limits.max_registers = 24;
    vm = mvm_create(&limits, &e); CHECK(vm != NULL);
    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        char source[512];
        mvm_module *m;
        (void)snprintf(source, sizeof(source), ".function main 0 3\n%s\n.end", cases[i].body);
        m = compile(source);
        for (engine = 0; engine < 2; ++engine) {
            mvm_value v;
            CHECK(mvm_run(vm, m, 0, NULL, 0, (mvm_engine)engine, &v, &e) == cases[i].status);
            CHECK(e.message[0] && e.status == cases[i].status && v.kind == MVM_NIL);
            CHECK(mvm_steps(vm) <= 50);
        }
        mvm_module_destroy(m);
    }
    {
        mvm_module *m = compile(".function main 0 1\nconst r0 42\nret r0\n.end");
        CHECK(run(vm, m, MVM_ENGINE_BYTECODE).as.integer == 42); /* reusable after traps */
        mvm_module_destroy(m);
    }
    mvm_destroy(vm);
    limits.max_steps = 0; CHECK(mvm_create(&limits, &e) == NULL && e.status == MVM_INVALID);
}
static void test_gc(void) {
    mvm_vm *vm = new_vm(), *foreign = new_vm();
    mvm_value a, b, s, other, value;
    mvm_root *root;
    const unsigned char *bytes;
    size_t n;
    mvm_object_kind kind;
    OK(mvm_array_new(vm, 2, &a)); OK(mvm_pin(vm, a, &root));
    OK(mvm_array_new(vm, 1, &b)); OK(mvm_array_set(vm, a, 0, b)); OK(mvm_array_set(vm, b, 0, a));
    OK(mvm_string_new(vm, "a\0b", 3, &s)); OK(mvm_array_set(vm, a, 1, s));
    OK(mvm_string_new(foreign, "foreign", 7, &other));
    CHECK(mvm_array_set(vm, a, 0, other) == MVM_INVALID);
    CHECK(mvm_object_length(vm, other, &n) == MVM_TYPE);
    CHECK(mvm_collect(vm) == 0); CHECK(mvm_memory_usage(vm).objects == 3);
    OK(mvm_object_type(vm, s, &kind)); CHECK(kind == MVM_STRING);
    OK(mvm_string_data(vm, s, &bytes, &n)); CHECK(n == 3 && !memcmp(bytes, "a\0b", 3) && bytes[3] == 0);
    OK(mvm_array_get(vm, mvm_root_value(root), 1, &value)); CHECK(mvm_value_equal(value, s));
    CHECK(mvm_object_describe(MVM_ARRAY)->traced_elements == 1);
    CHECK(mvm_object_describe((mvm_object_kind)99) == NULL);
    CHECK(mvm_array_get(vm, a, 2, &value) == MVM_BOUNDS);
    mvm_unpin(root); CHECK(mvm_collect(vm) == 3); CHECK(mvm_memory_usage(vm).heap_bytes == 0);
    OK(mvm_array_new(vm, 0, &a)); OK(mvm_object_length(vm, a, &n)); CHECK(n == 0);
    CHECK(mvm_array_get(vm, a, 0, &value) == MVM_BOUNDS);
    CHECK(mvm_collect(vm) == 1);
    CHECK(mvm_array_new(vm, SIZE_MAX, &value) == MVM_LIMIT);
    CHECK(mvm_string_new(vm, "", SIZE_MAX, &value) == MVM_LIMIT);
    mvm_destroy(foreign); mvm_destroy(vm);
    {
        mvm_limits limits = mvm_default_limits();
        mvm_error e;
        limits.max_heap_bytes = 160;
        vm = mvm_create(&limits, &e); CHECK(vm != NULL);
        OK(mvm_string_new(vm, "123456789", 9, &a)); OK(mvm_pin(vm, a, &root));
        OK(mvm_string_new(vm, "abcdefghi", 9, &b));
        OK(mvm_string_new(vm, "123456789", 9, &s)); /* collects unpinned b */
        OK(mvm_string_data(vm, mvm_root_value(root), &bytes, &n)); CHECK(n == 9);
        CHECK(mvm_memory_usage(vm).objects == 2);
        mvm_unpin(root); mvm_destroy(vm);
    }
}
typedef struct host_context { const mvm_module *module; unsigned calls; } host_context;
static mvm_status native_echo(mvm_vm *vm, const mvm_value *args, size_t count,
    mvm_value *out, void *user, mvm_error *error) {
    host_context *ctx = user;
    mvm_value temporary;
    mvm_error local;
    (void)error;
    CHECK(count == 1 && args[0].kind == MVM_OBJECT);
    CHECK(mvm_run_bytecode(vm, ctx->module, 0, NULL, 0, &temporary, &local) == MVM_BUSY);
    CHECK(mvm_register_native(vm, "forbidden", 1, native_echo, user, &local) == MVM_BUSY);
    (void)mvm_collect(vm); /* argument array is still rooted by its frame */
    OK(mvm_array_get(vm, args[0], 0, &temporary)); CHECK(temporary.as.integer == 42);
    *out = args[0]; ++ctx->calls;
    return MVM_OK;
}
static mvm_status native_fail(mvm_vm *vm, const mvm_value *args, size_t n,
    mvm_value *out, void *user, mvm_error *e) {
    (void)vm; (void)args; (void)n; (void)out; (void)user;
    mvm_error_clear(e);
    if (e) (void)snprintf(e->message, sizeof(e->message), "deliberate native failure");
    return MVM_BOUNDS;
}
static void test_host(void) {
    const char *source = ".import echo 1\n.function main 0 4\nconst r0 1\narray r0 r0\n"
        "const r1 0\nconst r2 42\nset r0 r1 r2\nhost r3 echo r0\nret r3\n.end";
    mvm_module *m = compile(source);
    mvm_vm *vm = new_vm();
    host_context ctx;
    mvm_error e;
    mvm_value v, a;
    unsigned engine;
    const char *name;
    uint32_t arity;
    {
        mvm_buffer binary = {0};
        mvm_module *decoded = NULL;
        OK(mvm_serialize(m, &binary, &e));
        OK(mvm_deserialize(binary.data, binary.size, 0, &decoded, &e));
        mvm_module_destroy(m); m = decoded;
        mvm_buffer_dispose(&binary);
    }
    ctx.module = m; ctx.calls = 0;
    CHECK(mvm_module_import_count(m) == 1);
    OK(mvm_module_import(m, 0, &name, &arity)); CHECK(!strcmp(name, "echo") && arity == 1);
    CHECK(mvm_run_bytecode(vm, m, 0, NULL, 0, &v, &e) == MVM_NOT_FOUND);
    OK(mvm_register_native(vm, "echo", 1, native_echo, &ctx, &e));
    CHECK(mvm_register_native(vm, "echo", 1, native_echo, &ctx, &e) == MVM_INVALID);
    for (engine = 0; engine < 2; ++engine) {
        v = run(vm, m, (mvm_engine)engine);
        (void)mvm_collect(vm); /* last result retained */
        OK(mvm_array_get(vm, v, 0, &a)); CHECK(a.kind == MVM_INT && a.as.integer == 42);
    }
    CHECK(ctx.calls == 2);
    mvm_module_destroy(m); mvm_destroy(vm);
    m = compile(".import fail 0\n.function main 0 1\nhost r0 fail r1\nret r0\n.end");
    vm = new_vm(); OK(mvm_register_native(vm, "fail", 0, native_fail, NULL, &e));
    CHECK(mvm_run_threaded(vm, m, 0, NULL, 0, &v, &e) == MVM_HOST_ERROR);
    CHECK(e.status == MVM_HOST_ERROR && strstr(e.message, "deliberate") != NULL);
    CHECK(e.function == 0 && e.instruction == 0);
    mvm_module_destroy(m); mvm_destroy(vm);
}
static void test_invalid_source(void) {
    static const char *const cases[] = {
        "", ".end", "ret r0", ".function f 0 1\n.end", ".function f 2 1\n.end",
        ".function f 0 1\nconst r0 1\n.end", ".function f 0 1\nret r1\n.end",
        ".function f 0 1\nbool r0 2\nret r0\n.end", ".function f 0 1\nwat r0\n.end",
        ".function f 0 1\nconst r0 9223372036854775808\nret r0\n.end",
        ".function f 0 1\nconst r0 -9223372036854775809\nret r0\n.end",
        ".function f 0 1\njmp missing\n.end", ".function f 0 1\njmp end\nend:\n.end",
        ".function f 0 1\na:\na:\nret r0\n.end", ".function f 0 1\nret r0",
        ".function f 0 1\n.function g 0 1\nret r0\n.end",
        ".function f 0 1\ncall r0 unknown r0\nret r0\n.end",
        ".function f 0 1\nhost r0 unknown r0\nret r0\n.end",
        ".function f 0 1\nret r0 extra\n.end", ".function f 0 1\nconst r4294967296 0\nret r0\n.end",
        ".import x 1\n.import x 1", ".function f 0 1\nret r0\n.end\n.function f 0 1\nret r0\n.end",
        ".function f 0 1\nadd r0 r0 r2\nret r0\n.end",
        ".function f 1 1\ncall r0 f r1\nret r0\n.end"
    };
    size_t i;
    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        mvm_module *m = NULL;
        mvm_error e;
        CHECK(mvm_compile(cases[i], strlen(cases[i]), 0, &m, &e) != MVM_OK);
        CHECK(m == NULL && e.status != MVM_OK && e.message[0] && e.line > 0);
    }
    {
        mvm_module *m = NULL;
        mvm_error e;
        const char source[] = ".function f 0 1\nconst r0 1\nret r2\n.end";
        CHECK(mvm_compile(source, strlen(source), 0, &m, &e) == MVM_VERIFY); CHECK(e.line == 3);
        CHECK(mvm_compile("a\0b", 3, 0, &m, &e) == MVM_FORMAT);
        CHECK(mvm_compile(source, strlen(source), 1, &m, &e) == MVM_LIMIT);
    }
}
static void test_serialization(void) {
    mvm_module *m = compile(factorial), *copy = NULL;
    mvm_buffer b = {0}, b2 = {0};
    mvm_error e;
    mvm_vm *vm = new_vm();
    size_t i;
    OK(mvm_serialize(m, &b, &e)); CHECK(b.size > 16 && !memcmp(b.data, "MVM\0\1\0\0\0", 8));
    OK(mvm_deserialize(b.data, b.size, 0, &copy, &e));
    CHECK(run(vm, copy, MVM_ENGINE_THREADED).as.integer == 3628800);
    OK(mvm_serialize(copy, &b2, &e)); CHECK(b.size == b2.size && !memcmp(b.data, b2.data, b.size));
    mvm_module_destroy(copy);
    for (i = 0; i < b.size; ++i) {
        copy = NULL;
        CHECK(mvm_deserialize(b.data, i, 0, &copy, &e) != MVM_OK); CHECK(copy == NULL);
    }
    CHECK(mvm_deserialize(b.data, b.size, b.size - 1, &copy, &e) == MVM_LIMIT);
    b.data[4] = 2; CHECK(mvm_deserialize(b.data, b.size, 0, &copy, &e) == MVM_FORMAT); b.data[4] = 1;
    OK(mvm_buffer_append(&b, "x", 1)); CHECK(mvm_deserialize(b.data, b.size, 0, &copy, &e) == MVM_FORMAT);
    mvm_buffer_dispose(&b); mvm_buffer_dispose(&b2);
    mvm_module_destroy(m); mvm_destroy(vm);
    {
        static const unsigned char expected[] = {
            'M','V','M',0, 1,0,0,0, 0,0,0,0, 1,0,0,0,
            1,0,0,0,'f', 0,0,0,0, 1,0,0,0, 2,0,0,0,
            1,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 255,255,255,255,255,255,255,255,
            23,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,0,0,0,0
        };
        m = compile(".function f 0 1\nconst r0 -1\nret r0\n.end");
        OK(mvm_serialize(m, &b, &e));
        CHECK(b.size == sizeof(expected) && !memcmp(b.data, expected, sizeof(expected)));
        mvm_buffer_dispose(&b); mvm_module_destroy(m);
    }
}
static void test_rewrite_cache_io(void) {
    mvm_module *m = compile(".function f 0 1\nconst r0 42\nmov r0 r0\njmp end\nend:\nret r0\n.end");
    mvm_vm *vm = new_vm();
    mvm_cache *cache = mvm_cache_create(2);
    mvm_error e;
    mvm_instruction bad = {MVM_OP_JMP, 0, 0, 0, 99};
    mvm_instruction replacement = {MVM_OP_CONST, 0, 0, 0, -1};
    mvm_module *loaded;
    mvm_buffer file = {0};
    char *path;
    size_t changes;
    uint64_t steps;
    CHECK(cache != NULL); CHECK(mvm_cache_create(0) == NULL);
    CHECK(run(vm, m, MVM_ENGINE_BYTECODE).as.integer == 42); steps = mvm_steps(vm);
    CHECK(mvm_module_replace(m, 0, 2, bad, &e) == MVM_VERIFY);
    OK(mvm_optimize(m, &changes, &e)); CHECK(changes == 2);
    CHECK(run(vm, m, MVM_ENGINE_THREADED).as.integer == 42 && mvm_steps(vm) == steps);
    OK(mvm_optimize(m, &changes, &e)); CHECK(changes == 0);
    OK(mvm_cache_put(cache, "a", m, &e)); OK(mvm_cache_put(cache, "b", m, &e));
    CHECK(mvm_cache_get(cache, "a") != NULL); OK(mvm_cache_put(cache, "c", m, &e));
    CHECK(mvm_cache_get(cache, "b") == NULL);
    OK(mvm_module_replace(m, 0, 0, replacement, &e));
    CHECK(run(vm, mvm_cache_get(cache, "a"), MVM_ENGINE_BYTECODE).as.integer == 42);
    OK(mvm_cache_put(cache, "a", m, &e));
    CHECK(run(vm, mvm_cache_get(cache, "a"), MVM_ENGINE_BYTECODE).as.integer == -1);
    OK(mvm_save_file("machinery-test.bin", m, &e));
    OK(mvm_load_file("machinery-test.bin", 0, &loaded, &e));
    CHECK(run(vm, loaded, MVM_ENGINE_THREADED).as.integer == -1); mvm_module_destroy(loaded);
    CHECK(mvm_read_file("machinery-test.bin", 1, &file, &e) == MVM_LIMIT && file.size == 0);
    CHECK(remove("machinery-test.bin") == 0);
    OK(mvm_write_file("machinery-test.mvm", factorial, strlen(factorial), &e));
    OK(mvm_load_file("machinery-test.mvm", 0, &loaded, &e));
    CHECK(run(vm, loaded, MVM_ENGINE_BYTECODE).as.integer == 3628800); mvm_module_destroy(loaded);
    CHECK(remove("machinery-test.mvm") == 0);
    CHECK(mvm_load_file("machinery-test.mvm", 0, &loaded, &e) == MVM_IO && loaded == NULL);
    OK(mvm_path_join("a", "b", &path)); CHECK(!strcmp(path, "a/b")); free(path);
    OK(mvm_path_join("a/", "b", &path)); CHECK(!strcmp(path, "a/b")); free(path);
    OK(mvm_path_join("a", "/b", &path)); CHECK(!strcmp(path, "/b")); free(path);
    OK(mvm_path_join("", "", &path)); CHECK(!strcmp(path, "")); free(path);
    mvm_cache_clear(cache); CHECK(mvm_cache_get(cache, "a") == NULL);
    mvm_cache_destroy(cache); mvm_module_destroy(m); mvm_destroy(vm); mvm_buffer_dispose(&file);
}
static void test_optimizer_control_flow(void) {
    mvm_error e;
    size_t changes;
    mvm_vm *vm = new_vm();
    mvm_module *m = compile(
        ".function f 0 3\nconst r0 40\nconst r1 2\njmp join\n"
        "const r0 100\nconst r1 200\njoin:\nadd r2 r0 r1\nret r2\n.end");
    OK(mvm_optimize(m, &changes, &e)); CHECK(changes == 0);
    CHECK(run(vm, m, MVM_ENGINE_BYTECODE).as.integer == 42);
    mvm_module_destroy(m);
    m = compile(".function f 0 3\nconst r0 40\njmp second\nconst r0 100\n"
        "second:\nconst r1 2\nadd r2 r0 r1\nret r2\n.end");
    OK(mvm_optimize(m, &changes, &e)); CHECK(changes == 0);
    CHECK(run(vm, m, MVM_ENGINE_THREADED).as.integer == 42);
    mvm_module_destroy(m); mvm_destroy(vm);
}
static uint32_t random_next(uint32_t *state) { *state = *state * UINT32_C(1664525) + UINT32_C(1013904223); return *state; }
static void test_mutations(void) {
    mvm_module *m = compile(factorial);
    mvm_buffer b = {0};
    mvm_error e;
    mvm_limits limits = mvm_default_limits();
    mvm_vm *vm;
    unsigned iteration;
    uint32_t state = 0x12345678;
    limits.max_steps = 1000; limits.max_frames = 16; limits.max_registers = 1024; limits.max_heap_bytes = 4096;
    vm = mvm_create(&limits, &e); CHECK(vm != NULL);
    OK(mvm_serialize(m, &b, &e));
    for (iteration = 0; iteration < 2000; ++iteration) {
        mvm_module *copy = NULL;
        size_t index = random_next(&state) % b.size;
        unsigned char old = b.data[index];
        b.data[index] ^= (unsigned char)(1u << (random_next(&state) % 8));
        if (mvm_deserialize(b.data, b.size, 0, &copy, &e) == MVM_OK) {
            mvm_value a, c;
            mvm_status sa = mvm_run_bytecode(vm, copy, 0, NULL, 0, &a, &e);
            mvm_status sc = mvm_run_threaded(vm, copy, 0, NULL, 0, &c, &e);
            CHECK(sa == sc);
            if (sa == MVM_OK && a.kind != MVM_OBJECT) CHECK(mvm_value_equal(a, c));
            mvm_module_destroy(copy);
        } else CHECK(copy == NULL);
        b.data[index] = old;
    }
    for (iteration = 0; iteration < 1000; ++iteration) {
        char text[128];
        size_t i, n = random_next(&state) % sizeof(text);
        mvm_module *copy = NULL;
        for (i = 0; i < n; ++i) text[i] = (char)(random_next(&state) >> 24);
        if (mvm_compile(text, n, 0, &copy, &e) == MVM_OK) mvm_module_destroy(copy);
        else CHECK(copy == NULL);
    }
    mvm_buffer_dispose(&b); mvm_module_destroy(m); mvm_destroy(vm);
}
static void test_reader(void) {
    mvm_reader r;
    uint32_t a;
    uint64_t b;
    unsigned char bytes[] = {1,2,3,4,5,6,7,8};
    OK(mvm_reader_init(&r, bytes, sizeof(bytes)));
    OK(mvm_reader_u32(&r, &a)); CHECK(a == UINT32_C(0x04030201));
    CHECK(mvm_reader_u64(&r, &b) == MVM_FORMAT && r.offset == 4);
    OK(mvm_reader_init(&r, bytes, sizeof(bytes)));
    OK(mvm_reader_u64(&r, &b)); CHECK(b == UINT64_C(0x0807060504030201));
    OK(mvm_reader_init(&r, NULL, 0)); OK(mvm_reader_bytes(&r, NULL, 0));
}
int main(void) {
    test_execution(); test_arithmetic(); test_control_and_objects();
    test_limits_and_traps(); test_gc(); test_host(); test_invalid_source();
    test_serialization(); test_rewrite_cache_io(); test_optimizer_control_flow(); test_mutations(); test_reader();
    printf("MetaVM Machinery: %u checks passed\n", checks);
    return 0;
}

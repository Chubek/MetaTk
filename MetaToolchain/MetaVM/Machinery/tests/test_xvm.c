#include "MetaVM.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(x) do { ++checks; if (!(x)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); exit(1); } } while (0)
static mvm_module *compile(const char *source) {
    mvm_module *m = NULL;
    mvm_error e;
    CHECK(mvm_compile(source, strlen(source), 0, &m, &e) == MVM_OK);
    return m;
}
static mvm_status gc_host(mvm_vm *vm, const mvm_value *args, size_t n, mvm_value *out, void *user, mvm_error *e) {
    mvm_value temporary, ignored;
    mvm_xvm_options options = mvm_xvm_default_options();
    (void)n;
    CHECK(mvm_run_default(vm, user, 0, NULL, 0, &ignored, e) == MVM_BUSY);
    CHECK(mvm_xvm_configure(vm, &options, e) == MVM_BUSY);
    CHECK(mvm_string_new(vm, "collect me", 10, &temporary) == MVM_OK);
    CHECK(mvm_collect(vm) >= 1);
    *out = args[0];
    return MVM_OK;
}
static void test_options(void) {
    mvm_error e;
    mvm_vm *vm = mvm_create(NULL, &e);
    mvm_module *m = compile(".function main 0 1\nconst r0 42\nret r0\n.end\n");
    mvm_value result;
    mvm_xvm_options options = mvm_xvm_default_options(), invalid;
    mvm_xvm_statistics stats;
    int native;
    CHECK(vm != NULL && MVM_ENGINE_DEFAULT == MVM_ENGINE_XVM);
    CHECK(mvm_run_default(vm, m, 0, NULL, 0, &result, &e) == MVM_OK && result.as.integer == 42);
    stats = mvm_xvm_stats(vm); native = stats.native;
    CHECK(stats.instructions == 2 && stats.blocks == 1 && stats.executed_blocks == 1);
    CHECK(stats.plan_bytes > 0 && (stats.native_bytes > 0) == native);
    invalid = options; invalid.max_plan_bytes = 0;
    CHECK(mvm_xvm_configure(vm, &invalid, &e) == MVM_INVALID);
    invalid = options; invalid.native_mode = (mvm_xvm_native_mode)99;
    CHECK(mvm_xvm_configure(vm, &invalid, &e) == MVM_INVALID);
    options.native_mode = MVM_XVM_NATIVE_OFF; options.max_native_bytes = 1;
    CHECK(mvm_xvm_configure(vm, &options, &e) == MVM_OK);
    CHECK(mvm_run_xvm(vm, m, 0, NULL, 0, &result, &e) == MVM_OK && result.as.integer == 42);
    CHECK(!mvm_xvm_stats(vm).native && !mvm_xvm_stats(vm).native_bytes);
    options.max_plan_bytes = 1;
    CHECK(mvm_xvm_configure(vm, &options, &e) == MVM_OK);
    CHECK(mvm_run_xvm(vm, m, 0, NULL, 0, &result, &e) == MVM_LIMIT && result.kind == MVM_NIL);
    options = mvm_xvm_default_options(); options.native_mode = MVM_XVM_NATIVE_REQUIRED;
    CHECK(mvm_xvm_configure(vm, &options, &e) == MVM_OK);
    CHECK(mvm_run_xvm(vm, m, 0, NULL, 0, &result, &e) == (native ? MVM_OK : MVM_NOT_FOUND));
    if (native) {
        options.max_native_bytes = 1;
        CHECK(mvm_xvm_configure(vm, &options, &e) == MVM_OK);
        CHECK(mvm_run_xvm(vm, m, 0, NULL, 0, &result, &e) == MVM_LIMIT);
        CHECK(mvm_run_xvm(vm, m, 0, NULL, 0, &result, &e) == MVM_LIMIT);
    }
    CHECK(mvm_run_bytecode(vm, m, 0, NULL, 0, &result, &e) == MVM_OK);
    CHECK(!mvm_xvm_stats(vm).blocks);
    CHECK(mvm_xvm_stats(NULL).blocks == 0);
    mvm_destroy(vm); mvm_module_destroy(m);
}
static void test_differential(void) {
    static const char *programs[] = {
        ".function main 0 2\nconst r0 0\njz r0 bad\nconst r1 42\nret r1\nbad:\nconst r1 -1\nret r1\n.end",
        ".function main 0 3\nconst r0 10\nconst r1 1\nloop:\nsub r0 r0 r1\nle r2 r0 r1\njz r2 loop\nret r0\n.end",
        ".function main 0 2\nconst r0 41\ncall r1 f r0\nret r1\n.end\n.function f 1 2\nconst r1 1\nadd r0 r0 r1\nret r0\n.end",
        ".function main 0 3\nconst r0 1\nconst r1 0\ndiv r2 r0 r1\nret r2\n.end",
        ".function main 0 2\nconst r0 1\narray r1 r0\nget r0 r1 r0\nret r0\n.end"
    };
    size_t p;
    for (p = 0; p < sizeof(programs) / sizeof(*programs); ++p) {
        mvm_module *m = compile(programs[p]);
        mvm_vm *vm;
        mvm_error expected_error, actual_error;
        mvm_value expected, actual;
        uint64_t steps;
        unsigned mode;
        mvm_status status;
        vm = mvm_create(NULL, &actual_error); CHECK(vm != NULL);
        status = mvm_run_bytecode(vm, m, 0, NULL, 0, &expected, &expected_error); steps = mvm_steps(vm);
        for (mode = 0; mode < 2; ++mode) {
            mvm_xvm_options options = mvm_xvm_default_options();
            options.native_mode = mode ? MVM_XVM_NATIVE_AUTO : MVM_XVM_NATIVE_OFF;
            CHECK(mvm_xvm_configure(vm, &options, &actual_error) == MVM_OK);
            CHECK(mvm_run_xvm(vm, m, 0, NULL, 0, &actual, &actual_error) == status);
            CHECK(mvm_value_equal(expected, actual)); CHECK(mvm_steps(vm) == steps);
            CHECK(actual_error.status == expected_error.status);
            CHECK(actual_error.function == expected_error.function && actual_error.instruction == expected_error.instruction);
            CHECK(!strcmp(actual_error.message, expected_error.message));
        }
        mvm_destroy(vm); mvm_module_destroy(m);
    }
}
static void test_boundaries(void) {
    mvm_module *m = compile(".function main 0 1\nconst r0 42\nret r0\n.end");
    unsigned engine;
    for (engine = 0; engine < 3; ++engine) {
        unsigned fuel;
        for (fuel = 1; fuel <= 2; ++fuel) {
            mvm_error e; mvm_value value;
            mvm_limits limits = mvm_default_limits(); mvm_vm *vm;
            limits.max_steps = fuel; vm = mvm_create(&limits, &e); CHECK(vm != NULL);
            CHECK(mvm_run(vm, m, 0, NULL, 0, (mvm_engine)engine, &value, &e) == (fuel == 1 ? MVM_LIMIT : MVM_OK));
            CHECK(mvm_steps(vm) == fuel);
            if (fuel == 1) CHECK(e.function == 0 && e.instruction == 1 && value.kind == MVM_NIL);
            mvm_destroy(vm);
        }
    }
    mvm_module_destroy(m);
    m = compile(".import collect 1\n.function main 0 2\nconst r0 1\narray r1 r0\nhost r0 collect r1\nret r0\n.end");
    for (engine = 0; engine < 3; ++engine) {
        mvm_error e; mvm_value value; mvm_object_kind kind;
        mvm_vm *vm = mvm_create(NULL, &e); CHECK(vm != NULL);
        CHECK(mvm_register_native(vm, "collect", 1, gc_host, m, &e) == MVM_OK);
        CHECK(mvm_run(vm, m, 0, NULL, 0, (mvm_engine)engine, &value, &e) == MVM_OK);
        CHECK(mvm_object_type(vm, value, &kind) == MVM_OK && kind == MVM_ARRAY);
        CHECK(mvm_collect(vm) == 0);
        mvm_destroy(vm);
    }
    mvm_module_destroy(m);
    m = compile(".function main 1 1\nret r0\n.end");
    {
        mvm_error e; mvm_value value = mvm_int(42);
        mvm_vm *vm = mvm_create(NULL, &e); CHECK(vm != NULL);
        CHECK(mvm_run_default(vm, m, 0, &value, 1, &value, &e) == MVM_OK && value.as.integer == 42);
        mvm_destroy(vm);
    }
    mvm_module_destroy(m);
}
int main(void) {
    test_options(); test_differential(); test_boundaries();
    printf("XVM: %u checks passed\n", checks);
    return 0;
}

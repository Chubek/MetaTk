/* Standalone: cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude tests/test_compiler_ir.c */
#define MULIB_IMPL_MIR
#define MULIB_IMPL_MINIVM
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_CMACHINE
#define MULIB_IMPL_VMWEAVE
#define MULIB_IMPL_CCWEAVE
#define MULIB_IMPL_ISCHED
#define MULIB_IMPL_MUBURG
#define MULIB_IMPL_EGRAPH
#define MULIB_IMPL_REWRITE
#include "mulib-mir.h"
#include "mulib-minivm.h"
#include "mulib-strbuf.h"
#include "mulib-cmachine.h"
#include "mulib-vmweave.h"
#include "mulib-ccweave.h"
#include "mulib-isched.h"
#include "mulib-muburg.h"
#include "mulib-egraph.h"
#include "mulib-rewrite.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define N MULIB_MIR_NONE
#define C(v) { MULIB_MIR_CONST, MULIB_MIR_I64, N, N, (v) }
#define P(v) { MULIB_MIR_PARAM, MULIB_MIR_I64, N, N, (v) }
#define B(op,t,a,b) { (op), (t), (a), (b), 0 }
#define R(a) B(MULIB_MIR_RETURN, MULIB_MIR_VOID, a, N)

static mulib_minivm_result_t run(const mulib_mir_program_t *p, const int64_t *args, size_t argc) {
    mulib_cmachine_t m = { 0 };
    mulib_minivm_result_t r;
    assert(mulib_cmachine_lower(p, args, argc, 1024, 8192, &m, NULL) == MULIB_MIR_OK);
    (void)mulib_cmachine_run(&m, NULL, &r);
    mulib_cmachine_free(&m); assert(!m.code); return r;
}
static void test_mir(void) {
    mulib_mir_instruction_t code[] = { P(0), C(2), B(MULIB_MIR_MUL,MULIB_MIR_I64,0,1), R(2) };
    mulib_mir_program_t p = { code, 4, 1 };
    mulib_mir_diagnostic_t d;
    mulib_mir_buffer_t b = { 0 };
    int64_t out = 19;
    assert(mulib_mir_validate(&p, 4, &d) == MULIB_MIR_OK);
    assert(mulib_mir_validate(&p, 3, &d) == MULIB_MIR_RESOURCE_LIMIT);
    code[2].a = 2; assert(mulib_mir_validate(&p, 4, &d) == MULIB_MIR_BAD_REFERENCE && d.record == 2); code[2].a = 0;
    code[2].type = MULIB_MIR_BOOL; assert(mulib_mir_validate(&p, 4, NULL) == MULIB_MIR_BAD_TYPE); code[2].type = MULIB_MIR_I64;
    code[0].immediate = -1; assert(mulib_mir_validate(&p, 4, NULL) == MULIB_MIR_BAD_REFERENCE); code[0].immediate = 0;
    code[1].a = 0; assert(mulib_mir_validate(&p, 4, NULL) == MULIB_MIR_BAD_REFERENCE); code[1].a = N;
    code[1].type = MULIB_MIR_BOOL; code[1].immediate = 2;
    assert(mulib_mir_validate(&p, 4, NULL) == MULIB_MIR_BAD_TYPE); code[1].type = MULIB_MIR_I64;
    code[2].opcode = (mulib_mir_opcode_t)999; assert(mulib_mir_validate(&p, 4, NULL) == MULIB_MIR_BAD_OPCODE); code[2].opcode = MULIB_MIR_MUL;
    p.count = 3; assert(mulib_mir_validate(&p, 4, NULL) == MULIB_MIR_BAD_RETURN); p.count = 4;
    code[1].opcode = MULIB_MIR_RETURN; assert(mulib_mir_validate(&p, 4, NULL) != MULIB_MIR_OK); code[1].opcode = MULIB_MIR_CONST;
    assert(mulib_mir_validate(NULL, 0, NULL) == MULIB_MIR_INVALID);
    p.count = 0; assert(mulib_mir_validate(&p, 4, NULL) == MULIB_MIR_INVALID);
    assert(mulib_mir_buffer_init(&b, SIZE_MAX, 0) == MULIB_MIR_RESOURCE_LIMIT);
    assert(mulib_mir_buffer_init(&b, 0, 0) == MULIB_MIR_OK);
    assert(mulib_mir_append(&b, code[0]) == MULIB_MIR_RESOURCE_LIMIT); mulib_mir_buffer_free(&b);
    assert(mulib_mir_buffer_init(&b, 1, 1) == MULIB_MIR_OK);
    assert(mulib_mir_append(&b, code[0]) == MULIB_MIR_OK);
    assert(mulib_mir_view(&b).count == 1); mulib_mir_buffer_free(&b);
    assert(mulib_mir_evaluate(MULIB_MIR_ADD, INT64_MAX, 1, 0, &out) == MULIB_MIR_OVERFLOW && out == 19);
    assert(mulib_mir_evaluate(MULIB_MIR_SUB, INT64_MIN, 1, 0, &out) == MULIB_MIR_OVERFLOW);
    assert(mulib_mir_evaluate(MULIB_MIR_MUL, INT64_MIN, -1, 0, &out) == MULIB_MIR_OVERFLOW);
    assert(mulib_mir_evaluate(MULIB_MIR_MUL, -1, INT64_MIN, 0, &out) == MULIB_MIR_OVERFLOW);
    assert(mulib_mir_evaluate(MULIB_MIR_MUL, INT64_MIN, 1, 0, &out) == MULIB_MIR_OK && out == INT64_MIN);
    assert(mulib_mir_evaluate(MULIB_MIR_DIV, 1, 0, 0, &out) == MULIB_MIR_DIVIDE_BY_ZERO);
    assert(mulib_mir_evaluate(MULIB_MIR_MOD, INT64_MIN, -1, 0, &out) == MULIB_MIR_OVERFLOW);
    assert(mulib_mir_evaluate(MULIB_MIR_NEG, INT64_MIN, 0, 0, &out) == MULIB_MIR_OVERFLOW);
    assert(mulib_mir_evaluate(MULIB_MIR_RETURN, 0, 0, 0, &out) == MULIB_MIR_INVALID);
    assert(mulib_mir_evaluate(MULIB_MIR_CONST, 0, 0, 1, NULL) == MULIB_MIR_INVALID);
}
static void test_pipeline(void) {
    mulib_mir_instruction_t code[] = { P(0), C(6), C(7), B(MULIB_MIR_MUL,MULIB_MIR_I64,1,2),
        B(MULIB_MIR_COPY,MULIB_MIR_I64,3,N), C(123), B(MULIB_MIR_ADD,MULIB_MIR_I64,0,4), R(6) };
    mulib_mir_program_t p = { code, 8, 1 }, q;
    mulib_mir_buffer_t optimized = { 0 };
    mulib_cmachine_t m = { 0 };
    mulib_minivm_result_t a, b;
    mulib_minivm_limits_t limits = { 1, 1024, 1000 };
    int64_t arg = 8;
    char *source = NULL;
    assert(mulib_vmweave_optimize(&p, 8, &optimized, NULL) == MULIB_MIR_OK);
    q = mulib_mir_view(&optimized); assert(q.count == 4);
    assert(mulib_mir_validate(&q, 8, NULL) == MULIB_MIR_OK);
    a = run(&p,&arg,1); b = run(&q,&arg,1); assert(a.status == b.status && b.value == 50);
    arg = INT64_MAX; a = run(&p,&arg,1); b = run(&q,&arg,1);
    assert(a.status == MULIB_MINIVM_OVERFLOW && b.status == a.status);
    assert(mulib_cmachine_lower(&q,&arg,1,8,1,&m,NULL) == MULIB_MIR_RESOURCE_LIMIT && !m.code);
    assert(mulib_cmachine_lower(&q,NULL,0,8,100,&m,NULL) == MULIB_MIR_INVALID);
    assert(mulib_cmachine_lower(&q,&arg,1,8,100,&m,NULL) == MULIB_MIR_OK);
    assert(mulib_cmachine_run(&m,&limits,&a) == MULIB_MINIVM_STACK_OVERFLOW);
    limits.stack = 2; limits.steps = 0; assert(mulib_cmachine_run(&m,&limits,&a) == MULIB_MINIVM_STEP_LIMIT);
    mulib_cmachine_free(&m);
    assert(mulib_ccweave_generate(&q,"mulib_generated_test",8,20,&source,NULL) == MULIB_MIR_RESOURCE_LIMIT && !source);
    assert(mulib_ccweave_generate(&q,"int",8,10000,&source,NULL) == MULIB_MIR_INVALID);
    assert(mulib_ccweave_generate(&q,"mulib_generated_test",8,10000,&source,NULL) == MULIB_MIR_OK);
    assert(strstr(source,"INT64_MAX") && strstr(source,"return 8")); free(source);
    mulib_mir_buffer_free(&optimized);
    assert(mulib_vmweave_optimize(&p, 7, &optimized, NULL) == MULIB_MIR_RESOURCE_LIMIT && !optimized.code);
    {
        mulib_mir_instruction_t fault[] = { C(INT64_MAX), C(1), B(MULIB_MIR_ADD,MULIB_MIR_I64,0,1), C(0), R(3) };
        p.code = fault; p.count = 5; p.parameter_count = 0;
        assert(mulib_vmweave_optimize(&p,5,&optimized,NULL) == MULIB_MIR_OK); q = mulib_mir_view(&optimized);
        b = run(&q,NULL,0); assert(b.status == MULIB_MINIVM_OVERFLOW); mulib_mir_buffer_free(&optimized);
        fault[0].immediate = 1; fault[1].immediate = 0; fault[2].opcode = MULIB_MIR_DIV;
        assert(mulib_vmweave_optimize(&p,5,&optimized,NULL) == MULIB_MIR_OK); q = mulib_mir_view(&optimized);
        b = run(&q,NULL,0); assert(b.status == MULIB_MINIVM_DIVIDE_BY_ZERO); mulib_mir_buffer_free(&optimized);
    }
    {
        mulib_mir_instruction_t boolean[] = { { MULIB_MIR_PARAM,MULIB_MIR_BOOL,N,N,0 }, B(MULIB_MIR_NOT,MULIB_MIR_BOOL,0,N), R(1) };
        p.code = boolean; p.count = 3; p.parameter_count = 1; arg = 1;
        b = run(&p,&arg,1); assert(b.status == MULIB_MINIVM_OK && b.value == 0);
        arg = 2; assert(mulib_cmachine_lower(&p,&arg,1,3,20,&m,NULL) == MULIB_MIR_BAD_TYPE);
    }
}
static const int64_t boundaries[] = { INT64_MIN, INT64_MIN + 1, -3, -1, 0, 1, 2, INT64_MAX - 1, INT64_MAX };
static void test_differential(void) {
    int op;
    size_t a,b;
    for (op = MULIB_MIR_COPY; op <= MULIB_MIR_NOT; ++op) {
        for (a = 0; a < sizeof(boundaries)/sizeof(boundaries[0]); ++a) {
            for (b = 0; b < sizeof(boundaries)/sizeof(boundaries[0]); ++b) {
                mulib_mir_instruction_t code[] = { C(0),C(0),B(MULIB_MIR_ADD,MULIB_MIR_I64,0,1),R(2) };
                mulib_mir_program_t p = { code,4,0 }, q;
                mulib_mir_buffer_t optimized = { 0 };
                mulib_minivm_result_t x,y;
                int64_t expected = 0;
                mulib_mir_status_t s;
                code[0].immediate = boundaries[a]; code[1].immediate = boundaries[b]; code[2].opcode = (mulib_mir_opcode_t)op;
                if (mulib_mir_arity(code[2].opcode) == 1) code[2].b = N;
                if (op >= MULIB_MIR_EQ) code[2].type = MULIB_MIR_BOOL;
                if (op == MULIB_MIR_NOT) { if (boundaries[a] != 0 && boundaries[a] != 1) continue; code[0].type = MULIB_MIR_BOOL; }
                s = mulib_mir_evaluate(code[2].opcode,boundaries[a],boundaries[b],0,&expected);
                x = run(&p,NULL,0);
                assert(mulib_vmweave_optimize(&p,4,&optimized,NULL) == MULIB_MIR_OK); q = mulib_mir_view(&optimized); y = run(&q,NULL,0);
                assert(x.status == y.status);
                if (s == MULIB_MIR_OK) assert(x.status == MULIB_MINIVM_OK && x.value == expected && y.value == expected);
                else if (s == MULIB_MIR_OVERFLOW) assert(x.status == MULIB_MINIVM_OVERFLOW);
                else assert(s == MULIB_MIR_DIVIDE_BY_ZERO && x.status == MULIB_MINIVM_DIVIDE_BY_ZERO);
                mulib_mir_buffer_free(&optimized);
            }
        }
    }
}
static void test_isched(void) {
    mulib_isched_node_t nodes[] = { {4}, {1}, {2}, {1} };
    mulib_isched_edge_t edges[] = { {0,2}, {1,2}, {2,3} };
    mulib_isched_schedule_t s = { 0 };
    mulib_isched_diagnostic_t d;
    mulib_isched_limits_t limits = {4,4,100};
    assert(mulib_isched_run(nodes,4,edges,3,limits,&s,&d) == MULIB_ISCHED_OK);
    assert(s.order[0] == 0 && s.start[2] == 4 && s.start[3] == 6 && s.makespan == 7); mulib_isched_free(&s);
    limits.cycles = 6; assert(mulib_isched_run(nodes,4,edges,3,limits,&s,NULL) == MULIB_ISCHED_RESOURCE_LIMIT); limits.cycles = SIZE_MAX;
    edges[2].before = 2; edges[2].after = 0;
    assert(mulib_isched_run(nodes,4,edges,3,limits,&s,&d) == MULIB_ISCHED_CYCLE);
    edges[2].before = 2; edges[2].after = 3; nodes[0].latency = SIZE_MAX;
    assert(mulib_isched_run(nodes,4,edges,3,limits,&s,NULL) == MULIB_ISCHED_OVERFLOW);
    nodes[0].latency = 0; assert(mulib_isched_run(nodes,4,edges,3,limits,&s,NULL) == MULIB_ISCHED_INVALID); nodes[0].latency = 4;
    edges[0].after = 4; assert(mulib_isched_run(nodes,4,edges,3,limits,&s,NULL) == MULIB_ISCHED_INVALID); edges[0].after = 2;
    limits.nodes = 3; assert(mulib_isched_run(nodes,4,edges,3,limits,&s,NULL) == MULIB_ISCHED_RESOURCE_LIMIT);
    assert(mulib_isched_run(NULL,0,NULL,0,limits,&s,NULL) == MULIB_ISCHED_OK);
    mulib_isched_free(&s);
    {
        mulib_isched_node_t one[] = {{SIZE_MAX}};
        limits.nodes = 1;
        assert(mulib_isched_run(one,1,NULL,0,limits,&s,NULL) == MULIB_ISCHED_OK && s.makespan == SIZE_MAX); mulib_isched_free(&s);
    }
}
static int emit_instruction(void *user, size_t node, size_t rule, unsigned instruction) {
    unsigned *sum = (unsigned *)user; (void)node; (void)rule; *sum += instruction; return 1;
}
static int stop_instruction(void *user, size_t node, size_t rule, unsigned instruction) {
    (void)user; (void)node; (void)rule; (void)instruction; return 0;
}
typedef struct selected_bytecode {
    mulib_minivm_instruction_t code[8];
    const mulib_muburg_node_t *nodes;
    const int64_t *literals;
    size_t count;
} selected_bytecode;
static int emit_bytecode(void *user, size_t node, size_t rule, unsigned instruction) {
    selected_bytecode *b = (selected_bytecode *)user;
    (void)rule;
    if (!instruction) return 1; /* An immediate goal is consumed by its parent. */
    if (b->count > 5) return 0;
    if (instruction == 1 || instruction == 3) {
        size_t literal_node = instruction == 1 ? node : b->nodes[node].child[1];
        b->code[b->count].opcode = MULIB_MINIVM_PUSH;
        b->code[b->count++].operand = b->literals[literal_node];
    }
    if (instruction == 2 || instruction == 3) {
        b->code[b->count].opcode = MULIB_MINIVM_ADD; b->code[b->count++].operand = 0;
    }
    return 1;
}
static void test_muburg(void) {
    mulib_muburg_node_t nodes[] = { {0,0,{0,0}}, {0,0,{0,0}}, {1,2,{0,1}} };
    mulib_muburg_rule_t rules[] = { {0,0,0,{0,0},2,1}, {1,0,0,{0,0},0,2},
        {0,1,2,{0,0},3,3}, {0,1,2,{0,1},1,4} };
    mulib_muburg_limits_t limits = {3,4,2,6};
    mulib_muburg_plan_t p = { 0 };
    unsigned sum = 0; size_t emitted;
    assert(mulib_muburg_select(nodes,3,rules,4,2,2,0,limits,&p,NULL) == MULIB_MUBURG_OK && p.cost == 3);
    assert(p.choices[4] == 3);
    assert(mulib_muburg_emit(&p,3,emit_instruction,&sum,&emitted) == MULIB_MUBURG_OK && emitted == 3 && sum == 7);
    assert(mulib_muburg_emit(&p,2,emit_instruction,&sum,&emitted) == MULIB_MUBURG_RESOURCE_LIMIT && emitted == 2);
    assert(mulib_muburg_emit(&p,3,stop_instruction,NULL,&emitted) == MULIB_MUBURG_STOPPED && emitted == 1);
    nodes[2].child[0] = 3;
    assert(mulib_muburg_emit(&p,3,emit_instruction,&sum,&emitted) == MULIB_MUBURG_INVALID && emitted == 0);
    nodes[2].child[0] = 0;
    rules[3].child_goal[0] = 2;
    assert(mulib_muburg_emit(&p,3,emit_instruction,&sum,&emitted) == MULIB_MUBURG_INVALID && emitted == 0);
    rules[3].child_goal[0] = 0;
    rules[3].arity = 3;
    assert(mulib_muburg_emit(&p,3,emit_instruction,&sum,&emitted) == MULIB_MUBURG_INVALID && emitted == 0);
    rules[3].arity = 2;
    p.goal_count = SIZE_MAX;
    assert(mulib_muburg_emit(&p,3,emit_instruction,&sum,&emitted) == MULIB_MUBURG_RESOURCE_LIMIT && emitted == 0);
    p.goal_count = 2;
    mulib_muburg_free(&p);
    nodes[0].arity = 1; nodes[0].child[0] = 2;
    assert(mulib_muburg_select(nodes,3,rules,4,2,2,0,limits,&p,NULL) == MULIB_MUBURG_CYCLE); nodes[0].arity = 0;
    nodes[2].child[1] = 3; assert(mulib_muburg_select(nodes,3,rules,4,2,2,0,limits,&p,NULL) == MULIB_MUBURG_INVALID); nodes[2].child[1] = 1;
    assert(mulib_muburg_select(nodes,3,rules,0,2,2,0,limits,&p,NULL) == MULIB_MUBURG_NO_COVER);
    rules[0].cost = UINT64_MAX;
    assert(mulib_muburg_select(nodes,3,rules,4,2,0,0,limits,&p,NULL) == MULIB_MUBURG_OK && p.cost == UINT64_MAX);
    mulib_muburg_free(&p);
    assert(mulib_muburg_select(nodes,3,rules,4,2,2,0,limits,&p,NULL) == MULIB_MUBURG_OVERFLOW);
    limits.cells = 5; assert(mulib_muburg_select(nodes,3,rules,4,2,2,0,limits,&p,NULL) == MULIB_MUBURG_RESOURCE_LIMIT);
}
static void test_selected_execution(void) {
    mulib_muburg_node_t nodes[] = { {0,0,{0,0}}, {0,0,{0,0}}, {1,2,{0,1}} };
    mulib_muburg_rule_t rules[] = { {0,0,0,{0,0},2,1}, {1,0,0,{0,0},0,0}, {0,1,2,{0,0},3,2}, {0,1,2,{0,1},1,3} };
    mulib_muburg_limits_t limits = {3,4,2,6};
    mulib_muburg_plan_t plan = {0};
    int64_t literals[] = {6,7,0};
    selected_bytecode selected;
    mulib_minivm_program_t target;
    mulib_minivm_result_t actual,reference;
    mulib_mir_instruction_t code[] = {C(6),C(7),B(MULIB_MIR_ADD,MULIB_MIR_I64,0,1),R(2)};
    mulib_mir_program_t source = {code,4,0};
    memset(&selected,0,sizeof(selected)); selected.nodes = nodes; selected.literals = literals;
    assert(mulib_muburg_select(nodes,3,rules,4,2,2,0,limits,&plan,NULL) == MULIB_MUBURG_OK && plan.cost == 3);
    assert(mulib_muburg_emit(&plan,3,emit_bytecode,&selected,NULL) == MULIB_MUBURG_OK);
    assert(selected.count == 3); /* PUSH 6; selected ADD-immediate 7 -> PUSH 7; ADD. */
    selected.code[selected.count].opcode = MULIB_MINIVM_HALT; selected.code[selected.count++].operand = 0;
    target.code = selected.code; target.code_count = selected.count; target.local_count = 0; target.hosts = NULL; target.host_count = 0;
    assert(mulib_minivm_run(&target,NULL,&actual) == MULIB_MINIVM_OK);
    reference = run(&source,NULL,0); assert(reference.status == actual.status && actual.value == reference.value && actual.value == 13);
    mulib_muburg_free(&plan);
}
static void test_scheduled_mir(void) {
    mulib_mir_instruction_t code[] = { C(3), C(6), C(4), B(MULIB_MIR_ADD,MULIB_MIR_I64,0,2), B(MULIB_MIR_MUL,MULIB_MIR_I64,3,1), R(4) };
    mulib_mir_program_t original = { code,6,0 }, reordered;
    mulib_isched_node_t nodes[] = {{1},{1},{1},{2},{4}};
    mulib_isched_edge_t edges[] = {{0,3},{2,3},{3,4},{1,4}};
    mulib_isched_limits_t limits = {5,4,100};
    mulib_isched_schedule_t schedule = {0};
    mulib_mir_buffer_t result = {0};
    size_t map[5],k;
    mulib_minivm_result_t a,b;
    assert(mulib_isched_run(nodes,5,edges,4,limits,&schedule,NULL) == MULIB_ISCHED_OK);
    assert(schedule.order[1] == 2); /* This is a real reorder, not an identity pass. */
    assert(mulib_mir_buffer_init(&result,6,0) == MULIB_MIR_OK);
    for (k = 0; k < 5; ++k) {
        size_t id = schedule.order[k];
        mulib_mir_instruction_t i = code[id];
        int arity = mulib_mir_arity(i.opcode);
        if (arity) i.a = map[i.a];
        if (arity == 2) i.b = map[i.b];
        map[id] = k;
        assert(mulib_mir_append(&result,i) == MULIB_MIR_OK);
    }
    { mulib_mir_instruction_t ret = R(map[4]); assert(mulib_mir_append(&result,ret) == MULIB_MIR_OK); }
    reordered = mulib_mir_view(&result); a = run(&original,NULL,0); b = run(&reordered,NULL,0);
    assert(a.status == b.status && b.value == 42);
    mulib_mir_buffer_free(&result); mulib_isched_free(&schedule);
}
static size_t add(mulib_egraph_t *g, unsigned op, int64_t literal, unsigned arity, size_t a, size_t b) {
    mulib_egraph_node_t n = {0,0,0,{0,0}}; size_t id;
    n.opcode = op; n.literal = literal; n.arity = arity; n.child[0] = a; n.child[1] = b;
    assert(mulib_egraph_add(g,n,&id) == MULIB_EGRAPH_OK); return id;
}
static uint64_t expensive(const mulib_egraph_node_t *n, void *user) { (void)user; return n->arity ? 1 : UINT64_MAX; }
static uint64_t zero_cost(const mulib_egraph_node_t *n, void *user) { (void)n; (void)user; return 0; }
static void test_egraph(void) {
    mulib_egraph_t *g = NULL;
    mulib_egraph_tree_t t = { 0 };
    mulib_egraph_extract_limits_t limits = {1000,100};
    size_t a,b,fa,fb,ga,gb,id,used;
    mulib_egraph_node_t n = { 0,0,9,{0,0} };
    assert(mulib_egraph_create(SIZE_MAX,&g) == MULIB_EGRAPH_RESOURCE_LIMIT);
    assert(mulib_egraph_create(8,&g) == MULIB_EGRAPH_OK);
    a = add(g,0,1,0,0,0); b = add(g,0,2,0,0,0);
    assert(add(g,0,1,0,0,0) == a && mulib_egraph_count(g) == 2);
    fa = add(g,1,0,1,a,0); fb = add(g,1,0,1,b,0);
    ga = add(g,2,0,1,fa,0); gb = add(g,2,0,1,fb,0);
    assert(mulib_egraph_union(g,a,b,NULL) == MULIB_EGRAPH_OK);
    assert(mulib_egraph_add(g,n,&id) == MULIB_EGRAPH_DIRTY);
    assert(mulib_egraph_extract(g,ga,NULL,NULL,limits,&t) == MULIB_EGRAPH_DIRTY);
    assert(mulib_egraph_rebuild(g,1,&used) == MULIB_EGRAPH_RESOURCE_LIMIT && used == 1);
    assert(mulib_egraph_rebuild(g,100,&used) == MULIB_EGRAPH_OK);
    assert(mulib_egraph_find(g,fa) == mulib_egraph_find(g,fb));
    assert(mulib_egraph_find(g,ga) == mulib_egraph_find(g,gb));
    assert(mulib_egraph_extract(g,ga,NULL,NULL,limits,&t) == MULIB_EGRAPH_OK && t.count == 3 && t.cost == 3); mulib_egraph_tree_free(&t);
    assert(mulib_egraph_extract(g,ga,expensive,NULL,limits,&t) == MULIB_EGRAPH_OVERFLOW);
    limits.nodes = 2; assert(mulib_egraph_extract(g,ga,NULL,NULL,limits,&t) == MULIB_EGRAPH_RESOURCE_LIMIT); limits.nodes = 100;
    limits.work = 0; assert(mulib_egraph_extract(g,ga,NULL,NULL,limits,&t) == MULIB_EGRAPH_RESOURCE_LIMIT); limits.work = 1000;
    assert(mulib_egraph_union(g,a,fa,NULL) == MULIB_EGRAPH_OK); assert(mulib_egraph_rebuild(g,100,NULL) == MULIB_EGRAPH_OK);
    assert(mulib_egraph_extract(g,fa,zero_cost,NULL,limits,&t) == MULIB_EGRAPH_OK && t.count == 1 && t.cost == 0); mulib_egraph_tree_free(&t);
    assert(mulib_egraph_union(g,a,999,NULL) == MULIB_EGRAPH_INVALID);
    assert(mulib_egraph_find(g,999) == MULIB_EGRAPH_NONE);
    n.arity = 3; assert(mulib_egraph_add(g,n,&id) == MULIB_EGRAPH_INVALID);
    n.arity = 1; n.child[0] = 999; assert(mulib_egraph_add(g,n,&id) == MULIB_EGRAPH_INVALID);
    mulib_egraph_destroy(g); g = NULL;
    assert(mulib_egraph_create(0,&g) == MULIB_EGRAPH_OK); n.arity = 0;
    assert(mulib_egraph_add(g,n,&id) == MULIB_EGRAPH_RESOURCE_LIMIT);
    assert(mulib_egraph_rebuild(g,0,NULL) == MULIB_EGRAPH_OK); mulib_egraph_destroy(g);
    g = NULL; assert(mulib_egraph_create(1,&g) == MULIB_EGRAPH_OK);
    a = add(g,0,9,0,0,0); assert(add(g,0,9,0,0,0) == a); n.literal = 10;
    assert(mulib_egraph_add(g,n,&id) == MULIB_EGRAPH_RESOURCE_LIMIT);
    limits.work = 2; limits.nodes = 1;
    assert(mulib_egraph_extract(g,a,expensive,NULL,limits,&t) == MULIB_EGRAPH_OK && t.cost == UINT64_MAX);
    mulib_egraph_tree_free(&t); mulib_egraph_destroy(g);
}
#define VAR(v) { MULIB_REWRITE_VARIABLE,(v),0,0,0,{0,0} }
#define PAT(op,lit,ar,a,b) { MULIB_REWRITE_NODE,0,(op),(ar),(lit),{(a),(b)} }
static void test_rewrite(void) {
    /* x+0 = x, opcodes deliberately agree with MIR for a real extraction/lowering path. */
    mulib_rewrite_pattern_t lhs[] = { VAR(0), PAT(MULIB_MIR_CONST,0,0,0,0), PAT(MULIB_MIR_ADD,0,2,0,1) };
    mulib_rewrite_pattern_t rhs[] = { VAR(0) };
    mulib_rewrite_rule_t rule = { lhs,rhs,3,1 };
    mulib_rewrite_limits_t limits = {1,8,1000,100000};
    mulib_rewrite_result_t result;
    mulib_egraph_t *g = NULL;
    mulib_egraph_tree_t tree = { 0 };
    mulib_egraph_extract_limits_t extract = {1000,20};
    size_t x,z,root,i;
    mulib_mir_buffer_t mir = { 0 };
    mulib_mir_program_t p;
    mulib_minivm_result_t execution;
    assert(mulib_egraph_create(20,&g) == MULIB_EGRAPH_OK);
    x = add(g,MULIB_MIR_CONST,42,0,0,0); z = add(g,MULIB_MIR_CONST,0,0,0,0);
    root = add(g,MULIB_MIR_ADD,0,2,x,z);
    assert(mulib_rewrite_run(g,&rule,1,limits,&result) == MULIB_REWRITE_OK && result.applications == 1);
    assert(mulib_egraph_find(g,x) == mulib_egraph_find(g,root));
    assert(mulib_egraph_extract(g,root,NULL,NULL,extract,&tree) == MULIB_EGRAPH_OK && tree.count == 1);
    assert(mulib_mir_buffer_init(&mir,tree.count+1,0) == MULIB_MIR_OK);
    for (i = 0; i < tree.count; ++i) {
        mulib_mir_instruction_t instruction;
        instruction.opcode = (mulib_mir_opcode_t)tree.nodes[i].opcode; instruction.type = MULIB_MIR_I64;
        instruction.a = tree.nodes[i].arity ? tree.nodes[i].child[0] : N;
        instruction.b = tree.nodes[i].arity == 2 ? tree.nodes[i].child[1] : N;
        instruction.immediate = tree.nodes[i].literal;
        assert(mulib_mir_append(&mir,instruction) == MULIB_MIR_OK);
    }
    { mulib_mir_instruction_t ret = R(tree.root); assert(mulib_mir_append(&mir,ret) == MULIB_MIR_OK); }
    p = mulib_mir_view(&mir); execution = run(&p,NULL,0); assert(execution.status == MULIB_MINIVM_OK && execution.value == 42);
    mulib_mir_buffer_free(&mir); mulib_egraph_tree_free(&tree);
    rhs[0].variable = 1; assert(mulib_rewrite_run(g,&rule,1,limits,&result) == MULIB_REWRITE_INVALID); rhs[0].variable = 0;
    rule.rhs_count = MULIB_REWRITE_PATTERN_LIMIT + 1;
    assert(mulib_rewrite_run(g,&rule,1,limits,&result) == MULIB_REWRITE_INVALID); rule.rhs_count = 1;
    lhs[2].child[0] = 2; assert(mulib_rewrite_run(g,&rule,1,limits,&result) == MULIB_REWRITE_INVALID); lhs[2].child[0] = 0;
    limits.iterations = 0; assert(mulib_rewrite_run(g,&rule,1,limits,&result) == MULIB_REWRITE_ITERATION_LIMIT); limits.iterations = 8;
    limits.matches = 0; assert(mulib_rewrite_run(g,&rule,1,limits,&result) == MULIB_REWRITE_MATCH_LIMIT); limits.matches = 100;
    limits.work = 0; assert(mulib_rewrite_run(g,&rule,1,limits,&result) == MULIB_REWRITE_WORK_LIMIT); limits.work = 100000;
    limits.rules = 0; assert(mulib_rewrite_run(g,&rule,1,limits,&result) == MULIB_REWRITE_RULE_LIMIT); limits.rules = 1;
    mulib_egraph_destroy(g); g = NULL;
    /* Repeated variable matching must backtrack past an incompatible first child alternative. */
    {
        mulib_rewrite_pattern_t repeated[] = { VAR(0), VAR(0), PAT(3,0,2,0,1) };
        mulib_rewrite_pattern_t replacement[] = { PAT(9,7,0,0,0) };
        mulib_rewrite_rule_t rr = { repeated,replacement,3,1 };
        size_t y,xx,xy,replacement_id;
        assert(mulib_egraph_create(20,&g) == MULIB_EGRAPH_OK);
        x = add(g,0,1,0,0,0); y = add(g,0,2,0,0,0);
        xy = add(g,3,0,2,x,y); xx = add(g,3,0,2,x,x);
        assert(mulib_rewrite_run(g,&rr,1,limits,&result) == MULIB_REWRITE_OK && result.applications == 1);
        replacement_id = add(g,9,7,0,0,0);
        assert(mulib_egraph_find(g,xx) == mulib_egraph_find(g,replacement_id));
        assert(mulib_egraph_find(g,xy) != mulib_egraph_find(g,replacement_id));
        mulib_egraph_destroy(g); g = NULL;
    }
    {
        mulib_rewrite_pattern_t nested[] = { VAR(0), PAT(4,0,1,0,0), VAR(0), PAT(5,0,2,1,2) };
        mulib_rewrite_pattern_t replacement[] = { PAT(9,8,0,0,0) };
        mulib_rewrite_rule_t rr = { nested,replacement,4,1 };
        size_t y,gx,gy,replacement_id;
        assert(mulib_egraph_create(20,&g) == MULIB_EGRAPH_OK);
        x = add(g,0,1,0,0,0); y = add(g,0,2,0,0,0);
        gx = add(g,4,0,1,x,0); gy = add(g,4,0,1,y,0);
        root = add(g,5,0,2,gx,y);
        assert(mulib_egraph_union(g,gx,gy,NULL) == MULIB_EGRAPH_OK);
        /* The first g(x) alternative binds 1, incompatible with the right child 2.
         * Full backtracking must then find g(2), even though it is not first. */
        assert(mulib_rewrite_run(g,&rr,1,limits,&result) == MULIB_REWRITE_OK && result.applications == 1);
        replacement_id = add(g,9,8,0,0,0);
        assert(mulib_egraph_find(g,root) == mulib_egraph_find(g,replacement_id));
        mulib_egraph_destroy(g); g = NULL;
    }
    {
        assert(mulib_egraph_create(20,&g) == MULIB_EGRAPH_OK);
        x = add(g,MULIB_MIR_CONST,42,0,0,0); z = add(g,MULIB_MIR_CONST,0,0,0,0);
        root = add(g,MULIB_MIR_ADD,0,2,x,z);
        limits.iterations = 1;
        assert(mulib_rewrite_run(g,&rule,1,limits,&result) == MULIB_REWRITE_ITERATION_LIMIT && result.applications == 1);
        assert(mulib_egraph_find(g,root) == mulib_egraph_find(g,x));
        limits.iterations = 8;
        assert(mulib_rewrite_run(g,&rule,1,limits,&result) == MULIB_REWRITE_OK);
        assert(mulib_rewrite_run(g,NULL,0,limits,&result) == MULIB_REWRITE_OK);
        mulib_egraph_destroy(g); g = NULL;
    }
    /* Expanding rule consumes graph capacity and must report the underlying limit. */
    {
        mulib_rewrite_pattern_t a[] = { VAR(0) }, b[] = { VAR(0), PAT(7,0,1,0,0) };
        mulib_rewrite_rule_t expanding = { a,b,1,2 };
        assert(mulib_egraph_create(1,&g) == MULIB_EGRAPH_OK); (void)add(g,0,0,0,0,0);
        assert(mulib_rewrite_run(g,&expanding,1,limits,&result) == MULIB_REWRITE_EGRAPH_ERROR && result.graph_status == MULIB_EGRAPH_RESOURCE_LIMIT);
        mulib_egraph_destroy(g);
    }
}
static void emit_literal(int64_t v) {
    if (v == INT64_MIN) fputs("INT64_MIN",stdout);
    else if (v < 0) printf("-INT64_C(%" PRId64 ")",-v);
    else printf("INT64_C(%" PRId64 ")",v);
}
static void emit_generated(void) {
    int op;
    for (op = MULIB_MIR_COPY; op <= MULIB_MIR_NOT; ++op) {
        mulib_mir_instruction_t code[] = { P(0), P(1), B(MULIB_MIR_ADD,MULIB_MIR_I64,0,1), R(2) };
        mulib_mir_program_t p = { code,4,2 };
        char name[64], *source = NULL;
        code[2].opcode = (mulib_mir_opcode_t)op;
        if (mulib_mir_arity(code[2].opcode) == 1) code[2].b = N;
        if (op == MULIB_MIR_EQ || op == MULIB_MIR_LT || op == MULIB_MIR_LE || op == MULIB_MIR_NOT) code[2].type = MULIB_MIR_BOOL;
        if (op == MULIB_MIR_NOT) code[0].type = MULIB_MIR_BOOL;
        (void)snprintf(name,sizeof(name),"mulib_generated_op%d",op);
        assert(mulib_ccweave_generate(&p,name,4,100000,&source,NULL) == MULIB_MIR_OK); fputs(source,stdout); free(source);
    }
    {
        mulib_mir_instruction_t code[] = { C(INT64_MIN),C(-3),C(INT64_MAX),R(2) };
        mulib_mir_program_t p = { code,4,0 };
        char *source = NULL;
        assert(mulib_ccweave_generate(&p,"mulib_generated_literals",4,10000,&source,NULL) == MULIB_MIR_OK);
        fputs(source,stdout); free(source);
    }
    puts("#include <assert.h>\nint main(void) { int64_t a[2] = {6, 2}, out = 0;");
    for (op = MULIB_MIR_COPY; op <= MULIB_MIR_NOT; ++op) {
        int64_t expected;
        assert(mulib_mir_evaluate((mulib_mir_opcode_t)op,op == MULIB_MIR_NOT ? 1 : 6,2,0,&expected) == MULIB_MIR_OK);
        printf("a[0] = %d; a[1] = 2; assert(mulib_generated_op%d(a,2,&out) == 0 && out == %" PRId64 ");\n",op == MULIB_MIR_NOT ? 1 : 6,op,expected);
    }
    printf("a[0]=INT64_MAX; a[1]=1; out=17; assert(mulib_generated_op%d(a,2,&out)==%d && out==17);\n",MULIB_MIR_ADD,MULIB_MIR_OVERFLOW);
    printf("a[0]=INT64_MIN; a[1]=-1; assert(mulib_generated_op%d(a,2,&out)==%d);\n",MULIB_MIR_MUL,MULIB_MIR_OVERFLOW);
    printf("a[1]=0; assert(mulib_generated_op%d(a,2,&out)==%d);\n",MULIB_MIR_DIV,MULIB_MIR_DIVIDE_BY_ZERO);
    printf("a[0]=2; assert(mulib_generated_op%d(a,2,&out)==%d);\n",MULIB_MIR_NOT,MULIB_MIR_BAD_TYPE);
    puts("assert(mulib_generated_literals(NULL,0,&out)==0 && out==INT64_MAX);");
    for (op = MULIB_MIR_COPY; op <= MULIB_MIR_NOT; ++op) {
        size_t a,b;
        for (a = 0; a < sizeof(boundaries)/sizeof(boundaries[0]); ++a) for (b = 0; b < sizeof(boundaries)/sizeof(boundaries[0]); ++b) {
            int64_t expected = 17;
            mulib_mir_status_t s;
            if (op == MULIB_MIR_NOT && boundaries[a] != 0 && boundaries[a] != 1) s = MULIB_MIR_BAD_TYPE;
            else s = mulib_mir_evaluate((mulib_mir_opcode_t)op,boundaries[a],boundaries[b],0,&expected);
            fputs("a[0]=",stdout); emit_literal(boundaries[a]); fputs("; a[1]=",stdout); emit_literal(boundaries[b]);
            printf("; out=17; assert(mulib_generated_op%d(a,2,&out)==%d && out==",op,(int)s);
            emit_literal(expected); puts(");");
        }
    }
    puts("return 0; }");
}
int main(int argc, char **argv) {
    if (argc == 2 && !strcmp(argv[1],"--emit")) { emit_generated(); return 0; }
    test_mir(); test_pipeline(); test_differential(); test_isched(); test_muburg(); test_selected_execution(); test_scheduled_mir(); test_egraph(); test_rewrite();
    puts("compiler IR tests passed"); return 0;
}

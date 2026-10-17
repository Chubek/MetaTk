use ir, pipeline, control_flow, dataflow, optimization, compute;

fn same(actual: list<int>, expected: list<int>) {
    assert actual.len() == expected.len();
    for (let i = 0; i < actual.len(); i++) assert actual[i] == expected[i];
    return 0;
}
fn rows(actual: list<list<int>>, expected: list<list<int>>) {
    assert actual.len() == expected.len();
    for (let i = 0; i < actual.len(); i++) same(actual[i], expected[i]);
    return 0;
}

{
    let code = pipeline::compile("module 1 { const r0 0; jz r0 4; const r1 42; jmp 5; const r1 42; mov r2 r1; ret r2; }", 0);
    same(control_flow::reachable(code), [1, 1, 1, 1, 1, 1, 1]);
    rows(control_flow::predecessors(code), [[], [0], [1], [2], [1], [3, 4], [5]]);
    rows(control_flow::blocks(code), [[0, 2], [2, 4], [4, 5], [5, 7]]);
    same(control_flow::immediate_dominators(code), [0, 0, 1, 2, 1, 1, 5]);
    rows(dataflow::liveness(code), [[0, 1], [1, 0], [0, 2], [2, 2], [0, 2], [2, 4], [4, 0]]);
    let states = optimization::constants(code);
    assert states[5][0] == 1 && states[5][1] == 3 && states[5][3] == 42;
    let global = optimization::propagate(code);
    assert optimization::fold(code)[5][0] == ir::MOV;
    same(global[5], [ir::CONST, 2, 0, 0, 42]);
    assert code[5][0] == ir::MOV;
    global[0][4] = 99;
    assert code[0][4] == 0; // no row aliases, including unchanged instructions
    assert compute::run(optimization::propagate(code), 10) == 42;
}
{
    // A constant branch is not pruned: both syntactic CFG edges participate.
    let code = pipeline::compile("module 1 { const r0 0; jz r0 4; const r1 41; jmp 5; const r1 42; mov r2 r1; ret r2; }", 0);
    let states = optimization::constants(code);
    assert states[5][1] == 1 && states[5][3] == 0;
    assert optimization::propagate(code)[5][0] == ir::MOV;
}
{
    // Unreachable predecessors do not pollute reachable dominance or facts.
    let code = pipeline::compile("module 1 { const r0 42; jmp 4; const r0 9; jmp 4; mov r1 r0; ret r1; }", 0);
    same(control_flow::reachable(code), [1, 1, 0, 0, 1, 1]);
    same(control_flow::reverse_postorder(code), [0, 1, 4, 5]);
    same(control_flow::immediate_dominators(code), [0, 0, -1, -1, 1, 4]);
    same(control_flow::predecessors(code)[4], [1, 3]);
    same(dataflow::liveness(code)[2], [0, 0]);
    same(optimization::propagate(code)[4], [ir::CONST, 1, 0, 0, 42]);
    let states = optimization::constants(code);
    assert states[2][0] == 0 && states[2][1] == 0;
    states[2][2] = 99;
    assert states[3][2] == 0; // distinct empty states, not repeated aliases
}
{
    let text = "module 1 { const r0 5; const r1 0; const r2 1; add r1 r1 r0; sub r0 r0 r2; jz r0 7; jmp 3; ret r1; }";
    let code = pipeline::compile(text, 0);
    let global = optimization::propagate(code);
    same(control_flow::immediate_dominators(code), [0, 0, 1, 2, 3, 4, 5, 5]);
    assert optimization::constants(code)[3][1] == 4;
    assert global[3][0] == ir::ADD && global[4][0] == ir::SUB;
    assert pipeline::run(text, 100, 2) == 15;
    assert pipeline::run(text, 23, 2) == 15; // exact original instruction count
}
{
    let code = pipeline::compile("module 1 { const r31 7; jz r31 2; ret r31; }", 0);
    same(control_flow::successors(code, 1), [2]); // no duplicate CFG edge
    same(control_flow::predecessors(code)[2], [1]);
    same(control_flow::immediate_dominators(code), [0, 0, 1]);
    rows(dataflow::liveness(code), [[0, 2147483648], [2147483648, 2147483648], [2147483648, 0]]);
}
{
    let code = pipeline::compile("module 1 { const r0 1; jmp 0; }", 0);
    same(control_flow::immediate_dominators(code), [0, 0]);
    assert optimization::constants(code)[0][1] == 0;
}

fn binary(op, a, b) {
    // Branch boundary makes the fold depend on global, not block-local facts.
    let code = [[ir::CONST, 0, 0, 0, a], [ir::CONST, 1, 0, 0, b],
                [ir::JMP, 0, 0, 0, 3], [op, 2, 0, 1, 0], [ir::RET, 0, 2, 0, 0]];
    let global = optimization::propagate(code);
    assert global[3][0] == ir::CONST;
    assert compute::run(global, 5) == ir::evaluate(op, a, b);
    rows(optimization::propagate(global), global); // idempotent
    return 0;
}
for (let op = ir::ADD; op <= ir::LT; op++) binary(op, -27, 3);
binary(ir::ADD, 9223372036854775807, 1);
binary(ir::MUL, -9223372036854775808, -1);

{
    let code = pipeline::compile("module 1 { const r0 1; const r1 0; jmp 3; div r0 r0 r1; ret r0; }", 2);
    assert code[3][0] == ir::DIV;
    code = pipeline::compile("module 1 { const r0 -9223372036854775808; const r1 -1; jmp 3; mod r0 r0 r1; ret r0; }", 2);
    assert code[3][0] == ir::MOD;
    code = pipeline::compile("module 1 { const r0 1; const r1 64; jmp 3; shr r0 r0 r1; ret r0; }", 2);
    assert code[3][0] == ir::SHR;
}
print "MLVM dataflow passed";

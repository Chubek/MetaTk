use ir, parser, control_flow, compute, optimization, serialize, bytecode, pipeline;

let text = "module 1 { const r0 20; const r1 22; add r2 r0 r1; ret r2; }";
let code = pipeline::compile(text, 0);
assert code.len() == 4;
assert compute::run(code, 4) == 42;
let folded = optimization::fold(code);
assert folded[2][0] == ir::CONST && folded[2][4] == 42;
assert code[2][0] == ir::ADD;  // optimizer never mutates the input
folded[0][4] = 99;
assert code[0][4] == 20;
assert pipeline::run(serialize::text(code), 100, 1) == 42;
let words = bytecode::encode(code);
let copy = bytecode::decode(words);
assert compute::run(copy, 100) == 42;
copy[0][4] = 10;
assert code[0][4] == 20 && words[7] == 20;

// A backedge must not invent definitions at the entry, and the optimizer
// must forget constants at a loop header and at its exits.
let looptext = "module 1 { const r0 5; const r1 0; const r2 1; add r1 r1 r0; sub r0 r0 r2; jz r0 7; jmp 3; ret r1; }";
assert pipeline::run(looptext, 100, 0) == 15;
assert pipeline::run(looptext, 100, 1) == 15;
let diamond = "module 1 { const r0 0; jz r0 4; const r1 30; jmp 5; const r1 42; ret r1; }";
assert pipeline::run(diamond, 100, 1) == 42;
assert pipeline::run("module 1 { const r0 9223372036854775807; const r1 1; add r0 r0 r1; ret r0; }", 10, 1) == -9223372036854775808;
assert pipeline::run("module 1 { const r0 -1; const r1 63; shr r0 r0 r1; ret r0; }", 10, 1) == 1;
assert pipeline::run("module 1 { # comment\n const r0 -42; mov r1 r0; ret r1; }", 10, 1) == -42;

fn binary(op, a, b) {
    let program = [[ir::CONST, 0, 0, 0, a], [ir::CONST, 1, 0, 0, b],
                   [op, 2, 0, 1, 0], [ir::RET, 0, 2, 0, 0]];
    assert compute::run(program, 10) == ir::evaluate(op, a, b);
    assert compute::run(optimization::fold(program), 10) == ir::evaluate(op, a, b);
    return 0;
}
for (let op = ir::ADD; op <= ir::LT; op++) binary(op, -27, 3);

// Traps remain runtime operations, not optimizer errors or removed work.
let trap = pipeline::compile("module 1 { const r0 1; const r1 0; div r0 r0 r1; ret r0; }", 1);
assert trap[2][0] == ir::DIV;
let shift = pipeline::compile("module 1 { const r0 1; const r1 64; shl r0 r0 r1; ret r0; }", 1);
assert shift[2][0] == ir::SHL;
print "MLVM core passed";

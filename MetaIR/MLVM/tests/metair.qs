use ir, metair, pipeline, compute, optimization, serialize;

fn roundtrip(code: list<list<int>>, expected) {
    let bytes = metair::export_mcil(code);
    assert bytes.len() == 12 + code.len() * 12;
    assert bytes.sub(0, 4) == "MCIL" && bytes.at(4) == 1;
    let copy = metair::import_mcil(bytes);
    assert serialize::text(copy) == serialize::text(code);
    assert metair::export_mcil(copy) == bytes;
    assert compute::run(copy, 100) == expected;
    assert compute::run(optimization::propagate(copy), 100) == expected;
    copy[0][4] = 99;
    assert metair::export_mcil(code) == bytes;
    return 0;
}
roundtrip(pipeline::compile("module 1 { const r0 5; const r1 0; const r2 1; add r1 r1 r0; sub r0 r0 r2; jz r0 7; jmp 3; ret r1; }", 0), 15);
roundtrip([[ir::CONST, 31, 0, 0, -9223372036854775808], [ir::RET, 0, 31, 0, 0]], -9223372036854775808);
roundtrip([[ir::CONST, 31, 0, 0, 9223372036854775807], [ir::RET, 0, 31, 0, 0]], 9223372036854775807);
roundtrip([[ir::CONST, 31, 0, 0, -1], [ir::RET, 0, 31, 0, 0]], -1);

fn binary(op) {
    let code = [[ir::CONST, 0, 0, 0, -27], [ir::CONST, 1, 0, 0, 3],
                [op, 2, 0, 1, 0], [ir::MOV, 31, 2, 0, 0], [ir::RET, 0, 31, 0, 0]];
    roundtrip(code, ir::evaluate(op, -27, 3));
    return 0;
}
for (let op = ir::ADD; op <= ir::LT; op++) binary(op);
print "MLVM MetaIR conversion passed";

use ir, pipeline, register;

{
    let code = pipeline::compile("module 1 { const r0 20; const r1 22; add r31 r0 r1; ret r31; }", 0);
    let graph = register::interference(code);
    assert graph[0] == 2 && graph[1] == 1 && graph[31] == 0;
    let colors = register::color(code, 2);
    assert colors[0] == 0 && colors[1] == 1 && colors[31] == 0;
    assert colors[2] == -1;
    colors[0] = 99;
    assert register::color(code, 2)[0] == 0;
    assert code[0][1] == 0;
}
{
    // Dead destination writes still interfere with live values they can clobber.
    let code = pipeline::compile("module 1 { const r0 42; const r31 99; ret r0; }", 0);
    let graph = register::interference(code);
    assert graph[0] == 2147483648 && graph[31] == 1;
    let colors = register::color(code, 2);
    assert colors[0] != colors[31];
}
{
    // Thirty-two virtual register names, one live value at a time.
    let code: list<list<int>> = [];
    for (let reg = 0; reg < 32; reg++) code.push([ir::CONST, reg, 0, 0, reg]);
    code.push([ir::RET, 0, 31, 0, 0]);
    let colors = register::color(code, 1);
    for (my color in colors) assert color == 0;
}
{
    let code = pipeline::compile("module 1 { const r31 42; ret r31; const r0 1; ret r0; }", 0);
    let colors = register::color(code, 1);
    assert colors[31] == 0 && colors[0] == -1;
}
print "MLVM register allocation passed";

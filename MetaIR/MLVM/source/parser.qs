use ir;

grammar Text {
    token ws { [ \s | '#' <-[\n]>* ]* }
    token number { '-'? \d+ }
    token reg { 'r' \d+ }
    token op { <[a..z]>+ }
    rule instruction { <op> [ <reg> | <number> ]* ';' }
    rule TOP { ^ 'module' '1' '{' <instruction>* '}' $ }
}

fn _reg(text: str) {
    if (!text.starts_with("r")) die("MLVM: expected register");
    let r = text.sub(1, text.len() - 1).to_int();
    if (r < 0 || r >= ir::REGISTERS) die("MLVM: register out of range");
    return r;
}
fn _instruction(m: match): list<int> {
    let op = ir::opcode(m.child("op").text());
    let rs = m.children("reg");
    let ns = m.children("number");
    let expected = ir::binary(op) ? 3 : (op == ir::MOV ? 2 : 1);
    if (op == ir::JMP) expected = 0;
    let immediates = op == ir::CONST || op == ir::JMP || op == ir::JZ ? 1 : 0;
    if (rs.len() != expected || ns.len() != immediates)
        die("MLVM: wrong operands for " ~ m.child("op").text());
    // Check order too: counting captures alone would accept `const 3 r0`.
    for (let i = 0; i < expected; i++) {
        if (m.at(i + 1).rule() != "reg") die("MLVM: expected register operand");
    }
    if (immediates && m.at(expected + 1).rule() != "number")
        die("MLVM: expected immediate operand");
    let row = [op, 0, 0, 0, 0];
    if (ir::writes(op)) row[1] = _reg(rs[0].text());
    if (ir::reads_left(op)) row[2] = _reg(rs[ir::writes(op) ? 1 : 0].text());
    if (ir::binary(op)) row[3] = _reg(rs[2].text());
    if (immediates) row[4] = ns[0].text().to_int();
    return row;
}
fn parse(text: str): list<list<int>> {
    let m = Text.parse(text);
    if (!m.ok()) die("MLVM syntax: " ~ m.error());
    let out: list<list<int>> = [];
    for (my item in m.children("instruction")) {
        if (out.len() >= ir::MAX_INSTRUCTIONS) die("MLVM: instruction limit exceeded");
        out.push(_instruction(item));
    }
    return out;
}

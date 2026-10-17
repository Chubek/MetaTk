use ir, control_flow;

fn text(code: list<list<int>>): str {
    control_flow::verify(code);
    let out = "module 1 {\n";
    let names = ir::names();
    for (my row in code) {
        let op = row[0];
        out ~= "  " ~ names[op];
        if (ir::writes(op)) out ~= " r" ~ row[1].str();
        if (ir::reads_left(op)) out ~= " r" ~ row[2].str();
        if (ir::binary(op)) out ~= " r" ~ row[3].str();
        if (op == ir::CONST || op == ir::JMP || op == ir::JZ) out ~= " " ~ row[4].str();
        out ~= ";\n";
    }
    return out ~ "}";
}

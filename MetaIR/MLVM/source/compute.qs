use ir, control_flow;

fn run(code: list<list<int>>, max_steps) {
    control_flow::verify(code);
    if (max_steps <= 0) die("MLVM: step limit exceeded");
    let regs = ir::zeros(ir::REGISTERS);
    let pc = 0;
    for (let steps = 0; steps < max_steps; steps++) {
        let row = code[pc];
        let op = row[0];
        if (op == ir::RET) return regs[row[2]];
        if (op == ir::CONST) regs[row[1]] = row[4];
        elsif (op == ir::MOV) regs[row[1]] = regs[row[2]];
        elsif (ir::binary(op)) regs[row[1]] = ir::evaluate(op, regs[row[2]], regs[row[3]]);
        if (op == ir::JMP || op == ir::JZ && regs[row[2]] == 0) pc = row[4];
        else pc++;
    }
    die("MLVM: step limit exceeded");
    return 0;
}

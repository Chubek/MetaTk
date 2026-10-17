use ir, control_flow;

// Portable lowering to QScript. QScript owns native ABI, assembler, relocations
// and ELF linking; MLVM does not duplicate its architecture tables.
fn qscript(code: list<list<int>>, max_steps): str {
    control_flow::verify(code);
    if (max_steps <= 0) die("MLVM: step limit must be positive");
    let out = "let regs: list<int> = [";
    for (let i = 0; i < ir::REGISTERS; i++) out ~= i == 0 ? "0" : ",0";
    out ~= "];\nlet pc = 0;\nlet steps = 0;\nwhile (steps < " ~ max_steps.str() ~ ") {\nsteps++;\n";
    let operators = ["+", "-", "*", "/", "%", "&", "|", "^", "<<", ">>", "==", "<"];
    for (let i = 0; i < code.len(); i++) {
        let row = code[i];
        let op = row[0];
        let dst = "regs[" ~ row[1].str() ~ "]";
        let a = "regs[" ~ row[2].str() ~ "]";
        let b = "regs[" ~ row[3].str() ~ "]";
        out ~= "if (pc == " ~ i.str() ~ ") {\n";
        if (op == ir::RET) out ~= "return " ~ a ~ ";\n";
        elsif (op == ir::JMP) out ~= "pc = " ~ row[4].str() ~ ";\n";
        elsif (op == ir::JZ)
            out ~= "pc = " ~ a ~ " == 0 ? " ~ row[4].str() ~ " : " ~ (i + 1).str() ~ ";\n";
        else {
            let expr = row[4].str();
            if (op == ir::MOV) expr = a;
            if (ir::binary(op)) expr = a ~ " " ~ operators[op - ir::ADD] ~ " " ~ b;
            out ~= dst ~ " = " ~ expr ~ ";\npc = " ~ (i + 1).str() ~ ";\n";
        }
        out ~= "continue;\n}\n";
    }
    return out ~ "die(\"MLVM: invalid program counter\");\n}\ndie(\"MLVM: step limit exceeded\");\n";
}

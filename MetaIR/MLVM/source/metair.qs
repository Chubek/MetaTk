// MetaIR's MCIL version 1 wire format, not native architecture opcodes.
// The shared integer subset is one instruction in each IR, so PCs and budgets
// need no fixups. MetaIL HALT maps to MLVM RET, NOT MetaIL's stack-based RET.
use ir, control_flow;

fn _read(bytes: str, offset, count) {
    let value = 0;
    for (let i = 0; i < count; i++) value |= bytes.at(offset + i) << (8 * i);
    return value;
}
fn _bytes(value, count): str {
    let out = "";
    for (let i = 0; i < count; i++) out ~= ((value >> (8 * i)) & 255).chr();
    return out;
}
fn import_mcil(bytes: str): list<list<int>> {
    if (bytes.len() < 12 || bytes.sub(0, 4) != "MCIL" || bytes.at(4) != 1 ||
        bytes.at(5) != 0 || bytes.at(6) != 0 || bytes.at(7) != 0)
        die("MLVM bridge: invalid MCIL header");
    let count = _read(bytes, 8, 4);
    if (count < 1 || count > ir::MAX_INSTRUCTIONS || bytes.len() != 12 + count * 12)
        die("MLVM bridge: invalid MCIL length or instruction limit");
    let code: list<list<int>> = [];
    for (let pc = 0; pc < count; pc++) {
        let offset = 12 + pc * 12;
        let op = bytes.at(offset);
        let a = bytes.at(offset + 1);
        let b = bytes.at(offset + 2);
        let c = bytes.at(offset + 3);
        let imm = _read(bytes, offset + 4, 8);
        if (op >= 1 && op <= 14) code.push([op - 1, a, b, c, imm]);
        elsif (op == 15 || op == 16 || op == 23) {
            if (b != 0 || c != 0 || (op == 15 && a != 0) || (op == 23 && imm != 0))
                die("MLVM bridge: noncanonical MCIL instruction at " ~ pc.str());
            if (op == 15) code.push([ir::JMP, 0, 0, 0, imm]);
            elsif (op == 16) code.push([ir::JZ, 0, a, 0, imm]);
            else code.push([ir::RET, 0, a, 0, 0]);
        } else die("MLVM bridge: unsupported MetaIL opcode at " ~ pc.str());
    }
    // In particular, do not silently convert MetaIL's implicit zero registers
    // into MLVM definitions. The bridge requires definite assignment.
    control_flow::verify(code);
    return code;
}
fn export_mcil(code: list<list<int>>): str {
    control_flow::verify(code);
    let parts = ["MCIL\x01\0\0\0" ~ _bytes(code.len(), 4)];
    for (my row in code) {
        let op = row[0] + 1;
        let a = row[1];
        let b = row[2];
        let c = row[3];
        if (row[0] == ir::JZ || row[0] == ir::RET) {
            op = row[0] == ir::JZ ? 16 : 23;
            a = row[2]; b = 0; c = 0;
        }
        parts.push(op.chr() ~ a.chr() ~ b.chr() ~ c.chr() ~ _bytes(row[4], 8));
    }
    return parts.join("");
}

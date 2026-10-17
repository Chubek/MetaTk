use ir, control_flow, register, arch::riscv64;

fn _word(bytes: list<int>, word) {
    for (let i = 0; i < 4; i++) bytes.push((word >> (i * 8)) & 255);
    return 0;
}
fn _register(reg, colors: list<int>, allocation: list<int>) {
    let color = colors[reg];
    if (color < 0 || color >= allocation.len()) die("MLVM RV64: unallocated virtual register");
    return allocation[color];
}

// A no-argument leaf function, returning a signed integer in the Arch ABI
// result register. Raw little-endian code, not ELF and not a Linux executable.
// Branches and potentially trapping operations require a future trap ABI.
fn emit(code: list<list<int>>): list<int> {
    control_flow::verify(code);
    let allocation = arch::riscv64::allocation();
    let colors = register::color(code, allocation.len());
    let out: list<int> = [];
    for (my row in code) {
        let op = row[0];
        if (op == ir::RET) {
            _word(out, arch::riscv64::encode("addi", arch::riscv64::RESULT,
                  _register(row[2], colors, allocation), arch::riscv64::ZERO, 0));
            _word(out, arch::riscv64::encode("ret", 0, 0, 0, 0));
            return out;
        }
        if (op == ir::CONST) {
            _word(out, arch::riscv64::encode("addi", _register(row[1], colors, allocation),
                  arch::riscv64::ZERO, arch::riscv64::ZERO, row[4]));
        } elsif (op == ir::MOV) {
            _word(out, arch::riscv64::encode("addi", _register(row[1], colors, allocation),
                  _register(row[2], colors, allocation), arch::riscv64::ZERO, 0));
        } else {
            let name = "";
            if (op == ir::ADD) name = "add";
            elsif (op == ir::SUB) name = "sub";
            elsif (op == ir::MUL) name = "mul";
            elsif (op == ir::AND) name = "and";
            elsif (op == ir::OR) name = "or";
            elsif (op == ir::XOR) name = "xor";
            elsif (op == ir::LT) name = "slt";
            else die("MLVM RV64: unsupported operation (no trap/control-flow ABI)");
            _word(out, arch::riscv64::encode(name, _register(row[1], colors, allocation),
                  _register(row[2], colors, allocation), _register(row[3], colors, allocation), 0));
        }
    }
    die("MLVM RV64: missing return");
    return out;
}

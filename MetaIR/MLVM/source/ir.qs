// MLVM v1 integer IR. A module is an instruction list; each instruction is
// [op, destination, left, right, immediate]. No host handles or native opcodes.
const CONST = 0;
const MOV = 1;
const ADD = 2;
const SUB = 3;
const MUL = 4;
const DIV = 5;
const MOD = 6;
const AND = 7;
const OR = 8;
const XOR = 9;
const SHL = 10;
const SHR = 11;
const EQ = 12;
const LT = 13;
const JMP = 14;
const JZ = 15;
const RET = 16;
const REGISTERS = 32;
const MAX_INSTRUCTIONS = 4096;

fn names(): list<str> {
    return ["const", "mov", "add", "sub", "mul", "div", "mod", "and", "or",
            "xor", "shl", "shr", "eq", "lt", "jmp", "jz", "ret"];
}
fn opcode(name: str) {
    let ns = names();
    for (let i = 0; i < ns.len(); i++) { if (ns[i] == name) return i; }
    die("MLVM: unknown operation " ~ name);
    return -1;
}
fn writes(op) { return op >= CONST && op <= LT; }
fn reads_left(op) { return op >= MOV && op <= LT || op == JZ || op == RET; }
fn binary(op) { return op >= ADD && op <= LT; }
fn zeros(n): list<int> {
    let out: list<int> = [];
    for (let i = 0; i < n; i++) out.push(0);
    return out;
}
fn evaluate(op, a, b) {
    if (op == ADD) return a + b;
    if (op == SUB) return a - b;
    if (op == MUL) return a * b;
    if (op == DIV) return a / b;
    if (op == MOD) return a % b;
    if (op == AND) return a & b;
    if (op == OR) return a | b;
    if (op == XOR) return a ^ b;
    if (op == SHL) return a << b;
    if (op == SHR) return a >> b;
    if (op == EQ) return a == b;
    if (op == LT) return a < b;
    die("MLVM: not a binary operation");
    return 0;
}

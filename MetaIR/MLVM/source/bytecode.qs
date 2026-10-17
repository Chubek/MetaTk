use ir, control_flow;

// Versioned word stream, not host memory or an executable object format.
const MAGIC = 0x4d4c564d;
const VERSION = 1;
fn encode(code: list<list<int>>): list<int> {
    control_flow::verify(code);
    let out = [MAGIC, VERSION, code.len()];
    for (my row in code) { for (my field in row) out.push(field); }
    return out;
}
fn decode(words: list<int>): list<list<int>> {
    if (words.len() < 3 || words[0] != MAGIC || words[1] != VERSION)
        die("MLVM: invalid bytecode header");
    let n = words[2];
    if (n <= 0 || n > ir::MAX_INSTRUCTIONS || words.len() != 3 + n * 5)
        die("MLVM: invalid bytecode length");
    let out: list<list<int>> = [];
    for (let i = 0; i < n; i++) out.push(words.slice(3 + i * 5, 5));
    control_flow::verify(out);
    return out;
}

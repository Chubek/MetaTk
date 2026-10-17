use ir, control_flow;

// One [live-in, live-out] pair per instruction, represented as 32-bit register
// masks in signed 64-bit QScript integers. Unreachable nodes have zero masks.
fn liveness(code: list<list<int>>): list<list<int>> {
    control_flow::verify(code);
    let seen = control_flow::reachable(code);
    let out: list<list<int>> = [];
    for (let pc = 0; pc < code.len(); pc++) out.push([0, 0]);
    let changed = 1;
    while (changed) {
        changed = 0;
        for (let pc = code.len() - 1; pc >= 0; pc--) {
            if (!seen[pc]) continue;
            let row = code[pc];
            let live = 0;
            for (my target in control_flow::successors(code, pc)) live |= out[target][0];
            let uses = 0;
            let defs = 0;
            if (ir::reads_left(row[0])) uses |= 1 << row[2];
            if (ir::binary(row[0])) uses |= 1 << row[3];
            if (ir::writes(row[0])) defs = 1 << row[1];
            let incoming = uses | (live & ~defs);
            if (out[pc][0] != incoming || out[pc][1] != live) {
                out[pc][0] = incoming; out[pc][1] = live; changed = 1;
            }
        }
    }
    return out;
}

fn _ints(xs: list<int>): str {
    let parts: list<str> = [];
    for (my x in xs) parts.push(x.str());
    return "[" ~ parts.join(",") ~ "]";
}
fn _rows(xs: list<list<int>>): str {
    let parts: list<str> = [];
    for (my row in xs) parts.push(_ints(row));
    return "[" ~ parts.join(",") ~ "]";
}
// JSON serialization remains in QScript. The launcher only transports bytes.
fn report(code: list<list<int>>): str {
    control_flow::verify(code);
    let seen = control_flow::reachable(code);
    let ranges = control_flow::blocks(code);
    let preds = control_flow::predecessors(code);
    let count = 0;
    let edges = 0;
    for (my bit in seen) count += bit;
    for (my row in preds) edges += row.len();
    let text = "{\"instruction_count\":" ~ code.len().str();
    text ~= ",\"reachable_count\":" ~ count.str();
    text ~= ",\"block_count\":" ~ ranges.len().str();
    text ~= ",\"edge_count\":" ~ edges.str();
    text ~= ",\"reachable\":" ~ _ints(seen);
    text ~= ",\"predecessors\":" ~ _rows(preds);
    text ~= ",\"reverse_postorder\":" ~ _ints(control_flow::reverse_postorder(code));
    text ~= ",\"blocks\":" ~ _rows(ranges);
    text ~= ",\"immediate_dominators\":" ~ _ints(control_flow::immediate_dominators(code));
    text ~= ",\"liveness\":" ~ _rows(liveness(code));
    return text ~ "}";
}

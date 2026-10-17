use ir, control_flow, dataflow;

fn _clique(graph: list<int>, mask) {
    for (let reg = 0; reg < ir::REGISTERS; reg++) {
        let bit = 1 << reg;
        if (mask & bit) graph[reg] |= mask & ~bit;
    }
    return 0;
}
// Virtual-register interference, independent of physical architecture names.
// A definition conflicts with all other live-out values even if its own result
// is dead: emitted writes must not destroy another virtual register's value.
fn interference(code: list<list<int>>): list<int> {
    control_flow::verify(code);
    let seen = control_flow::reachable(code);
    let live = dataflow::liveness(code);
    let graph = ir::zeros(ir::REGISTERS);
    for (let pc = 0; pc < code.len(); pc++) {
        if (!seen[pc]) continue;
        _clique(graph, live[pc][0]);
        let mask = live[pc][1];
        if (ir::writes(code[pc][0])) mask |= 1 << code[pc][1];
        _clique(graph, mask);
    }
    return graph;
}
fn _active(code: list<list<int>>): int {
    let seen = control_flow::reachable(code);
    let mask = 0;
    for (let pc = 0; pc < code.len(); pc++) {
        if (!seen[pc]) continue;
        let row = code[pc];
        if (ir::writes(row[0])) mask |= 1 << row[1];
        if (ir::reads_left(row[0])) mask |= 1 << row[2];
        if (ir::binary(row[0])) mask |= 1 << row[3];
    }
    return mask;
}
fn _degree(mask) {
    let count = 0;
    while (mask) { count += mask & 1; mask = mask >> 1; }
    return count;
}
// Deterministic degree-first greedy coloring. No spills or promise of an
// optimal coloring; a failed coloring is an explicit error, not silent reuse.
// Unused/unreachable virtual registers map to -1.
fn color(code: list<list<int>>, capacity): list<int> {
    if (capacity < 1 || capacity > ir::REGISTERS) die("MLVM: register capacity must be 1..32");
    let graph = interference(code);
    let active = _active(code);
    let colors = ir::zeros(ir::REGISTERS);
    let degrees = ir::zeros(ir::REGISTERS);
    for (let reg = 0; reg < ir::REGISTERS; reg++) {
        colors[reg] = -1;
        degrees[reg] = _degree(graph[reg]);
    }
    for (let i = 0; i < ir::REGISTERS; i++) {
        let chosen = -1;
        let degree = -1;
        for (let reg = 0; reg < ir::REGISTERS; reg++) {
            if ((active & (1 << reg)) && colors[reg] < 0 && degrees[reg] > degree) {
                chosen = reg; degree = degrees[reg];
            }
        }
        if (chosen < 0) break;
        let used = 0;
        for (let reg = 0; reg < ir::REGISTERS; reg++) {
            if ((graph[chosen] & (1 << reg)) && colors[reg] >= 0) used |= 1 << colors[reg];
        }
        let available = 0;
        while (available < capacity && (used & (1 << available))) available++;
        if (available == capacity) die("MLVM: register allocation exhausted (spilling unavailable)");
        colors[chosen] = available;
    }
    return colors;
}

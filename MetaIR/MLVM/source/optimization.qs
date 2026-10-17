use ir, control_flow;

// State: [reachable, known-register bitmask, value(r0), ..., value(r31)].
// Unknown values are canonical zero, never an arbitrary stale constant.
fn _empty(): list<int> {
    let state = ir::zeros(ir::REGISTERS + 2);
    return state;
}
fn _result(row: list<int>, state: list<int>): list<int> {
    let op = row[0];
    if (op == ir::CONST) return [1, row[4]];
    if (op == ir::MOV && (state[1] & (1 << row[2]))) return [1, state[row[2] + 2]];
    if (ir::binary(op) && (state[1] & (1 << row[2])) && (state[1] & (1 << row[3]))) {
        let a = state[row[2] + 2];
        let b = state[row[3] + 2];
        if ((op == ir::DIV || op == ir::MOD) &&
            (b == 0 || a == -9223372036854775808 && b == -1)) return [0, 0];
        if ((op == ir::SHL || op == ir::SHR) && (b < 0 || b >= 64)) return [0, 0];
        return [1, ir::evaluate(op, a, b)];
    }
    return [0, 0];
}
fn _transfer(row: list<int>, state: list<int>): list<int> {
    let out = state.slice(0, state.len());
    if (ir::writes(row[0])) {
        let value = _result(row, state);
        let bit = 1 << row[1];
        out[1] &= ~bit;
        if (value[0]) out[1] |= bit;
        out[row[1] + 2] = value[1];
    }
    return out;
}
fn _rewrite(row: list<int>, state: list<int>): list<int> {
    if (ir::writes(row[0])) {
        let value = _result(row, state);
        if (value[0]) return [ir::CONST, row[1], 0, 0, value[1]];
    }
    return row.slice(0, 5);
}

// Local constant propagation: forget all facts at basic-block boundaries.
fn fold(code: list<list<int>>): list<list<int>> {
    control_flow::verify(code);
    let leaders = ir::zeros(code.len());
    for (my range in control_flow::blocks(code)) leaders[range[0]] = 1;
    let state = _empty();
    state[0] = 1;
    let out: list<list<int>> = [];
    for (let pc = 0; pc < code.len(); pc++) {
        if (leaders[pc]) { state = _empty(); state[0] = 1; }
        out.push(_rewrite(code[pc], state));
        state = _transfer(code[pc], state);
    }
    control_flow::verify(out);
    return out;
}

fn _merge(into: list<int>, state: list<int>) {
    if (!into[0]) {
        for (let i = 0; i < state.len(); i++) into[i] = state[i];
        return 1;
    }
    let mask = into[1] & state[1];
    for (let reg = 0; reg < ir::REGISTERS; reg++) {
        let bit = 1 << reg;
        if ((mask & bit) && into[reg + 2] != state[reg + 2]) mask &= ~bit;
    }
    if (mask == into[1]) return 0;
    into[1] = mask;
    for (let reg = 0; reg < ir::REGISTERS; reg++) {
        if (!(mask & (1 << reg))) into[reg + 2] = 0;
    }
    return 1;
}

// Forward must-constant analysis over ALL syntactic reachable CFG edges.
// Facts only weaken at joins: unequal constants or an unknown predecessor
// become unknown. Entry is always reachable with no register facts.
fn constants(code: list<list<int>>): list<list<int>> {
    control_flow::verify(code);
    let states: list<list<int>> = [];
    for (let pc = 0; pc < code.len(); pc++) states.push(_empty());
    states[0][0] = 1;
    let order = control_flow::reverse_postorder(code);
    let changed = 1;
    while (changed) {
        changed = 0;
        for (my pc in order) {
            if (!states[pc][0]) continue;
            let state = _transfer(code[pc], states[pc]);
            for (my target in control_flow::successors(code, pc)) {
                if (_merge(states[target], state)) changed = 1;
            }
        }
    }
    return states;
}
// Do not rewrite until convergence. Otherwise an early optimistic loop fact
// could become an incorrect constant when a later predecessor is discovered.
fn propagate(code: list<list<int>>): list<list<int>> {
    let states = constants(code);
    let out: list<list<int>> = [];
    for (let pc = 0; pc < code.len(); pc++) {
        if (states[pc][0]) out.push(_rewrite(code[pc], states[pc]));
        else out.push(code[pc].slice(0, 5));
    }
    control_flow::verify(out);
    return out;
}

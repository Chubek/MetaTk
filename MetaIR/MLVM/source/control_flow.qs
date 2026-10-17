use ir;

fn successors(code: list<list<int>>, pc): list<int> {
    let row = code[pc];
    if (row[0] == ir::RET) { let empty: list<int> = []; return empty; }
    if (row[0] == ir::JMP) return [row[4]];
    if (row[0] == ir::JZ && row[4] != pc + 1) return [row[4], pc + 1];
    return [pc + 1];
}

// Validate even unreachable instructions. Definite assignment below checks
// reachable paths using intersection, not union, at control-flow merges.
fn verify(code: list<list<int>>) {
    let n = code.len();
    if (n == 0 || n > ir::MAX_INSTRUCTIONS) die("MLVM: invalid module size");
    for (let pc = 0; pc < n; pc++) {
        let row = code[pc];
        if (row.len() != 5) die("MLVM: instruction must have five fields");
        let op = row[0];
        if (op < ir::CONST || op > ir::RET) die("MLVM: invalid opcode");
        for (let j = 1; j <= 3; j++) {
            if (row[j] < 0 || row[j] >= ir::REGISTERS) die("MLVM: register out of range");
        }
        if ((!ir::writes(op) && row[1] != 0) ||
            (!ir::reads_left(op) && row[2] != 0) ||
            (!ir::binary(op) && row[3] != 0) ||
            (op != ir::CONST && op != ir::JMP && op != ir::JZ && row[4] != 0))
            die("MLVM: noncanonical instruction");
        for (my target in successors(code, pc)) {
            if (target < 0 || target >= n) die("MLVM: branch or fallthrough out of range");
        }
    }
    let incoming = ir::zeros(n);
    let seen = ir::zeros(n);
    seen[0] = 1;
    let changed = 1;
    while (changed) {
        changed = 0;
        for (let pc = 0; pc < n; pc++) {
            if (!seen[pc]) continue;
            let row = code[pc];
            let mask = incoming[pc];
            if (ir::writes(row[0])) mask |= 1 << row[1];
            for (my target in successors(code, pc)) {
                let merged = seen[target] ? incoming[target] & mask : mask;
                if (!seen[target] || merged != incoming[target]) {
                    seen[target] = 1;
                    incoming[target] = merged;
                    changed = 1;
                }
            }
        }
    }
    for (let pc = 0; pc < n; pc++) {
        if (!seen[pc]) continue;
        let row = code[pc];
        let mask = incoming[pc];
        if (ir::reads_left(row[0]) && !(mask & (1 << row[2])))
            die("MLVM: read of undefined register at " ~ pc.str());
        if (ir::binary(row[0]) && !(mask & (1 << row[3])))
            die("MLVM: read of undefined register at " ~ pc.str());
    }
    return 0;
}

fn _reachable(code: list<list<int>>): list<int> {
    let seen = ir::zeros(code.len());
    let work = [0];
    seen[0] = 1;
    while (work.len()) {
        let pc = work.pop();
        for (my target in successors(code, pc)) {
            if (!seen[target]) { seen[target] = 1; work.push(target); }
        }
    }
    return seen;
}
fn reachable(code: list<list<int>>): list<int> {
    verify(code);
    return _reachable(code);
}

fn _predecessors(code: list<list<int>>): list<list<int>> {
    let out: list<list<int>> = [];
    for (let pc = 0; pc < code.len(); pc++) { let empty: list<int> = []; out.push(empty); }
    for (let pc = 0; pc < code.len(); pc++) {
        for (my target in successors(code, pc)) out[target].push(pc);
    }
    return out;
}
// Includes structurally valid unreachable edges. Reachability is a separate
// fact; clients must not mistake an unreachable predecessor for an entry path.
fn predecessors(code: list<list<int>>): list<list<int>> {
    verify(code);
    return _predecessors(code);
}

// Iterative DFS avoids consuming QScript's call stack on long CFGs.
fn _order(code: list<list<int>>): list<int> {
    let seen = ir::zeros(code.len());
    let cursor = ir::zeros(code.len());
    let stack = [0];
    let post: list<int> = [];
    seen[0] = 1;
    while (stack.len()) {
        let pc = stack.last();
        let edges = successors(code, pc);
        if (cursor[pc] < edges.len()) {
            let target = edges[cursor[pc]];
            cursor[pc]++;
            if (!seen[target]) { seen[target] = 1; stack.push(target); }
        } else { stack.pop(); post.push(pc); }
    }
    post.reverse();
    return post;
}
fn reverse_postorder(code: list<list<int>>): list<int> {
    verify(code);
    return _order(code);
}

fn blocks(code: list<list<int>>): list<list<int>> {
    verify(code);
    let leaders = ir::zeros(code.len());
    leaders[0] = 1;
    for (let pc = 0; pc < code.len(); pc++) {
        let op = code[pc][0];
        if (op == ir::JMP || op == ir::JZ) leaders[code[pc][4]] = 1;
        if (op >= ir::JMP && pc + 1 < code.len()) leaders[pc + 1] = 1;
    }
    let out: list<list<int>> = [];
    let start = 0;
    for (let pc = 1; pc < code.len(); pc++) {
        if (leaders[pc]) { out.push([start, pc]); start = pc; }
    }
    out.push([start, code.len()]);
    return out;
}

fn _intersect(a, b, idom: list<int>, rank: list<int>) {
    while (a != b) {
        if (rank[a] > rank[b]) a = idom[a];
        else b = idom[b];
    }
    return a;
}
// Cooper-style immediate dominators in reverse postorder: O(V+E) storage,
// unlike an instruction-by-instruction matrix of dominator sets.
fn immediate_dominators(code: list<list<int>>): list<int> {
    verify(code);
    let order = _order(code);
    let preds = _predecessors(code);
    let rank = ir::zeros(code.len());
    let idom = ir::zeros(code.len());
    for (let pc = 0; pc < code.len(); pc++) { rank[pc] = -1; idom[pc] = -1; }
    for (let i = 0; i < order.len(); i++) rank[order[i]] = i;
    idom[0] = 0;
    let changed = 1;
    while (changed) {
        changed = 0;
        for (let i = 1; i < order.len(); i++) {
            let pc = order[i];
            let parent = -1;
            for (my pred in preds[pc]) {
                if (idom[pred] < 0) continue;
                parent = parent < 0 ? pred : _intersect(parent, pred, idom, rank);
            }
            if (idom[pc] != parent) { idom[pc] = parent; changed = 1; }
        }
    }
    return idom;
}

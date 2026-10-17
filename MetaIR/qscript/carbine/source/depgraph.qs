// The dependency graph over targets 0..count-1. The host describes it with
// carbine::target_ndeps(t) and carbine::target_dep(t, k): target t depends
// on the targets those calls name. Nothing here knows target names.
//
// The algorithms are Kahn's topological sort and a closure over
// dependencies. Both are O(count^2) or better; Carbinefiles are small.

use cfg, ffi;

// Index of the first target with no unplaced dependencies, or -1.
fn ready(count, pending) {
    for (let t = 0; t < count; t++) if (vec_get(pending, t) == 0) return t;
    return -1;
}

// Placing t removes one unplaced dependency from every target that
// depends on t. Placed targets hold -1 and are left alone.
fn release(count, pending, t) {
    for (let u = 0; u < count; u++) {
        for (let k = 0; k < carbine::target_ndeps(u); k++) {
            if (carbine::target_dep(u, k) == t && vec_get(pending, u) > 0)
                vec_set(pending, u, vec_get(pending, u) - 1);
        }
    }
    return 0;
}

// Topological order. pending must be a vector of count entries; on return
// pending[t] is -1 for every placed target, and order[0..placed) lists the
// targets with each one after all of its dependencies. Returns the number
// placed. Fewer than count means a dependency cycle.
fn topo_order(count, order, pending) {
    let placed = 0;
    for (let t = 0; t < count; t++) vec_set(pending, t, carbine::target_ndeps(t));
    let pick = ready(count, pending);
    while (pick >= 0) {
        vec_set(pending, pick, -1);
        vec_set(order, placed, pick);
        placed++;
        release(count, pending, pick);
        pick = ready(count, pending);
    }
    return placed;
}

// Reports every target that topo_order could not place.
fn report_cycle(count, pending) {
    for (let t = 0; t < count; t++) {
        if (vec_get(pending, t) > 0) carbine::message(MSG_CYCLE, t);
    }
    return 0;
}

// Closure over dependencies. mark[t] is 1 for each target asked for; on
// return it is 1 for every target those need, transitively. Each target is
// pushed at most once, so count slots of stack are enough.
fn close_over_deps(count, mark) {
    let stack = vec_new(count);
    let top = 0;
    for (let t = 0; t < count; t++) {
        if (vec_get(mark, t) == 1) {
            vec_set(stack, top, t);
            top++;
        }
    }
    while (top > 0) {
        top--;
        let t = vec_get(stack, top);
        for (let k = 0; k < carbine::target_ndeps(t); k++) {
            let d = carbine::target_dep(t, k);
            if (vec_get(mark, d) == 0) {
                vec_set(mark, d, 1);
                vec_set(stack, top, d);
                top++;
            }
        }
    }
    vec_free(stack);
    return 0;
}

// `carbine build`: runs the selected targets in dependency order.
//
// A target is rebuilt when it is stale: its output is missing, its newest
// source is newer than the output, or one of its dependencies was rebuilt
// in this run. A target whose dependency failed is skipped. Without -k the
// run stops at the first failure.

use cfg, ffi, depgraph, loader;

// True when t has to run. The host reports modification times (0 = missing).
fn is_stale(t, rebuilt) {
    let out = carbine::target_output_mtime(t);
    if (out == 0) return 1;
    if (carbine::target_input_mtime(t) > out) return 1;
    for (let k = 0; k < carbine::target_ndeps(t); k++) {
        if (vec_get(rebuilt, carbine::target_dep(t, k)) == 1) return 1;
    }
    return 0;
}

// True when one of t's dependencies failed or was skipped.
fn dep_failed(t, failed) {
    for (let k = 0; k < carbine::target_ndeps(t); k++) {
        if (vec_get(failed, carbine::target_dep(t, k)) == 1) return 1;
    }
    return 0;
}

// Runs the marked targets in the order given. Returns the number of
// targets that failed. Skipped targets are not counted; they follow a
// failure that is.
fn run_order(count, order, mark, rebuilt, failed, keep_going) {
    let failures = 0;
    for (let p = 0; p < count; p++) {
        let t = vec_get(order, p);
        if (vec_get(mark, t) == 0) continue;
        if (dep_failed(t, failed) == 1) {
            vec_set(failed, t, 1);
            carbine::status(SAY_SKIPPED, t);
        } elsif (is_stale(t, rebuilt) == 1) {
            carbine::status(SAY_BUILDING, t);
            if (carbine::build_target(t) == 0) {
                vec_set(rebuilt, t, 1);
            } else {
                vec_set(failed, t, 1);
                carbine::message(MSG_FAILED, t);
                failures++;
                if (keep_going == 0) return failures;
            }
        } else {
            carbine::status(SAY_UP_TO_DATE, t);
        }
    }
    return failures;
}

// `carbine build [TARGET...]`: with no targets, builds everything.
fn cmd_build() {
    let count = load_spec();
    let order = vec_new(count);
    let pending = vec_new(count);
    if (topo_order(count, order, pending) < count) {
        report_cycle(count, pending);
        return 1;
    }
    let mark = vec_new(count);
    vec_fill(mark, 0);
    let asked = carbine::arg_count();
    if (asked == 0) vec_fill(mark, 1);
    for (let a = 0; a < asked; a++) {
        let t = carbine::arg_target(a);
        if (t < 0) {
            carbine::unknown_arg(a);
            return 2;
        }
        vec_set(mark, t, 1);
    }
    close_over_deps(count, mark);
    let rebuilt = vec_new(count);
    vec_fill(rebuilt, 0);
    let failed = vec_new(count);
    vec_fill(failed, 0);
    let failures = run_order(count, order, mark, rebuilt, failed, carbine::option(OPT_KEEP_GOING));
    if (failures == 0) {
        carbine::message(MSG_FINISHED, -1);
        return 0;
    }
    return 1;
}

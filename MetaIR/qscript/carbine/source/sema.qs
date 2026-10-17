// `carbine check` and `carbine lint`.
//
// check: the host rejects unknown dependencies and missing or misplaced
// sources while it loads the Carbinefile; this adds the graph check, which
// is the cycle test.
// lint: style checks on every source file (see tooling/lint.c).

use cfg, ffi, depgraph, loader;

fn cmd_check() {
    let count = load_spec();
    let order = vec_new(count);
    let pending = vec_new(count);
    if (topo_order(count, order, pending) < count) {
        report_cycle(count, pending);
        return 1;
    }
    carbine::message(MSG_VALID, -1);
    return 0;
}

fn cmd_lint() {
    load_spec();
    if (carbine::lint() == 0) return 0;
    carbine::message(MSG_LINT, -1);
    return 1;
}

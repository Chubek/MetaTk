// `carbine graph`: each target followed by its dependency edges, in build
// order. The host prints the names, so this file only walks the graph.

use cfg, ffi, depgraph, loader;

fn cmd_graph() {
    let count = load_spec();
    let order = vec_new(count);
    let pending = vec_new(count);
    if (topo_order(count, order, pending) < count) {
        report_cycle(count, pending);
        return 1;
    }
    for (let p = 0; p < count; p++) {
        let t = vec_get(order, p);
        carbine::show_target(t);
        for (let k = 0; k < carbine::target_ndeps(t); k++) carbine::show_edge(t, k);
    }
    return 0;
}

// `carbine gen BACKEND`: writes build.ninja, Makefile, CMakeLists.txt or
// meson.build into the project root. A cyclic graph is refused first, since
// every backend would fail on it later with a worse message.

use cfg, ffi, depgraph, loader;

fn cmd_gen() {
    let count = load_spec();
    let order = vec_new(count);
    let pending = vec_new(count);
    if (topo_order(count, order, pending) < count) {
        report_cycle(count, pending);
        return 1;
    }
    return carbine::emit(carbine::gen_backend());
}

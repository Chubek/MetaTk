// C extension modules named with --module. Their natives appear as
// <module>::<name> and are callable from any engine file once loaded.

fn load_plugins() {
    return carbine::module_load();
}

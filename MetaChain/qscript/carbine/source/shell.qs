// Carbine engine entry point. The host runs this program with the natives
// registered; the value it returns is the process exit status.
//
//   carbine build [TARGET...]    carbine graph      carbine check
//   carbine lint                 carbine gen BACKEND
//   carbine dist OUT.tar         carbine fetch URL DEST
//   carbine version              (help is printed by the host)

use cfg, loader, exec, dump, sema, gen, module, ffi;

fn cmd_dist() {
    load_spec();
    return carbine::dist();
}

fn cmd_fetch() {
    return carbine::fetch();
}

load_plugins();
let command = carbine::command();
if (command == CMD_BUILD) return cmd_build();
if (command == CMD_GRAPH) return cmd_graph();
if (command == CMD_CHECK) return cmd_check();
if (command == CMD_LINT) return cmd_lint();
if (command == CMD_GEN) return cmd_gen();
if (command == CMD_DIST) return cmd_dist();
if (command == CMD_FETCH) return cmd_fetch();
if (command == CMD_VERSION) return carbine::version();
return 0;

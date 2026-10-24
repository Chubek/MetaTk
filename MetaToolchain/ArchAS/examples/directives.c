#include "ArchASPlugin.h"
#include <string.h>

/* .pair EXPR emits two bytes through the normal assembler pipeline. */
static int pair(void *user, const ArchASInvocation *inv, const ArchASEmitter *out) {
    (void)user;
    if (!inv->arguments_length) {
        out->error(out->context, ".pair requires an expression");
        return -1;
    }
    if (out->emit(out->context, ".byte ", 6) ||
        out->emit(out->context, inv->arguments, inv->arguments_length) ||
        out->emit(out->context, ",", 1) ||
        out->emit(out->context, inv->arguments, inv->arguments_length) ||
        out->emit(out->context, "\n", 1)) return -1;
    return 0;
}
static int init(const ArchASHost *host, void **state) {
    (void)state;
    if (host->abi_version != ARCHAS_PLUGIN_ABI || host->struct_size < sizeof *host) return -1;
    return host->register_directive(host->context, "pair", pair, NULL);
}
ARCHAS_PLUGIN_EXPORT const ArchASPlugin *archas_plugin_v1(void) {
    static const ArchASPlugin plugin = {ARCHAS_PLUGIN_ABI, sizeof(ArchASPlugin), "pair", init, NULL};
    return &plugin;
}

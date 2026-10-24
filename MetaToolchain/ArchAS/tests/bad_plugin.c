#include "ArchASPlugin.h"
ARCHAS_PLUGIN_EXPORT const ArchASPlugin *archas_plugin_v1(void) {
    static const ArchASPlugin plugin = {999, sizeof(ArchASPlugin), "wrong-version", NULL, NULL};
    return &plugin;
}

/* Carbine plugin helpers. Include this in a module's source file, then:
 *
 *     CB_MODULE("geo", "0.1.0")
 *
 *     static int area(const int64_t *a, size_t n, int64_t *r, void *u) {
 *         (void)u; if (n != 2) return CB_INVALID_ARGUMENT;
 *         *r = a[0] * a[1]; return CB_OK;
 *     }
 *
 *     int carbine_module_init(const cb_module_host *host) {
 *         return host->add_native("area", 2, area, NULL) == CB_OK ? 0 : 1;
 *     }
 *
 * CB_MODULE defines carbine_module_info. The module still defines
 * carbine_module_init itself. */
#ifndef CARBINE_PLUGIN_H
#define CARBINE_PLUGIN_H

#include "Carbine-Module.h"

#define CB_MODULE(NAME, VERSION)                                              \
    static const cb_module_info cb_module_info_value = {                      \
        CARBINE_API_VERSION, NAME, VERSION};                                  \
    const cb_module_info *carbine_module_info(void) {                         \
        return &cb_module_info_value;                                         \
    }

#endif

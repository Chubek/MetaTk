/* Carbine C extension API: loading modules.
 *
 * A module is a shared object exporting two functions:
 *
 *     const cb_module_info *carbine_module_info(void);
 *     int carbine_module_init(const cb_module_host *host);
 *
 * carbine_module_init returns 0 on success and registers its natives with
 * host->add_native. Each native becomes the QScript function
 * "<module>::<name>". Modules are loaded once and stay loaded for the life
 * of the process. See Carbine-Plugin.h for a macro that writes the exports. */
#ifndef CARBINE_MODULE_H
#define CARBINE_MODULE_H

#include "Carbine-API.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct cb_module_info {
    int abi;                /* must equal CARBINE_API_VERSION */
    const char *name;       /* the QScript namespace of the module's natives */
    const char *version;
} cb_module_info;

typedef struct cb_module_host {
    int abi;
    const char *module;     /* name of the module being initialised */
    /* Registers "<module>::<name>". Only valid during carbine_module_init. */
    cb_status (*add_native)(const char *name, size_t arity, cb_native_fn fn, void *user);
    /* Writes a line to the Carbine diagnostic stream (stderr). */
    void (*log)(const char *message);
} cb_module_host;

typedef const cb_module_info *(*cb_module_info_fn)(void);
typedef int (*cb_module_init_fn)(const cb_module_host *host);

/* Loads the shared object at path, checks its ABI and runs its init. */
cb_status cb_module_load(const char *path);
/* Number of modules loaded so far. */
size_t cb_module_count(void);
/* Name of the i-th loaded module, or NULL. */
const char *cb_module_name(size_t index);

#ifdef __cplusplus
}
#endif
#endif

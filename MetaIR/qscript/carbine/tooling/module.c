/* Carbine tooling: the native registry and the C extension module loader.
 *
 * The registry is what the host hands to QScript (qs_register_function).
 * Built-in natives and module natives are kept side by side; a module's
 * natives are named "<module>::<name>". */
#define _GNU_SOURCE

#include "internal.h"

#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>

static cb_registered *registry;
static size_t registry_count, registry_cap;

static struct {
    size_t count;
    char *names[64];
} loaded_modules;

/* The module whose carbine_module_init is running. */
static const char *current_module;

cb_status cb_registry_add(const char *name, size_t arity, cb_native_fn fn, void *user) {
    size_t i;
    cb_registered *r;
    for (i = 0; i < registry_count; ++i) {
        if (strcmp(registry[i].native.name, name) == 0) {
            cb_set_error("native registered twice: %s", name);
            return CB_INVALID_ARGUMENT;
        }
    }
    if (registry_count == registry_cap) {
        size_t cap = registry_cap ? registry_cap * 2 : 32;
        cb_registered *grown = realloc(registry, cap * sizeof *grown);
        if (!grown) return CB_ERROR;
        registry = grown;
        registry_cap = cap;
    }
    r = &registry[registry_count];
    r->native.name = cb_strdup(name);
    if (!r->native.name) return CB_ERROR;
    r->native.arity = arity;
    r->native.fn = fn;
    r->user = user;
    ++registry_count;
    return CB_OK;
}

size_t cb_registry_count(void) { return registry_count; }

const cb_registered *cb_registry_get(size_t index) {
    return index < registry_count ? &registry[index] : NULL;
}

static void host_log(const char *message) { fprintf(stderr, "carbine: %s\n", message); }

static cb_status host_add_native(const char *name, size_t arity, cb_native_fn fn, void *user) {
    char *full;
    cb_status st;
    size_t n;
    if (!current_module) {
        cb_set_error("add_native called outside module init");
        return CB_ERROR;
    }
    n = strlen(current_module) + 2 + strlen(name) + 1;
    full = malloc(n);
    if (!full) return CB_ERROR;
    snprintf(full, n, "%s::%s", current_module, name);
    st = cb_registry_add(full, arity, fn, user);
    free(full);
    return st;
}

static cb_module_host module_host = {CARBINE_API_VERSION, NULL, host_add_native, host_log};

cb_status cb_module_load(const char *path) {
    void *handle;
    cb_module_info_fn info_fn;
    cb_module_init_fn init_fn;
    const cb_module_info *info;
    int rc;
    size_t i;

    handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        cb_set_error("cannot load module %s: %s", path, dlerror());
        return CB_NOT_FOUND;
    }
    info_fn = (cb_module_info_fn)dlsym(handle, "carbine_module_info");
    init_fn = (cb_module_init_fn)dlsym(handle, "carbine_module_init");
    if (!info_fn || !init_fn) {
        cb_set_error("%s does not export carbine_module_info and carbine_module_init", path);
        return CB_INVALID_ARGUMENT;
    }
    info = info_fn();
    if (!info || info->abi != CARBINE_API_VERSION || !info->name) {
        cb_set_error("%s was built for a different Carbine API", path);
        return CB_UNSUPPORTED;
    }
    for (i = 0; i < loaded_modules.count; ++i)
        if (strcmp(loaded_modules.names[i], info->name) == 0) {
            cb_set_error("module %s loaded twice", info->name);
            return CB_INVALID_ARGUMENT;
        }
    if (loaded_modules.count == 64) {
        cb_set_error("too many modules");
        return CB_ERROR;
    }

    module_host.module = info->name;
    current_module = info->name;
    rc = init_fn(&module_host);
    current_module = NULL;
    if (rc != 0) {
        cb_set_error("module %s failed to initialise (code %d)", info->name, rc);
        return CB_ERROR;
    }
    loaded_modules.names[loaded_modules.count] = cb_strdup(info->name);
    if (!loaded_modules.names[loaded_modules.count]) return CB_ERROR;
    ++loaded_modules.count;
    return CB_OK;
}

size_t cb_module_count(void) { return loaded_modules.count; }

const char *cb_module_name(size_t index) {
    return index < loaded_modules.count ? loaded_modules.names[index] : NULL;
}

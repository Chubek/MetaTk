#ifndef METAARCH_ARCHAS_PLUGIN_H
#define METAARCH_ARCHAS_PLUGIN_H

/* C11 directive plugin ABI v1. No MetaArch/C++ headers or link dependency.
 * Export: const ArchASPlugin *archas_plugin_v1(void).
 * Every pointer in an invocation is borrowed until the callback returns.
 * Return 0 on success, nonzero on failure; report a message through error().
 * Callbacks must not throw across this ABI or retain an emitter/invocation.
 */
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ARCHAS_PLUGIN_ABI 1u
/* The default-visibility POSIX build needs no compiler extension. A plugin
 * using hidden visibility may supply its own export annotation at build time. */
#ifndef ARCHAS_PLUGIN_EXPORT
#define ARCHAS_PLUGIN_EXPORT
#endif

typedef struct ArchASEmitter {
    void *context;
    int (*emit)(void *context, const char *text, size_t length);
    void (*error)(void *context, const char *message);
} ArchASEmitter;

typedef struct ArchASInvocation {
    const char *arguments;
    size_t arguments_length;
    const char *file;
    size_t line, column;
    const char *target;
    const char *syntax; /* "att" or "intel" */
} ArchASInvocation;

typedef int (*ArchASDirective)(void *user, const ArchASInvocation *, const ArchASEmitter *);

typedef struct ArchASHost {
    uint32_t abi_version;
    size_t struct_size;
    void *context;
    /* Registration is allowed only during init. The name is copied. */
    int (*register_directive)(void *context, const char *name, ArchASDirective callback, void *user);
    void (*error)(void *context, const char *message);
    const char *target;
    const char *syntax;
} ArchASHost;

typedef struct ArchASPlugin {
    uint32_t abi_version;
    size_t struct_size;
    const char *name;
    int (*init)(const ArchASHost *host, void **state);
    /* Called before unloading, including failed init when *state is non-NULL.
       The host stays alive until shutdown returns. Must not call directives. */
    void (*shutdown)(void *state);
} ArchASPlugin;

typedef const ArchASPlugin *(*ArchASPluginEntry)(void);
ARCHAS_PLUGIN_EXPORT const ArchASPlugin *archas_plugin_v1(void);

#ifdef __cplusplus
}
#endif
#endif

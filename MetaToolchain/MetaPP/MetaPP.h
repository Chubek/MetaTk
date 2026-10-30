/* MetaPP: C11 interface to the preprocessing engine. */
#ifndef METAPP_H
#define METAPP_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct metapp_context metapp_context;
/* Contexts own macros and search paths. Independent contexts may be used
 * concurrently; serialize calls on the same context. */
metapp_context *metapp_create(void);
void metapp_destroy(metapp_context *ctx);
/* Return 1 on success, 0 on failure. Text arguments are copied. Definitions
 * use directive syntax ("NAME value" or "F(x) x"), without #define. */
int metapp_define(metapp_context *ctx, const char *definition);
int metapp_undefine(metapp_context *ctx, const char *name);
int metapp_add_include_path(metapp_context *ctx, const char *path);
/* Output is allocated and must be released with metapp_free. On failure
 * *output is NULL. filename supplies diagnostics and include resolution.
 * Macros persist between calls; pragma-once state does not. */
int metapp_preprocess(metapp_context *ctx, const char *source,
                      const char *filename, char **output);
/* Length-delimited input; rejects embedded NUL bytes instead of truncating.
 * source must be non-NULL even when length is zero. Same ownership and
 * error behavior as metapp_preprocess. */
int metapp_preprocess_n(metapp_context *ctx, const char *source, size_t length,
                        const char *filename, char **output);
int metapp_preprocess_file(metapp_context *ctx, const char *filename, char **output);
void metapp_free(void *memory);
/* Borrowed text, valid until the next operation. May contain warnings on
 * success. A NULL context returns a fixed error string. */
const char *metapp_diagnostics(const metapp_context *ctx);
#ifdef __cplusplus
}
#endif
#endif

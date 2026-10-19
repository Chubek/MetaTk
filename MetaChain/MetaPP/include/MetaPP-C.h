#ifndef METAPP_C_H
#define METAPP_C_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define METAPP_ABI_VERSION 1

typedef struct metapp_context metapp_context;

/* The callback borrows arguments and filename for the duration of the call.
 * Return a NUL-terminated token source, valid until the next callback invocation,
 * or NULL to report failure. The caller owns userdata and its lifetime. */
typedef const char* (*metapp_kernel_fn)(const char* arguments, const char* filename,
                                       size_t line, void* userdata);

unsigned metapp_abi_version(void);
/* NULL indicates allocation failure or invalid include-path arguments. */
metapp_context* metapp_create(const char* const* include_paths, size_t count);
void metapp_destroy(metapp_context* context);

/* Mutators return 0 on success and 1 on error. Never throw C++ exceptions.
 * A context is not safe for concurrent or reentrant calls.
 * Macro definitions persist between preprocessing calls. */
int metapp_define(metapp_context* context, const char* definition);
int metapp_undefine(metapp_context* context, const char* name);
int metapp_process(metapp_context* context, const char* source, const char* filename);
int metapp_process_file(metapp_context* context, const char* path);
int metapp_register_kernel(metapp_context* context, const char* name,
                           metapp_kernel_fn callback, void* userdata);

/* Borrowed strings: output is valid until the next process call or destruction;
 * last_error until the next mutator or destruction. Errors clear process output.
 * Diagnostics are truncated to 2047 bytes. Neither accessor returns NULL. */
const char* metapp_output(const metapp_context* context);
const char* metapp_last_error(const metapp_context* context);
size_t metapp_warning_count(const metapp_context* context);

#ifdef __cplusplus
}
#endif
#endif

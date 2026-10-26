#ifndef GPPX_PUBLIC_H
#define GPPX_PUBLIC_H
/* GPPX v1 plugin ABI. All returned C strings are malloc-allocated; release with gppx_free().
   Callbacks return 0 on success and nonzero on failure. The host copies callback results. */
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
#define GPPX_ABI_VERSION 1u
#if defined(_WIN32) && defined(GPPX_BUILD_SHARED)
#define GPPX_API __declspec(dllexport)
#elif defined(__GNUC__) && defined(GPPX_BUILD_SHARED)
#define GPPX_API __attribute__((visibility("default")))
#else
#define GPPX_API
#endif

typedef struct GPPX_State GPPX_State;
typedef int (*GPPX_DirectiveFn)(GPPX_State*,const char *arguments,char **replacement,void *userdata);
typedef int (*GPPX_MacroFn)(GPPX_State*,int argc,const char *const *argv,char **replacement,void *userdata);
typedef int (*GPPX_RewriteFn)(GPPX_State*,const char *line,char **replacement,void *userdata);
typedef int (*GPPX_ReaderFn)(GPPX_State*,const char *path,char **contents,void *userdata);
typedef int (*GPPX_SinkFn)(GPPX_State*,const char *destination,const char *data,size_t size,void *userdata);
typedef int (*GPPX_EmbedFn)(GPPX_State*,const unsigned char *bytes,size_t size,char **replacement,void *userdata);
typedef int (*GPPX_SyntaxFn)(GPPX_State*,const char *value,void *userdata);
typedef int (*GPPX_PMatchFn)(GPPX_State*,const char *pattern,const char *subject,void *userdata); /* >0 match, 0 miss, <0 error */

typedef struct GPPX_HostAPI {
 unsigned abi_version;
 int (*register_directive)(GPPX_State*,const char*,GPPX_DirectiveFn,void*);
 int (*register_macro)(GPPX_State*,const char*,GPPX_MacroFn,void*);
 int (*register_rewriter)(GPPX_State*,GPPX_RewriteFn,void*);
 int (*register_reader)(GPPX_State*,const char*,GPPX_ReaderFn,void*);
 int (*register_sink)(GPPX_State*,const char*,GPPX_SinkFn,void*);
 int (*register_embed)(GPPX_State*,const char*,GPPX_EmbedFn,void*);
 int (*register_syntax)(GPPX_State*,const char*,GPPX_SyntaxFn,void*);
 int (*register_pmatch)(GPPX_State*,const char*,GPPX_PMatchFn,void*);
 int (*define)(GPPX_State*,const char*,const char*);
 int (*set_option)(GPPX_State*,const char*,const char*);
 void (*diagnostic)(GPPX_State*,const char*);
} GPPX_HostAPI;
/* Plugins export this exact, unmangled C symbol. A plugin must not retain the host pointer. */
typedef int (*GPPX_PluginInit)(const GPPX_HostAPI*,GPPX_State*);

GPPX_API GPPX_State *gppx_create(void);
GPPX_API void gppx_destroy(GPPX_State*);
GPPX_API int gppx_process(GPPX_State*,const char *input,const char *virtual_path,char **output);
GPPX_API int gppx_process_file(GPPX_State*,const char *path,char **output);
GPPX_API int gppx_define(GPPX_State*,const char *name,const char *replacement);
GPPX_API int gppx_undef(GPPX_State*,const char *name);
GPPX_API int gppx_add_include_path(GPPX_State*,const char *directory);
GPPX_API int gppx_set_option(GPPX_State*,const char *key,const char *value);
GPPX_API int gppx_load_plugin(GPPX_State*,const char *library);
GPPX_API int gppx_register_directive(GPPX_State*,const char*,GPPX_DirectiveFn,void*);
GPPX_API int gppx_register_macro(GPPX_State*,const char*,GPPX_MacroFn,void*);
GPPX_API int gppx_register_rewriter(GPPX_State*,GPPX_RewriteFn,void*);
GPPX_API int gppx_register_reader(GPPX_State*,const char*,GPPX_ReaderFn,void*);
GPPX_API int gppx_register_sink(GPPX_State*,const char*,GPPX_SinkFn,void*);
GPPX_API int gppx_register_embed(GPPX_State*,const char*,GPPX_EmbedFn,void*);
GPPX_API int gppx_register_syntax(GPPX_State*,const char*,GPPX_SyntaxFn,void*);
GPPX_API int gppx_register_pmatch(GPPX_State*,const char*,GPPX_PMatchFn,void*);
GPPX_API const char *gppx_last_error(const GPPX_State*);
GPPX_API const char *gppx_diagnostics(const GPPX_State*);
GPPX_API void gppx_free(void*);
/* Useful to extensions producing replacement output. */
GPPX_API char *gppx_strdup(const char*);
#ifdef __cplusplus
}
#endif
#endif

#ifndef METATEX_LMTEX_H
#define METATEX_LMTEX_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct lmtex_engine lmtex_engine;
typedef struct lmtex_result {
    char* output;
    size_t output_size;
    char* diagnostics;
    int status; /* 0 success, 1 source errors, 2 API/runtime error. */
} lmtex_result;
lmtex_engine* lmtex_create(void);
void lmtex_destroy(lmtex_engine* engine);
/* No C++ exception crosses this ABI. Input is length-delimited UTF-8.
 * Each result must be released, including after errors. fragment!=0 preserves
 * definitions. New engines use the native plain subset, without plugins. */
lmtex_result lmtex_compile(lmtex_engine* engine, const char* source, size_t length,
    const char* filename, const char* device, int fragment);
void lmtex_release(lmtex_result* result);
struct lua_State;
int luaopen_lmtex(struct lua_State* state);
#ifdef __cplusplus
}
#endif
#endif

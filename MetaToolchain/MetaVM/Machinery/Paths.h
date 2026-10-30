#ifndef METAVM_PATHS_H
#define METAVM_PATHS_H
#include "System.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Lexical '/' join only: no filesystem access or normalization of '..'.
 * An absolute leaf replaces base. Output is allocated; release with free(). */
mvm_status mvm_path_join(const char *base, const char *leaf, char **output);
#ifdef __cplusplus
}
#endif
#endif

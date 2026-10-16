#ifndef METACHAIN_QSCRIPT_LIBRARY_H
#define METACHAIN_QSCRIPT_LIBRARY_H
// QScript libraries: the embedded standard library and how `use NAME;`
// finds library source.
//
// A library is an ordinary QScript source that contains only `fn`, `const`
// and `use` declarations. `use NAME;` makes its public names (those not
// starting with `_`) callable unqualified, or as NAME::f. Lookup order:
//   1. sources added with qs_add_library (or qs_module_add_library)
//   2. the resolver callback, if set
//   3. each directory added with qs_add_library_path: DIR/NAME.qs, with
//      `::` in NAME mapped to `/` (so `use text::ascii;` reads text/ascii.qs)
//   4. the embedded standard library (unless disabled with qs_set_stdlib)
// Nothing reads the filesystem unless a library path was added.
#include "qscript/QScript.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Embedded standard library, sorted by name. */
size_t qs_stdlib_count(void);
const char *qs_stdlib_name(size_t index);                         /* NULL if out of range */
const char *qs_stdlib_source(const char *name, size_t *length);   /* NULL if unknown */

qs_status qs_add_library(qs_context *, const char *name, const char *source, size_t length);
qs_status qs_add_library_path(qs_context *, const char *directory);
qs_status qs_set_stdlib(qs_context *, int enable);
/* Return library text (valid until the compile call returns) or NULL. */
typedef const char *(*qs_library_resolver)(const char *name, size_t *length, void *user);
qs_status qs_set_library_resolver(qs_context *, qs_library_resolver, void *user);
/* Forget added libraries, paths and the resolver; re-enable the stdlib. */
qs_status qs_clear_libraries(qs_context *);

#ifdef __cplusplus
}
#endif
#endif

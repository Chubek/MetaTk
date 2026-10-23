/** @file MetaML.h
 * C99 embedding API backed by the C++20 interpreter.
 * Link metaml-c; do not compile the C++ metalib/*.c files as C.
 * Sessions are independent, but each session requires external serialization.
 * No C++ exception crosses this API. This is not an isolation/security boundary.
 */
#ifndef METALIB_C_METAML_H
#define METALIB_C_METAML_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct metaml_c_session metaml_c_session;
typedef enum metaml_status {
    METAML_OK = 0,
    METAML_INVALID_ARGUMENT = 1,
    METAML_LANGUAGE_ERROR = 2,
    METAML_HOST_ERROR = 3
} metaml_status;

/* Owned, NUL-terminated bytes. size excludes the terminator; embedded NULs
 * are allowed. Initialize to {0}; free before reusing as an output argument.
 * On failure output arguments are set to {0}. Free with metaml_text_free only. */
typedef struct metaml_text { char *data; size_t size; } metaml_text;
void metaml_text_free(metaml_text *text);

metaml_status metaml_session_create(metaml_c_session **out);
void metaml_session_destroy(metaml_c_session *session); /* NULL is allowed */
const char *metaml_c_version(void); /* static lifetime */
/* Borrowed diagnostic text, valid until the next mutating call or destroy.
 * NULL session returns a static explanatory string. */
const char *metaml_last_error(const metaml_c_session *session);

/* Fresh sessions have only core primitives; standard library is explicit. */
metaml_status metaml_c_load_stdlib(metaml_c_session *session, const char *directory);
metaml_status metaml_install_metalib(metaml_c_session *session);
/* Replace the allowlist. Supports exact names and Module.*. Empty denies all
 * non-core primitives; installation alone never grants capabilities. */
metaml_status metaml_allow_primitives(metaml_c_session *session,
                                     const char *const *names, size_t count);
/* source may be NULL only when size is zero. eval returns canonical MetaML
 * value notation, not unquoted string contents. type_of never evaluates. */
metaml_status metaml_c_eval(metaml_c_session *session, const char *source, size_t size,
                         metaml_text *result);
metaml_status metaml_c_type_of(metaml_c_session *session, const char *source, size_t size,
                            metaml_text *result);
/* Copy captured IO.print/println output. clear!=0 clears only after success.
 * Core IO.eprint and input still use process streams. */
metaml_status metaml_output(metaml_c_session *session, metaml_text *result, int clear);
#ifdef __cplusplus
}
#endif
#endif

#ifndef XBIND_H
#define XBIND_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define XBIND_VERSION "0.1.0"

typedef enum xbind_status {
  XBIND_STATUS_OK = 0,
  XBIND_STATUS_INVALID_ARGUMENT = 1,
  XBIND_STATUS_NOT_FOUND = 2,
  XBIND_STATUS_BUFFER_TOO_SMALL = 3
} xbind_status;

typedef struct xbind_target_info {
  const char *name;
  const char *compiler_directory;
  const char *dombind_directory;
  const char *typedef_file;
  const char *native_boundary;
} xbind_target_info;

typedef void (*xbind_diagnostic_fn)(void *user, const char *message);

const char *xbind_version(void);
const char *xbind_status_string(xbind_status status);

size_t xbind_target_count(void);
const xbind_target_info *xbind_target_at(size_t index);
const xbind_target_info *xbind_find_target(const char *name);

xbind_status xbind_resolve_spec(const char *directory,
                                const char *library_name,
                                char *output,
                                size_t output_size);

size_t xbind_check_layout(const char *root,
                          xbind_diagnostic_fn diagnostic,
                          void *user);

#ifdef __cplusplus
}
#endif

#endif /* XBIND_H */

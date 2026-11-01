#ifndef METAVM_OS_H
#define METAVM_OS_H
#include "MetaBuffer.h"
#ifdef __cplusplus
extern "C" {
#endif
/* ISO C stdio only. Read appends atomically; limit bounds bytes read (0 uses
 * default). Write replaces a file; failure can leave a partial file. */
mvm_status mvm_read_file(const char *path, size_t limit, mvm_buffer *, mvm_error *);
mvm_status mvm_write_file(const char *path, const void *, size_t, mvm_error *);
/* Stage an exclusively created sibling, close it, then rename into place.
 * On failure the prior destination is preserved and staging cleanup is attempted.
 * Replacement is atomic on platforms whose rename replaces existing files
 * (POSIX); platforms refusing replacement return MVM_IO without deleting it.
 * No fsync/durability guarantee. Caller must control the destination directory. */
mvm_status mvm_write_file_atomic(const char *path, const void *, size_t, mvm_error *);
#ifdef __cplusplus
}
#endif
#endif

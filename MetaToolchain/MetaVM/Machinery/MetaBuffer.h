#ifndef METAVM_META_BUFFER_H
#define METAVM_META_BUFFER_H
#include "System.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Initialize with {0}, dispose once. No implicit NUL terminator. */
typedef struct mvm_buffer { unsigned char *data; size_t size, capacity; } mvm_buffer;
void mvm_buffer_dispose(mvm_buffer *);
/* Input must not alias the buffer. Failure preserves contents and size. */
mvm_status mvm_buffer_append(mvm_buffer *, const void *, size_t);
#ifdef __cplusplus
}
#endif
#endif

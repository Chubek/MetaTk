#ifndef METAVM_READER_H
#define METAVM_READER_H
#include "System.h"
#ifdef __cplusplus
extern "C" {
#endif
/* A bounded borrowed byte reader. Failed reads leave the cursor unchanged. */
typedef struct mvm_reader { const unsigned char *data; size_t size, offset; } mvm_reader;
mvm_status mvm_reader_init(mvm_reader *, const void *, size_t);
mvm_status mvm_reader_bytes(mvm_reader *, void *output, size_t);
mvm_status mvm_reader_u32(mvm_reader *, uint32_t *);
mvm_status mvm_reader_u64(mvm_reader *, uint64_t *);
#ifdef __cplusplus
}
#endif
#endif

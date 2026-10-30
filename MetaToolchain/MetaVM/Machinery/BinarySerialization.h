#ifndef METAVM_BINARY_SERIALIZATION_H
#define METAVM_BINARY_SERIALIZATION_H
#include "Module.h"
#include "MetaBuffer.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Versioned, little-endian, padding-free format. See README.md.
 * Serialize appends atomically to an initialized buffer. Deserialize consumes
 * the entire input, verifies it, sets *output=NULL on failure. Output is owned
 * by caller. max_bytes==0 selects the default module byte limit. */
mvm_status mvm_serialize(const mvm_module *, mvm_buffer *, mvm_error *);
mvm_status mvm_deserialize(const void *, size_t, size_t max_bytes,
    mvm_module **output, mvm_error *);
#ifdef __cplusplus
}
#endif
#endif

#ifndef METACHAIN_QSCRIPT_SERIALIZATION_H
#define METACHAIN_QSCRIPT_SERIALIZATION_H
#include "qscript/QScript.h"
#ifdef __cplusplus
extern "C" {
#endif
/* QSV1: four magic bytes, uint32 little-endian count, then count signed
 * two's-complement int64 little-endian values. No native padding/pointers.
 * NULL output with zero capacity queries required size/count. Short output
 * returns QS_INVALID_ARGUMENT without writing; required is still populated.
 * Input/output storage must not overlap. Decoding rejects trailing bytes. */
qs_status qs_values_encode(const int64_t *values, size_t count,
                           unsigned char *out, size_t capacity, size_t *required);
qs_status qs_values_decode(const unsigned char *data, size_t length,
                           int64_t *out, size_t capacity, size_t *required);
#ifdef __cplusplus
}
#endif
#endif

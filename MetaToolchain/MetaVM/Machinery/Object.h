#ifndef METAVM_OBJECT_H
#define METAVM_OBJECT_H
#include "Value.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum mvm_object_kind { MVM_STRING, MVM_ARRAY } mvm_object_kind;
/* Allocations can collect. Returned values are NOT host roots: use mvm_pin
 * before a later allocation. Strings are immutable byte sequences (may contain
 * NUL); arrays start as nil and accept only scalars or this VM's objects. */
mvm_status mvm_string_new(mvm_vm *, const void *bytes, size_t length, mvm_value *);
mvm_status mvm_array_new(mvm_vm *, size_t length, mvm_value *);
mvm_status mvm_object_type(const mvm_vm *, mvm_value, mvm_object_kind *);
mvm_status mvm_object_length(const mvm_vm *, mvm_value, size_t *);
/* Borrowed bytes, valid until the object is collected/VM destroyed. */
mvm_status mvm_string_data(const mvm_vm *, mvm_value, const unsigned char **, size_t *);
mvm_status mvm_array_get(const mvm_vm *, mvm_value, size_t index, mvm_value *);
mvm_status mvm_array_set(mvm_vm *, mvm_value, size_t index, mvm_value);
#ifdef __cplusplus
}
#endif
#endif

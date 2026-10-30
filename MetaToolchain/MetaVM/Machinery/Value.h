#ifndef METAVM_VALUE_H
#define METAVM_VALUE_H
#include "System.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum mvm_value_kind { MVM_NIL, MVM_BOOL, MVM_INT, MVM_OBJECT } mvm_value_kind;
typedef struct mvm_value {
    mvm_value_kind kind;
    union { int boolean; int64_t integer; mvm_object *object; } as;
} mvm_value;
mvm_value mvm_nil(void);
mvm_value mvm_bool(int value);
mvm_value mvm_int(int64_t value);
/* Scalar equality, object identity. Does not dereference objects. */
int mvm_value_equal(mvm_value left, mvm_value right);
/* nil and false are false; all other values (including integer zero) are true. */
int mvm_value_truthy(mvm_value value);
#ifdef __cplusplus
}
#endif
#endif

#ifndef METAVM_METAOBJECT_H
#define METAVM_METAOBJECT_H
#include "Object.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Immutable metadata describing the two built-in heap object layouts. */
typedef struct mvm_object_descriptor {
    mvm_object_kind kind;
    const char *name;
    int mutable_elements, traced_elements;
} mvm_object_descriptor;
const mvm_object_descriptor *mvm_object_describe(mvm_object_kind);
#ifdef __cplusplus
}
#endif
#endif

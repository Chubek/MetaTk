#ifndef METAVM_INTERNAL_H
#define METAVM_INTERNAL_H
#include "MetaVM.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

/* Stable wire format and signed-64 VM require these C11 optional types. */
_Static_assert(CHAR_BIT == 8, "MetaVM requires 8-bit bytes");
typedef struct mvm_function {
    char *name;
    uint32_t arity, registers;
    mvm_instruction *code;
    size_t count, capacity;
} mvm_function;
typedef struct mvm_import { char *name; uint32_t arity; } mvm_import;
struct mvm_module {
    mvm_function *functions;
    mvm_import *imports;
    size_t function_count, function_capacity, import_count, import_capacity;
};
struct mvm_object {
    mvm_object *next, *gray;
    mvm_object_kind kind;
    size_t length, bytes;
    int marked;
    void *data;
};
struct mvm_root {
    mvm_root *next, *previous;
    mvm_vm *vm;
    mvm_value value;
};
typedef struct mvm_binding {
    char *name;
    uint32_t arity;
    mvm_native callback;
    void *user;
} mvm_binding;
typedef struct mvm_frame {
    uint32_t function, destination;
    size_t pc;
    mvm_value *registers;
} mvm_frame;
struct mvm_vm {
    mvm_limits limits;
    mvm_object *objects;
    mvm_root *roots;
    size_t heap_bytes, object_count, root_count;
    mvm_binding *bindings;
    size_t binding_count, binding_capacity;
    mvm_frame *frames;
    size_t frame_count, frame_capacity, register_count;
    const mvm_module *module;
    mvm_value result;
    uint64_t steps;
    int busy;
};
mvm_status mvm_fail(mvm_error *, mvm_status, const char *format, ...);
int mvm_name_valid(const char *);
char *mvm_copy_string(const char *);
mvm_status mvm_grow(void **data, size_t *capacity, size_t need, size_t element);
int64_t mvm_from_bits(uint64_t);
int mvm_value_valid(const mvm_vm *, mvm_value);
mvm_object *mvm_owned_object(const mvm_vm *, mvm_value);
#endif

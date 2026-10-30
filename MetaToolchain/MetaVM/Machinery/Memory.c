#include "Internal.h"

mvm_object *mvm_owned_object(const mvm_vm *vm, mvm_value v) {
    mvm_object *o;
    if (!vm || v.kind != MVM_OBJECT || !v.as.object) return NULL;
    for (o = vm->objects; o; o = o->next) if (o == v.as.object) return o;
    return NULL;
}
int mvm_value_valid(const mvm_vm *vm, mvm_value v) {
    switch (v.kind) {
    case MVM_NIL: case MVM_INT: return 1;
    case MVM_BOOL: return v.as.boolean == 0 || v.as.boolean == 1;
    case MVM_OBJECT: return mvm_owned_object(vm, v) != NULL;
    default: return 0;
    }
}
static void mark_value(mvm_value v, mvm_object **gray) {
    mvm_object *o;
    if (v.kind != MVM_OBJECT) return;
    o = v.as.object; /* All stored values were ownership-checked on ingress. */
    if (o->marked) return;
    o->marked = 1;
    o->gray = *gray;
    *gray = o;
}
size_t mvm_collect(mvm_vm *vm) {
    mvm_object *gray = NULL, *o, **link;
    mvm_root *root;
    size_t i, j, count = 0;
    if (!vm) return 0;
    mark_value(vm->result, &gray);
    for (root = vm->roots; root; root = root->next) mark_value(root->value, &gray);
    for (i = 0; i < vm->frame_count; ++i) {
        const mvm_frame *f = &vm->frames[i];
        size_t n = vm->module->functions[f->function].registers;
        for (j = 0; j < n; ++j) mark_value(f->registers[j], &gray);
    }
    while (gray) {
        o = gray;
        gray = o->gray;
        if (o->kind == MVM_ARRAY) {
            const mvm_value *values = o->data;
            for (i = 0; i < o->length; ++i) mark_value(values[i], &gray);
        }
    }
    link = &vm->objects;
    while (*link) {
        o = *link;
        if (o->marked) { o->marked = 0; link = &o->next; }
        else {
            *link = o->next;
            vm->heap_bytes -= o->bytes;
            --vm->object_count;
            free(o->data); free(o); ++count;
        }
    }
    return count;
}
mvm_status mvm_pin(mvm_vm *vm, mvm_value v, mvm_root **out) {
    mvm_root *root;
    if (!out) return MVM_INVALID;
    *out = NULL;
    if (!vm || !mvm_value_valid(vm, v)) return MVM_INVALID;
    if (vm->root_count == SIZE_MAX) return MVM_LIMIT;
    root = malloc(sizeof(*root));
    if (!root) return MVM_OOM;
    root->vm = vm; root->value = v;
    root->previous = NULL; root->next = vm->roots;
    if (vm->roots) vm->roots->previous = root;
    vm->roots = root;
    ++vm->root_count;
    *out = root;
    return MVM_OK;
}
mvm_value mvm_root_value(const mvm_root *r) { return r ? r->value : mvm_nil(); }
void mvm_unpin(mvm_root *root) {
    if (!root) return;
    if (root->previous) root->previous->next = root->next;
    else root->vm->roots = root->next;
    if (root->next) root->next->previous = root->previous;
    --root->vm->root_count;
    free(root);
}
static mvm_status object_new(mvm_vm *vm, mvm_object_kind kind,
    size_t length, size_t payload, mvm_value *out) {
    size_t bytes;
    mvm_object *o;
    if (!vm || !out) return MVM_INVALID;
    *out = mvm_nil();
    /* Keep a valid payload pointer even for empty arrays; account for the byte. */
    if (!payload) payload = 1;
    if (payload > SIZE_MAX - sizeof(*o)) return MVM_LIMIT;
    bytes = sizeof(*o) + payload;
    if (bytes > vm->limits.max_heap_bytes) return MVM_LIMIT;
    if (vm->heap_bytes > vm->limits.max_heap_bytes - bytes) (void)mvm_collect(vm);
    if (vm->heap_bytes > vm->limits.max_heap_bytes - bytes) return MVM_LIMIT;
    o = calloc(1, sizeof(*o));
    if (!o) return MVM_OOM;
    o->data = malloc(payload);
    if (!o->data) { free(o); return MVM_OOM; }
    o->kind = kind; o->length = length; o->bytes = bytes;
    o->next = vm->objects; vm->objects = o;
    vm->heap_bytes += bytes; ++vm->object_count;
    out->kind = MVM_OBJECT; out->as.object = o;
    return MVM_OK;
}
mvm_status mvm_string_new(mvm_vm *vm, const void *data, size_t n, mvm_value *out) {
    mvm_status s;
    if (out) *out = mvm_nil();
    if (!data && n) return MVM_INVALID;
    if (n == SIZE_MAX) return MVM_LIMIT;
    s = object_new(vm, MVM_STRING, n, n + 1, out);
    if (s != MVM_OK) return s;
    if (n) memcpy(out->as.object->data, data, n);
    ((unsigned char *)out->as.object->data)[n] = 0;
    return MVM_OK;
}
mvm_status mvm_array_new(mvm_vm *vm, size_t n, mvm_value *out) {
    mvm_status s;
    size_t i;
    mvm_value *values;
    if (out) *out = mvm_nil();
    if (n > SIZE_MAX / sizeof(mvm_value)) return MVM_LIMIT;
    s = object_new(vm, MVM_ARRAY, n, n * sizeof(mvm_value), out);
    if (s != MVM_OK) return s;
    values = out->as.object->data;
    for (i = 0; i < n; ++i) values[i] = mvm_nil();
    return MVM_OK;
}
mvm_status mvm_object_type(const mvm_vm *vm, mvm_value v, mvm_object_kind *out) {
    mvm_object *o = mvm_owned_object(vm, v);
    if (!out) return MVM_INVALID;
    if (!o) return MVM_TYPE;
    *out = o->kind;
    return MVM_OK;
}
mvm_status mvm_object_length(const mvm_vm *vm, mvm_value v, size_t *out) {
    mvm_object *o = mvm_owned_object(vm, v);
    if (!out) return MVM_INVALID;
    if (!o) return MVM_TYPE;
    *out = o->length;
    return MVM_OK;
}
mvm_status mvm_string_data(const mvm_vm *vm, mvm_value v, const unsigned char **out, size_t *n) {
    mvm_object *o = mvm_owned_object(vm, v);
    if (!out || !n) return MVM_INVALID;
    if (!o || o->kind != MVM_STRING) return MVM_TYPE;
    *out = o->data; *n = o->length;
    return MVM_OK;
}
mvm_status mvm_array_get(const mvm_vm *vm, mvm_value v, size_t i, mvm_value *out) {
    mvm_object *o = mvm_owned_object(vm, v);
    if (!out) return MVM_INVALID;
    if (!o || o->kind != MVM_ARRAY) return MVM_TYPE;
    if (i >= o->length) return MVM_BOUNDS;
    *out = ((mvm_value *)o->data)[i];
    return MVM_OK;
}
mvm_status mvm_array_set(mvm_vm *vm, mvm_value v, size_t i, mvm_value value) {
    mvm_object *o = mvm_owned_object(vm, v);
    if (!o || o->kind != MVM_ARRAY) return MVM_TYPE;
    if (!mvm_value_valid(vm, value)) return MVM_INVALID;
    if (i >= o->length) return MVM_BOUNDS;
    ((mvm_value *)o->data)[i] = value;
    return MVM_OK;
}
const mvm_object_descriptor *mvm_object_describe(mvm_object_kind kind) {
    static const mvm_object_descriptor descriptors[] = {
        {MVM_STRING, "string", 0, 0}, {MVM_ARRAY, "array", 1, 1}
    };
    return (unsigned)kind < 2 ? &descriptors[kind] : NULL;
}
mvm_memory_stats mvm_memory_usage(const mvm_vm *vm) {
    mvm_memory_stats s = {0, 0, 0};
    if (vm) { s.objects = vm->object_count; s.heap_bytes = vm->heap_bytes; s.roots = vm->root_count; }
    return s;
}

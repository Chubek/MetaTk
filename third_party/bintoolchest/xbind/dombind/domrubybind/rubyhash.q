# rubyhash.q — ruby C API primitives.
@module rubyhash
@version 1.0
@param PREFIX rubyhash
@include <ruby.h>
@guard

@fn new {
static inline VALUE ${PREFIX}_new(void) {
    return rb_hash_new();
}
}

@fn get {
static inline VALUE ${PREFIX}_get(VALUE h, VALUE key) {
    return rb_hash_aref(h, key);
}
}

@fn set {
static inline VALUE ${PREFIX}_set(VALUE h, VALUE key, VALUE v) {
    return rb_hash_aset(h, key, v);
}
}

@fn remove {
static inline VALUE ${PREFIX}_remove(VALUE h, VALUE key) {
    return rb_hash_delete(h, key);
}
}


# rubyarray.q — ruby C API primitives.
@module rubyarray
@version 1.0
@param PREFIX rubyarray
@include <ruby.h>
@guard

@fn new {
static inline VALUE ${PREFIX}_new(void) {
    return rb_ary_new();
}
}

@fn size {
static inline long ${PREFIX}_size(VALUE a) {
    Check_Type(a, T_ARRAY); return RARRAY_LEN(a);
}
}

@fn get {
static inline VALUE ${PREFIX}_get(VALUE a, long i) {
    return rb_ary_entry(a, i);
}
}

@fn set {
static inline void ${PREFIX}_set(VALUE a, long i, VALUE v) {
    rb_ary_store(a, i, v);
}
}

@fn push {
static inline VALUE ${PREFIX}_push(VALUE a, VALUE v) {
    return rb_ary_push(a, v);
}
}

@fn pop {
static inline VALUE ${PREFIX}_pop(VALUE a) {
    return rb_ary_pop(a);
}
}


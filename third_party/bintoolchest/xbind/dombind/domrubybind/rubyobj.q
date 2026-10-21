# rubyobj.q — ruby C API primitives.
@module rubyobj
@version 1.0
@param PREFIX rubyobj
@include <ruby.h>
@guard

@fn retain {
static inline void ${PREFIX}_retain(VALUE *slot) {
    rb_gc_register_address(slot);
}
}

@fn release {
static inline void ${PREFIX}_release(VALUE *slot) {
    rb_gc_unregister_address(slot);
}
}

@fn is_nil {
static inline int ${PREFIX}_is_nil(VALUE v) {
    return NIL_P(v);
}
}

@fn truth {
static inline int ${PREFIX}_truth(VALUE v) {
    return RTEST(v);
}
}

@fn inspect {
static inline VALUE ${PREFIX}_inspect(VALUE v) {
    return rb_inspect(v);
}
}

@fn class_of {
static inline VALUE ${PREFIX}_class_of(VALUE v) {
    return rb_obj_class(v);
}
}


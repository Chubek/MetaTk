# rubymodule.q — ruby C API primitives.
@module rubymodule
@version 1.0
@param PREFIX rubymodule
@include <ruby.h>
@guard

@raw {
/* Callback signature: VALUE fn(int argc, VALUE *argv, VALUE self). */
#define ${PREFIX}_function(mod, name, fn) rb_define_module_function((mod), (name), (fn), -1)
}

@fn new {
static inline VALUE ${PREFIX}_new(const char *name) {
    return rb_define_module(name);
}
}

@fn constant {
static inline void ${PREFIX}_constant(VALUE mod, const char *name, VALUE v) {
    rb_define_const(mod, name, v);
}
}


# rubycall.q — ruby C API primitives.
@module rubycall
@version 1.0
@param PREFIX rubycall
@include <ruby.h>
@guard

@fn invoke {
static inline VALUE ${PREFIX}_invoke(VALUE obj, const char *name, int argc, const VALUE *argv) {
    return rb_funcallv(obj, rb_intern(name), argc, argv);
}
}

@fn eval {
static inline VALUE ${PREFIX}_eval(const char *source, int *state) {
    return rb_eval_string_protect(source, state);
}
}

@fn protect {
static inline VALUE ${PREFIX}_protect(VALUE (*fn)(VALUE), VALUE data, int *state) {
    return rb_protect(fn, data, state);
}
}

@fn error {
static inline VALUE ${PREFIX}_error(void) {
    return rb_errinfo();
}
}

@fn clear_error {
static inline void ${PREFIX}_clear_error(void) {
    rb_set_errinfo(Qnil);
}
}


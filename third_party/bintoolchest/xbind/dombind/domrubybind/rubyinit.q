# rubyinit.q — ruby C API primitives.
@module rubyinit
@version 1.0
@param PREFIX rubyinit
@include <ruby.h>
@guard

@fn setup {
static inline int ${PREFIX}_setup(void) {
    return ruby_setup();
}
}

@fn cleanup {
static inline int ${PREFIX}_cleanup(int status) {
    return ruby_cleanup(status);
}
}

@fn loadpath {
static inline void ${PREFIX}_loadpath(void) {
    ruby_init_loadpath();
}
}


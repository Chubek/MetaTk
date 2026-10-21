# ocamlinit.q — ocaml C API primitives.
@module ocamlinit
@version 1.0
@param PREFIX ocamlinit
@include <caml/mlvalues.h>
@include <caml/memory.h>
@include <caml/alloc.h>
@include <caml/callback.h>
@include <caml/fail.h>
@include <caml/threads.h>
@guard

@fn startup {
static inline void ${PREFIX}_startup(char **argv) {
    caml_startup(argv);
}
}

@fn shutdown {
static inline void ${PREFIX}_shutdown(void) {
    caml_shutdown();
}
}

@fn release {
static inline void ${PREFIX}_release(void) {
    caml_release_runtime_system();
}
}

@fn acquire {
static inline void ${PREFIX}_acquire(void) {
    caml_acquire_runtime_system();
}
}


# mulib-mop

An allocation-free meta object protocol for borrowed class descriptors and
inherited named methods. Define `MULIB_IMPL_MOP` in one translation unit.

Classes contain a name, optional parent and method array. An object borrows its
class and caller-owned payload. All descriptors, names, methods and payload
storage must outlive dispatch. Immutable descriptors may be shared; callers
synchronize access to mutable payloads. The protocol never destroys user data.

`mulib_mop_class_valid` checks the complete chain: at most 64 classes, no cycles,
nonempty names, at most 1024 methods per class, non-NULL callbacks, no duplicate
method names within a class. Derived methods override inherited methods.
`mulib_mop_init` validates before modifying output. `mulib_mop_is_a` tests class
identity/ancestry. `mulib_mop_lookup` borrows the nearest method or returns NULL.
`mulib_mop_call` returns 1 when dispatched and optionally copies the callback's
application-defined status; it returns 0 for missing methods or invalid objects.
Callback arguments/output are borrowed and application-typed. A callback's
failure does not automatically roll back changes it made to the payload.

NULL inputs fail safely. No dynamic registration, automatic field reflection,
allocation, serialization or global mutable registry. See
`examples/modules/mop.c` and `tests/test_new_modules.c`.

`mulib_mop_version()` returns API revision 1.

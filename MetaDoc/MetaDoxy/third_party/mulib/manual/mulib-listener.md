# mulib-listener

Define `MULIB_IMPL_LISTENER` and `MULIB_IMPL_EVTSTREAM` in one translation unit.

`mulib_listener_walk(root,listeners,count)` consumes AST events and dispatches
`mulib_listener_t` entries in table order. Each entry contains a production name
(NULL means all productions), optional `enter` and `leave` callbacks, and user
context. Names match productions, not semantic-action aliases.

Callbacks return nonzero to continue or zero to stop. The walk returns 1 on
completion, 0 on an early stop, -1 for invalid arguments. Early stop does not
unwind pending leave callbacks. Trees, tables and contexts remain borrowed;
callbacks must not change the traversed tree.

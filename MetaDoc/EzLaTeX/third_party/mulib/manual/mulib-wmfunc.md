# mulib-wmfunc

Portable window-manager geometry and action helpers. Define `MULIB_IMPL_WMFUNC`,
`MULIB_IMPL_WLROOTS` and `MULIB_IMPL_VEC` in one translation unit. The geometry
model is shared with `mulib-wlroots.h`. No X11/Wayland libraries are required.

`mulib_wmfunc_init` initializes caller-owned window state with nonzero identity
and a nonempty valid half-open box. `mulib_wmfunc_fit` shrinks and clamps a box
inside a work area, using wide intermediates to avoid coordinate overflow.
Inputs may alias output. Invalid or empty inputs leave output unchanged.

`mulib_wmfunc_apply` supports place, maximize, restore, focus and close. Place
takes a box and clears maximized state. Maximize takes a work area and saves
normal geometry exactly once, so repeated maximize preserves restore placement.
Restore while normal succeeds without backend dispatch. Focus/close dispatch
requests and do not destroy the window or change geometry.

A required application callback receives the candidate state. Returning exactly
1 commits it; any other return leaves state unchanged. The callback must not
reenter or modify that window. No callbacks or pointers are retained. The
application supplies actual X11/Wayland/native backend behavior and owns native
resources; this module itself opens no displays and sends no protocol messages.
Calls are synchronous; callers synchronize each window. NULL inputs, invalid
actions, overflowed boxes and failed backend requests return 0.

See `examples/modules/wmfunc.c` and `tests/test_new_modules.c`.

`mulib_wmfunc_version()` returns API revision 1.

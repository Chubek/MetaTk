#define MULIB_IMPL_WMFUNC
#define MULIB_IMPL_WLROOTS
#define MULIB_IMPL_VEC
#include "mulib-wmfunc.h"
#include <assert.h>
static int
backend (void *user, mulib_wmfunc_action_t action,
         const mulib_wmfunc_window_t *candidate)
{
  (void)user;
  (void)action;
  return candidate->id != 0;
}
int
main (void)
{
  mulib_wmfunc_window_t window;
  mulib_wlroots_box_t box = { 10, 10, 100, 100 }, area = { 0, 0, 1920, 1080 };
  assert (mulib_wmfunc_init (&window, 1, &box));
  assert (mulib_wmfunc_apply (&window, MULIB_WMFUNC_MAXIMIZE, &area, backend,
                              NULL));
  assert (window.maximized && window.box.width == 1920);
  assert (
      mulib_wmfunc_apply (&window, MULIB_WMFUNC_RESTORE, NULL, backend, NULL));
  assert (!window.maximized && window.box.width == 100);
  return 0;
}

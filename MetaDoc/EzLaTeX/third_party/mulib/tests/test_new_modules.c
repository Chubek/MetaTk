#define MULIB_IMPL_MOP
#define MULIB_IMPL_MUNE
#define MULIB_IMPL_WMFUNC
#define MULIB_IMPL_WLROOTS
#define MULIB_IMPL_VEC
#define MULIB_IMPL_FRACTION
#include "mulib-fraction.h"
#include "mulib-mop.h"
#include "mulib-mune.h"
#include "mulib-wmfunc.h"
#include <assert.h>
#include <string.h>
static int
method (mulib_mop_object_t *o, const void *arg, void *out)
{
  *(int *)out = *(int *)o->data + *(const int *)arg;
  return 7;
}
static int
backend (void *user, mulib_wmfunc_action_t action,
         const mulib_wmfunc_window_t *w)
{
  (void)action;
  assert (w->id == 1);
  return *(int *)user;
}
int
main (void)
{
  assert (mulib_mop_version () == 1 && mulib_mune_version () == 1
          && mulib_wmfunc_version () == 1);
  mulib_mop_method_t methods[] = { { "add", method } };
  mulib_mop_class_t base = { "base", NULL, methods, 1 },
                    child = { "child", &base, NULL, 0 };
  mulib_mop_object_t object;
  int data = 20, arg = 22, result = 0, status = 0;
  assert (mulib_mop_init (&object, &child, &data));
  assert (mulib_mop_is_a (&object, &base));
  assert (mulib_mop_call (&object, "add", &arg, &result, &status)
          && result == 42 && status == 7);
  assert (!mulib_mop_call (&object, "missing", &arg, &result, &status));
  assert (mulib_mop_lookup (&child, "add") == methods);
  child.parent = &child;
  assert (!mulib_mop_class_valid (&child));
  assert (!mulib_mop_lookup (&child, "add"));
  child.parent = &base;
  {
    mulib_mop_method_t duplicate[] = { { "add", method }, { "add", method } };
    mulib_mop_class_t invalid = { "invalid", NULL, duplicate, 2 };
    assert (!mulib_mop_class_valid (&invalid));
  }
  {
    mulib_mune_t *m = mulib_mune_create ();
    mulib_mune_error_t e;
    double value = 123;
    const char *script = "x = 6; y = 7; return x * y -- answer\n";
    assert (m
            && mulib_mune_eval (m, script, strlen (script), &value, &e)
                   == MULIB_MUNE_OK
            && value == 42);
    script = "x=9; return 1/0";
    value = 123;
    assert (mulib_mune_eval (m, script, strlen (script), &value, &e)
                == MULIB_MUNE_ARITHMETIC
            && value == 123);
    assert (mulib_mune_get (m, "x", &value) && value == 6);
    script = "return -9 % 4 + 2 * (3 + 1)";
    assert (mulib_mune_eval (m, script, strlen (script), &value, NULL)
                == MULIB_MUNE_OK
            && value == 11);
    script = "return missing";
    assert (mulib_mune_eval (m, script, strlen (script), &value, &e)
            == MULIB_MUNE_NAME);
    script = "return 1e+";
    assert (mulib_mune_eval (m, script, strlen (script), &value, &e)
            == MULIB_MUNE_SYNTAX);
    script = "return 1return 2";
    assert (mulib_mune_eval (m, script, strlen (script), &value, &e)
            == MULIB_MUNE_SYNTAX);
    script = "local x=1";
    assert (mulib_mune_eval (m, script, strlen (script), &value, &e)
            == MULIB_MUNE_SYNTAX);
    {
      const char nul[] = { 'x', '=', '1', 0, ';' };
      assert (mulib_mune_eval (m, nul, sizeof (nul), &value, &e)
              == MULIB_MUNE_SYNTAX);
    }
    {
      char deep[200];
      memset (deep, '(', 99);
      deep[99] = '1';
      memset (deep + 100, ')', 99);
      deep[199] = 0;
      char source[220];
      strcpy (source, "return ");
      strcat (source, deep);
      assert (mulib_mune_eval (m, source, strlen (source), &value, &e)
              == MULIB_MUNE_LIMIT);
    }
    assert (mulib_mune_eval (m, NULL, 0, &value, &e) == MULIB_MUNE_OK
            && value == 0);
    mulib_mune_destroy (m);
    mulib_mune_destroy (NULL);
  }
  {
    mulib_wlroots_box_t box = { 10, 20, 100, 200 },
                        area = { -100, -100, 400, 400 }, fit;
    mulib_wmfunc_window_t w;
    int accepted = 0;
    assert (mulib_wmfunc_init (&w, 1, &box));
    assert (!mulib_wmfunc_apply (&w, MULIB_WMFUNC_MAXIMIZE, &area, backend,
                                 &accepted));
    assert (!w.maximized && w.box.x == 10);
    accepted = 1;
    assert (mulib_wmfunc_apply (&w, MULIB_WMFUNC_MAXIMIZE, &area, backend,
                                &accepted));
    assert (w.maximized && w.box.x == -100);
    assert (mulib_wmfunc_apply (&w, MULIB_WMFUNC_MAXIMIZE, &area, backend,
                                &accepted));
    assert (mulib_wmfunc_apply (&w, MULIB_WMFUNC_RESTORE, NULL, backend,
                                &accepted));
    assert (!w.maximized && w.box.x == 10 && w.box.height == 200);
    box.x = 1000;
    box.width = 500;
    assert (mulib_wmfunc_fit (&box, &area, &fit) && fit.x == -100
            && fit.width == 400);
    area.width = -1;
    assert (!mulib_wmfunc_fit (&box, &area, &fit));
    assert (!mulib_wmfunc_init (&w, 0, &box));
  }
  {
    mulib_fraction_t f, a = { 1, INT64_MAX }, b = { 1, INT64_MAX };
    assert (mulib_fraction_make (INT64_MIN, INT64_MIN, &f) && f.num == 1
            && f.den == 1);
    assert (mulib_fraction_make (0, INT64_MIN, &f) && f.num == 0
            && f.den == 1);
    assert (mulib_fraction_make (2, INT64_MIN, &f) && f.num == -1
            && f.den == INT64_C (4611686018427387904));
    assert (!mulib_fraction_make (1, INT64_MIN, &f));
    assert (mulib_fraction_add (a, b, &f) && f.num == 2 && f.den == INT64_MAX);
    a.num = INT64_MIN;
    a.den = 1;
    b.num = 0;
    b.den = 1;
    assert (mulib_fraction_add (a, b, &f) && f.num == INT64_MIN && f.den == 1);
    b.num = -1;
    assert (!mulib_fraction_add (a, b, &f));
  }
  return 0;
}

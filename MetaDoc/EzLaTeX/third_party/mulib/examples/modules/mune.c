#define MULIB_IMPL_MUNE
#include "mulib-mune.h"
#include <assert.h>
#include <string.h>
int
main (void)
{
  const char *source = "x = 6; return x * 7";
  mulib_mune_t *m = mulib_mune_create ();
  double result;
  assert (m);
  assert (mulib_mune_eval (m, source, strlen (source), &result, NULL)
          == MULIB_MUNE_OK);
  assert (result == 42);
  mulib_mune_destroy (m);
  return 0;
}

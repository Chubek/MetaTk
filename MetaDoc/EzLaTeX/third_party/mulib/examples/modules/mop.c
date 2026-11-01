#define MULIB_IMPL_MOP
#include "mulib-mop.h"
#include <assert.h>
static int
value (mulib_mop_object_t *self, const void *arg, void *out)
{
  (void)arg;
  *(int *)out = *(int *)self->data;
  return 0;
}
int
main (void)
{
  const mulib_mop_method_t methods[] = { { "value", value } };
  const mulib_mop_class_t type = { "Number", NULL, methods, 1 };
  mulib_mop_object_t object;
  int data = 42, result = 0;
  assert (mulib_mop_init (&object, &type, &data));
  assert (mulib_mop_call (&object, "value", NULL, &result, NULL)
          && result == 42);
  return 0;
}

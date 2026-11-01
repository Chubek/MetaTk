#define MULIB_IMPL_WASK
#define MULIB_IMPL_DSYMSPEC
#define MULIB_IMPL_JSON
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_AST
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#include "mulib-wask.h"
#include <assert.h>
int
main (void)
{
  const unsigned char bytes[]
      = { 0,    97, 115,  109, 1, 0, 0, 0,    1, 7,    1, 0x60, 2,   0x7f,
          0x7f, 1,  0x7f, 3,   2, 1, 0, 7,    7, 1,    3, 'a',  'd', 'd',
          0,    0,  10,   9,   1, 7, 0, 0x20, 0, 0x20, 1, 0x6a, 0x0b };
  uint32_t args[] = { 20, 22 }, result;
  mulib_wask_error_t error;
  mulib_wask_t *module = mulib_wask_load (bytes, sizeof (bytes), &error);
  assert (module);
  assert (mulib_wask_call (module, "add", args, 2, 100, &result, &error)
          == MULIB_WASK_OK);
  assert (result == 42);
  mulib_wask_destroy (module);
  return 0;
}

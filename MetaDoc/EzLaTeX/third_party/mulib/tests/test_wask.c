#include <stdlib.h>
static int allocation_budget_ = -1;
static int
allocation_allowed_ (void)
{
  if (!allocation_budget_)
    return 0;
  if (allocation_budget_ > 0)
    --allocation_budget_;
  return 1;
}
static void *
checked_malloc_ (size_t n)
{
  return allocation_allowed_ () ? malloc (n) : NULL;
}
static void *
checked_calloc_ (size_t n, size_t size)
{
  return allocation_allowed_ () ? calloc (n, size) : NULL;
}
#define malloc checked_malloc_
#define calloc checked_calloc_
#define MULIB_IMPL_WASK
#define MULIB_IMPL_DSYMSPEC
#define MULIB_IMPL_JSON
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_AST
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#include "mulib-wask.h"
#undef malloc
#undef calloc
#include <assert.h>
#include <string.h>

static const unsigned char add[]
    = { 0,    97, 115,  109, 1, 0, 0, 0,    1, 7,    1, 0x60, 2,   0x7f,
        0x7f, 1,  0x7f, 3,   2, 1, 0, 7,    7, 1,    3, 'a',  'd', 'd',
        0,    0,  10,   9,   1, 7, 0, 0x20, 0, 0x20, 1, 0x6a, 0x0b };
static void
rejected (const unsigned char *p, size_t n, mulib_wask_status_t status)
{
  mulib_wask_error_t e;
  mulib_wask_t *m = mulib_wask_load (p, n, &e);
  if (m || e.code != status)
    fprintf (stderr, "expected %d got %d: %s at %zu\n", status, e.code,
             e.message, e.offset);
  assert (!m && e.code == status && e.message && e.offset <= n);
}
static size_t
leb (unsigned char *out, size_t value)
{
  size_t n = 0;
  do
    {
      unsigned byte = (unsigned)(value & 127);
      value >>= 7;
      out[n++] = (unsigned char)(byte | (value ? 128 : 0));
    }
  while (value);
  return n;
}
static mulib_wask_t *
body (const unsigned char *code, size_t length, mulib_wask_error_t *error)
{
  unsigned char module[2048], size[10];
  size_t n = 30, width = leb (size, length);
  assert (length + 64 < sizeof (module));
  memcpy (module, add, 30);
  module[n++] = 10;
  n += leb (module + n, 1 + width + length);
  module[n++] = 1;
  memcpy (module + n, size, width);
  n += width;
  memcpy (module + n, code, length);
  n += length;
  return mulib_wask_load (module, n, error);
}
static void
operations (void)
{
  const unsigned char binary[] = { 0x6a, 0x6b, 0x6c, 0x6d, 0x71, 0x72, 0x73 };
  const uint32_t expected[] = { 288, 192, 11520, 5, 48, 240, 192 };
  uint32_t args[] = { 240, 48 }, result;
  mulib_wask_error_t error;
  mulib_wask_t *m;
  assert (mulib_wask_version () == 1 && mulib_wask_supports ("i32.add"));
  assert (!mulib_wask_supports ("unreachable")
          && !mulib_wask_supports ("memory.grow"));
  assert (!mulib_wask_supports (NULL));
  for (size_t i = 0; i < sizeof (binary); ++i)
    {
      unsigned char code[] = { 0, 0x20, 0, 0x20, 1, 0, 0x0b };
      code[5] = binary[i];
      m = body (code, sizeof (code), &error);
      assert (m);
      assert (mulib_wask_call (m, "add", args, 2, 10, &result, NULL)
                  == MULIB_WASK_OK
              && result == expected[i]);
      mulib_wask_destroy (m);
    }
  {
    const unsigned char code[] = { 1, 1,    0x7f, 0x20, 0, 0x21, 2,   0x20,
                                   1, 0x22, 2,    0x20, 2, 0x6a, 0x0b };
    m = body (code, sizeof (code), &error);
    assert (m);
    assert (mulib_wask_call (m, "add", args, 2, 20, &result, NULL)
                == MULIB_WASK_OK
            && result == 96);
    mulib_wask_destroy (m);
  }
  {
    const unsigned char code[] = { 1, 1, 0x7f, 0x01, 0x20, 2, 0x45, 0x0b };
    m = body (code, sizeof (code), &error);
    assert (m);
    assert (mulib_wask_call (m, "add", args, 2, 20, &result, NULL)
                == MULIB_WASK_OK
            && result == 1);
    mulib_wask_destroy (m);
  }
  {
    unsigned char code[]
        = { 0, 0x41, 1, 0x1a, 0x20, 0, 0x20, 1, 0x41, 0, 0x1b, 0x0b };
    for (unsigned condition = 0; condition < 2; ++condition)
      {
        code[9] = (unsigned char)condition;
        m = body (code, sizeof (code), &error);
        assert (m);
        assert (mulib_wask_call (m, "add", args, 2, 20, &result, NULL)
                    == MULIB_WASK_OK
                && result == args[condition ? 0 : 1]);
        mulib_wask_destroy (m);
      }
  }
  {
    const unsigned char underflow[] = { 0, 0x6a, 0x0b };
    assert (!body (underflow, sizeof (underflow), &error)
            && error.code == MULIB_WASK_INVALID);
    const unsigned char trailing[] = { 0, 0x20, 0, 0x0b, 1 };
    assert (!body (trailing, sizeof (trailing), &error)
            && error.code == MULIB_WASK_INVALID);
    const unsigned char locals[] = { 1, 0xff, 1, 0x7f, 0x20, 0, 0x0b };
    assert (!body (locals, sizeof (locals), &error)
            && error.code == MULIB_WASK_LIMIT);
    unsigned char stack[520];
    size_t n = 0;
    stack[n++] = 0;
    for (unsigned i = 0; i < 257; ++i)
      {
        stack[n++] = 0x41;
        stack[n++] = 0;
      }
    stack[n++] = 0x0b;
    assert (!body (stack, n, &error) && error.code == MULIB_WASK_LIMIT);
  }
}
int
main (void)
{
  operations ();
  for (int failure = 0; failure < 12; ++failure)
    {
      mulib_wask_error_t error;
      allocation_budget_ = failure;
      mulib_wask_t *module = mulib_wask_load (add, sizeof (add), &error);
      allocation_budget_ = -1;
      assert (!module && error.code == MULIB_WASK_MEMORY);
    }
  mulib_wask_error_t e;
  uint32_t args[] = { 20, 22 }, out = 777;
  unsigned char bytes[sizeof (add) + 16];
  memcpy (bytes, add, sizeof (add));
  mulib_wask_t *m = mulib_wask_load (bytes, sizeof (add), &e);
  assert (m && e.code == MULIB_WASK_OK);
  memset (bytes, 0, sizeof (bytes)); /* module owns a copy */
  assert (mulib_wask_call (m, "add", args, 2, 4, &out, &e) == MULIB_WASK_OK
          && out == 42);
  args[0] = UINT32_MAX;
  args[1] = 2;
  assert (mulib_wask_call (m, "add", args, 2, 4, &out, NULL) == MULIB_WASK_OK
          && out == 1);
  out = 777;
  assert (mulib_wask_call (m, "add", args, 2, 3, &out, &e) == MULIB_WASK_LIMIT
          && out == 777);
  assert (mulib_wask_call (m, "add", args, 2, 0, &out, NULL)
          == MULIB_WASK_LIMIT);
  assert (mulib_wask_call (m, "absent", args, 2, 100, &out, &e)
          == MULIB_WASK_NOT_FOUND);
  assert (mulib_wask_call (m, "add", args, 1, 100, &out, &e)
          == MULIB_WASK_INVALID);
  assert (mulib_wask_call (m, "add", NULL, 2, 100, &out, &e)
          == MULIB_WASK_INVALID);
  mulib_wask_destroy (m);
  mulib_wask_destroy (NULL);
  rejected (NULL, 0, MULIB_WASK_INVALID);
  memcpy (bytes, add, sizeof (add));
  bytes[4] = 2;
  rejected (bytes, sizeof (add), MULIB_WASK_INVALID);
  memcpy (bytes, add, sizeof (add));
  bytes[35] = 0x44;
  rejected (bytes, sizeof (add), MULIB_WASK_UNSUPPORTED);
  memcpy (bytes, add, sizeof (add));
  bytes[36] = 2;
  rejected (bytes, sizeof (add), MULIB_WASK_INVALID);
  memcpy (bytes, add, sizeof (add));
  bytes[33] = 100;
  rejected (bytes, sizeof (add), MULIB_WASK_INVALID);
  memcpy (bytes, add, sizeof (add));
  bytes[31] = 10;
  rejected (bytes, sizeof (add), MULIB_WASK_INVALID);
  memcpy (bytes, add, sizeof (add));
  bytes[12] = 0x80;
  rejected (bytes, sizeof (add), MULIB_WASK_LIMIT);
  memcpy (bytes, add, sizeof (add));
  bytes[39] = 0x6d;
  m = mulib_wask_load (bytes, sizeof (add), &e);
  assert (m);
  args[0] = 10;
  args[1] = 0;
  out = 777;
  assert (mulib_wask_call (m, "add", args, 2, 100, &out, &e) == MULIB_WASK_TRAP
          && out == 777);
  args[0] = UINT32_C (0x80000000);
  args[1] = UINT32_MAX;
  assert (mulib_wask_call (m, "add", args, 2, 100, &out, &e)
          == MULIB_WASK_TRAP);
  args[0] = (uint32_t)-9;
  args[1] = 2;
  assert (mulib_wask_call (m, "add", args, 2, 100, &out, &e) == MULIB_WASK_OK
          && out == (uint32_t)-4);
  mulib_wask_destroy (m);
  /* One constant-return module, including nonminimal and invalid signed LEBs.
   */
  {
    unsigned char constant[]
        = { 0,  97,   115, 109, 1, 0,    0,    0,    1,    5,    1,    0x60, 0,
            1,  0x7f, 3,   2,   1, 0,    7,    5,    1,    1,    'f',  0,    0,
            10, 10,   1,   8,   0, 0x41, 0xff, 0xff, 0xff, 0xff, 0x7f, 0x0b };
    m = mulib_wask_load (constant, sizeof (constant), &e);
    assert (m);
    assert (mulib_wask_call (m, "f", NULL, 0, 2, &out, NULL) == MULIB_WASK_OK
            && out == UINT32_MAX);
    mulib_wask_destroy (m);
    constant[36] = 0x08; /* invalid s32 terminal sign extension */
    rejected (constant, sizeof (constant), MULIB_WASK_INVALID);
    constant[36] = 0x80;
    rejected (constant, sizeof (constant), MULIB_WASK_INVALID);
  }
  /* Every truncation must fail safely or remain a valid shorter module. */
  for (size_t n = 0; n < sizeof (add); ++n)
    {
      m = mulib_wask_load (add, n, &e);
      if (m)
        {
          assert (mulib_wask_call (m, "add", args, 2, 100, &out, &e)
                  == MULIB_WASK_NOT_FOUND);
          mulib_wask_destroy (m);
        }
      else
        assert (e.code != MULIB_WASK_OK && e.offset <= n);
    }
  /* Bounded mutations cover section framing, types, indices and bytecode. */
  for (size_t i = 0; i < sizeof (add); ++i)
    {
      memcpy (bytes, add, sizeof (add));
      bytes[i] ^= 0x80;
      m = mulib_wask_load (bytes, sizeof (add), &e);
      if (m)
        {
          (void)mulib_wask_call (m, "add", args, 2, 100, &out, &e);
          mulib_wask_destroy (m);
        }
      else
        assert (e.code != MULIB_WASK_OK && e.offset <= sizeof (add));
    }
  return 0;
}

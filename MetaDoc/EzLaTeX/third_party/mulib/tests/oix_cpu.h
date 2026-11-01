/* Test-only 16-bit x86 interpreter. It executes the emitted native kernel,
 * independently of the OIX task dispatcher. No production execution fallback.
 */
#ifndef TEST_OIX_CPU_H
#define TEST_OIX_CPU_H
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef struct oix_cpu
{
  unsigned char mem[65536], output[16384], input[4096];
  uint16_t r[8], ip;
  unsigned zf, cf, halted, exit_code;
  size_t output_size, input_size, input_at, ticks;
} oix_cpu;
static uint16_t
cpu_word (const unsigned char *p)
{
  return (uint16_t)(p[0] | (unsigned)p[1] << 8);
}
static void
cpu_store (unsigned char *p, uint16_t v)
{
  p[0] = (unsigned char)v;
  p[1] = (unsigned char)(v >> 8);
}
static unsigned
cpu_fetch (oix_cpu *c)
{
  return c->mem[c->ip++];
}
static uint16_t
cpu_fetch_word (oix_cpu *c)
{
  unsigned lo = cpu_fetch (c), hi = cpu_fetch (c);
  return (uint16_t)(lo | hi << 8);
}
typedef struct cpu_operand
{
  unsigned reg, index;
  uint16_t address;
} cpu_operand;
static cpu_operand
cpu_modrm (oix_cpu *c, unsigned m)
{
  unsigned mode = m >> 6, rm = m & 7;
  cpu_operand o = { mode == 3, rm, 0 };
  if (mode == 3)
    return o;
  switch (rm)
    {
    case 0:
      o.address = (uint16_t)(c->r[3] + c->r[6]);
      break;
    case 1:
      o.address = (uint16_t)(c->r[3] + c->r[7]);
      break;
    case 2:
      o.address = (uint16_t)(c->r[5] + c->r[6]);
      break;
    case 3:
      o.address = (uint16_t)(c->r[5] + c->r[7]);
      break;
    case 4:
      o.address = c->r[6];
      break;
    case 5:
      o.address = c->r[7];
      break;
    case 6:
      o.address = mode ? c->r[5] : cpu_fetch_word (c);
      break;
    case 7:
      o.address = c->r[3];
      break;
    }
  if (mode == 1)
    o.address = (uint16_t)(o.address + (int8_t)cpu_fetch (c));
  if (mode == 2)
    o.address = (uint16_t)(o.address + cpu_fetch_word (c));
  return o;
}
static uint16_t
cpu_get (oix_cpu *c, cpu_operand o, unsigned width)
{
  if (o.reg)
    {
      if (width == 2)
        return c->r[o.index];
      return (uint16_t)((c->r[o.index & 3] >> (o.index >= 4 ? 8 : 0)) & 255);
    }
  assert (width == 1 || o.address < 65535);
  return width == 1 ? c->mem[o.address] : cpu_word (c->mem + o.address);
}
static void
cpu_put (oix_cpu *c, cpu_operand o, unsigned width, uint16_t v)
{
  if (o.reg)
    {
      if (width == 2)
        c->r[o.index] = v;
      else
        {
          unsigned shift = o.index >= 4 ? 8 : 0, i = o.index & 3;
          c->r[i] = (uint16_t)((c->r[i] & ~(255U << shift))
                               | ((v & 255U) << shift));
        }
    }
  else
    {
      assert (width == 1 || o.address < 65535);
      if (width == 1)
        c->mem[o.address] = (unsigned char)v;
      else
        cpu_store (c->mem + o.address, v);
    }
}
static void
cpu_push (oix_cpu *c, uint16_t v)
{
  c->r[4] -= 2;
  assert (c->r[4] >= 0xe000 && c->r[4] < 65535);
  cpu_store (c->mem + c->r[4], v);
}
static uint16_t
cpu_pop (oix_cpu *c)
{
  uint16_t v;
  assert (c->r[4] >= 0xe000 && c->r[4] < 65535);
  v = cpu_word (c->mem + c->r[4]);
  c->r[4] += 2;
  return v;
}
static uint16_t
cpu_arith (oix_cpu *c, uint16_t a, uint16_t b, int sub, unsigned width)
{
  uint32_t mask = width == 1 ? 255 : 65535,
           value = sub ? (uint32_t)a - b : (uint32_t)a + b;
  c->cf = sub ? a < b : value > mask;
  c->zf = (value & mask) == 0;
  return (uint16_t)(value & mask);
}
static void
cpu_init (oix_cpu *c, const mulib_oix_image_t *image)
{
  const unsigned char *h = image->data;
  unsigned char *g;
  assert (!mulib_oix_validate (h, image->length, NULL, NULL));
  memset (c, 0, sizeof (*c));
  memcpy (c->mem + 0x1000, h + 64, image->length - 64);
  c->ip = 0x1000;
  c->r[4] = 0xfff0;
  g = c->mem + 0x500;
  cpu_store (g, cpu_word (h + 46));
  cpu_store (g + 2, cpu_word (h + 44));
  cpu_store (g + 4, cpu_word (h + 42));
  cpu_store (g + 8, cpu_word (h + 42));
  cpu_store (g + 12, cpu_word (h + 50));
  cpu_store (g + 14, cpu_word (h + 52));
  cpu_store (g + 16, cpu_word (h + 48));
  cpu_store (g + 18, (uint16_t)strlen ((char *)c->mem + cpu_word (h + 48)));
  cpu_store (g + 20, cpu_word (h + 40));
}
static void
cpu_run (oix_cpu *c, size_t budget)
{
  while (!c->halted && budget--)
    {
      uint16_t at = c->ip, a, b, v, relative;
      unsigned op = cpu_fetch (c), m, reg, width, group, cf;
      cpu_operand o, rr = { 1, 0, 0 };
      if (op >= 0xb8 && op <= 0xbf)
        {
          c->r[op - 0xb8] = cpu_fetch_word (c);
          continue;
        }
      if (op >= 0xb0 && op <= 0xb7)
        {
          rr.index = op - 0xb0;
          v = (uint16_t)cpu_fetch (c);
          cpu_put (c, rr, 1, v);
          continue;
        }
      if (op >= 0x50 && op <= 0x57)
        {
          cpu_push (c, c->r[op - 0x50]);
          continue;
        }
      if (op >= 0x58 && op <= 0x5f)
        {
          c->r[op - 0x58] = cpu_pop (c);
          continue;
        }
      if (op >= 0x40 && op <= 0x4f)
        {
          reg = op & 7;
          cf = c->cf;
          c->r[reg] = cpu_arith (c, c->r[reg], 1, op >= 0x48, 2);
          c->cf = cf;
          continue;
        }
      switch (op)
        {
        case 0xfa:
        case 0xfc:
          break;
        case 0x30:
        case 0x31:
        case 0x8e:
        case 0x8b:
        case 0x89:
        case 0x8a:
        case 0x8d:
        case 0x01:
        case 0x03:
        case 0x3b:
        case 0x85:
        case 0x84:
          m = cpu_fetch (c);
          reg = (m >> 3) & 7;
          o = cpu_modrm (c, m);
          rr.index = reg;
          width = (op == 0x30 || op == 0x8a || op == 0x84) ? 1 : 2;
          a = cpu_get (c, o, width);
          b = cpu_get (c, rr, width);
          if (op == 0x30 || op == 0x31)
            {
              v = a ^ b;
              c->zf = v == 0;
              c->cf = 0;
              cpu_put (c, o, width, v);
            }
          else if (op == 0x8e)
            assert (a == 0);
          else if (op == 0x8b || op == 0x8a)
            cpu_put (c, rr, width, a);
          else if (op == 0x89)
            cpu_put (c, o, width, b);
          else if (op == 0x8d)
            {
              assert (!o.reg);
              c->r[reg] = o.address;
            }
          else if (op == 0x01)
            cpu_put (c, o, 2, cpu_arith (c, a, b, 0, 2));
          else if (op == 0x03)
            c->r[reg] = cpu_arith (c, b, a, 0, 2);
          else if (op == 0x3b)
            (void)cpu_arith (c, b, a, 1, 2);
          else
            {
              c->zf = (a & b) == 0;
              c->cf = 0;
            }
          break;
        case 0xa1:
          a = cpu_fetch_word (c);
          assert (a < 65535);
          c->r[0] = cpu_word (c->mem + a);
          break;
        case 0xa3:
          a = cpu_fetch_word (c);
          assert (a < 65535);
          cpu_store (c->mem + a, c->r[0]);
          break;
        case 0xac:
          rr.index = 0;
          cpu_put (c, rr, 1, c->mem[c->r[6]++]);
          break;
        case 0xad:
          assert (c->r[6] < 65535);
          c->r[0] = cpu_word (c->mem + c->r[6]);
          c->r[6] += 2;
          break;
        case 0x04:
          a = (uint16_t)cpu_fetch (c);
          rr.index = 0;
          cpu_put (c, rr, 1, cpu_arith (c, cpu_get (c, rr, 1), a, 0, 1));
          break;
        case 0x3d:
          a = cpu_fetch_word (c);
          (void)cpu_arith (c, c->r[0], a, 1, 2);
          break;
        case 0xff:
        case 0x83:
        case 0xc7:
        case 0xc1:
        case 0xf7:
          m = cpu_fetch (c);
          group = (m >> 3) & 7;
          o = cpu_modrm (c, m);
          a = cpu_get (c, o, 2);
          if (op == 0xff)
            {
              assert (group <= 1);
              cf = c->cf;
              cpu_put (c, o, 2, cpu_arith (c, a, 1, group == 1, 2));
              c->cf = cf;
            }
          else if (op == 0x83)
            {
              b = (uint16_t)(int16_t)(int8_t)cpu_fetch (c);
              assert (group == 0 || group == 5 || group == 7);
              v = cpu_arith (c, a, b, group != 0, 2);
              if (group != 7)
                cpu_put (c, o, 2, v);
            }
          else if (op == 0xc7)
            {
              assert (group == 0);
              v = cpu_fetch_word (c);
              cpu_put (c, o, 2, v);
            }
          else if (op == 0xc1)
            {
              unsigned n = cpu_fetch (c);
              assert (group == 4 && n > 0 && n < 16);
              c->cf = (a >> (16 - n)) & 1;
              v = (uint16_t)(a << n);
              c->zf = v == 0;
              cpu_put (c, o, 2, v);
            }
          else
            {
              uint32_t numerator = ((uint32_t)c->r[2] << 16) | c->r[0];
              assert (group == 6 && a && numerator / a <= 65535);
              c->r[0] = (uint16_t)(numerator / a);
              c->r[2] = (uint16_t)(numerator % a);
            }
          break;
        case 0xe8:
          relative = cpu_fetch_word (c);
          cpu_push (c, c->ip);
          c->ip = (uint16_t)(c->ip + relative);
          break;
        case 0xe9:
          relative = cpu_fetch_word (c);
          c->ip = (uint16_t)(c->ip + relative);
          break;
        case 0xc3:
          c->ip = cpu_pop (c);
          break;
        case 0x0f:
          m = cpu_fetch (c);
          relative = cpu_fetch_word (c);
          assert (m == 0x84 || m == 0x85 || m == 0x82 || m == 0x83);
          if ((m == 0x84 && c->zf) || (m == 0x85 && !c->zf)
              || (m == 0x82 && c->cf) || (m == 0x83 && !c->cf))
            c->ip = (uint16_t)(c->ip + relative);
          break;
        case 0xec:
          assert (c->r[2] == 0xea || c->r[2] == 0xeb);
          rr.index = 0;
          a = c->r[2] == 0xea
                  ? (uint16_t)(c->input_at < c->input_size)
                  : (c->input_at < c->input_size ? c->input[c->input_at++]
                                                 : 0);
          cpu_put (c, rr, 1, a);
          break;
        case 0xee:
          if (c->r[2] == 0xe9)
            {
              assert (c->output_size < sizeof (c->output));
              c->output[c->output_size++] = (unsigned char)c->r[0];
            }
          else
            {
              assert (c->r[2] == 0xec);
              ++c->ticks;
            }
          break;
        case 0xef:
          assert (c->r[2] == 0xf4 && c->r[0] <= 255);
          c->halted = 1;
          c->exit_code = c->r[0];
          break;
        default:
          fprintf (stderr, "unknown x86 opcode %02x at %04x\n", op, at);
          assert (0);
        }
    }
}
#endif

/**
 * @file mulib-elf.h
 * @brief Bounds-checked, endian-aware ELF32/ELF64 memory views, not a loader.
 * Define MULIB_IMPL_ELF and MULIB_IMPL_ENDIAN once. Input is never
 * system-cast.
 */
#ifndef MULIB_ELF_H
#define MULIB_ELF_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Structural access results; no errno or hidden global error state.
   */
  enum mulib_elf_result
  {
    MULIB_ELF_OK = 0,
    MULIB_ELF_EINVAL = -1,
    MULIB_ELF_EFORMAT = -2,
    MULIB_ELF_EBOUNDS = -3,
    MULIB_ELF_ETYPE = -4,
    MULIB_ELF_ENOTFOUND = -5
  };
  /** @brief Borrowed, validated ELF header and table locations. Do not modify
   * fields. */
  typedef struct mulib_elf
  {
    const unsigned char *data;
    size_t size;
    unsigned bits;
    int little_endian;
    uint16_t type, machine;
    uint32_t flags;
    uint64_t entry, phoff, shoff;
    uint16_t phentsize, shentsize;
    size_t phnum, shnum, shstrndx;
  } mulib_elf_t;
  /** @brief Decoded section header; size is memory size for NOBITS (type 8).
   */
  typedef struct mulib_elf_section
  {
    uint32_t name, type;
    uint64_t flags, address, offset, size;
    uint32_t link, info;
    uint64_t alignment, entsize;
  } mulib_elf_section_t;
  /** @brief Decoded program header. File range is checked, including PT_LOAD
   * size rules. */
  typedef struct mulib_elf_program
  {
    uint32_t type, flags;
    uint64_t offset, vaddr, paddr, filesz, memsz, alignment;
  } mulib_elf_program_t;
  /** @brief Decoded symbol; section_index resolves SHN_XINDEX when present. */
  typedef struct mulib_elf_symbol
  {
    uint32_t name, section_index;
    uint64_t value, size;
    unsigned char info, other;
  } mulib_elf_symbol_t;
  /** @brief Open borrowed ELF bytes. Validate all header/table/payload bounds.
   * Supports extended table counts and section-name indexes. Output unchanged
   * on error. String termination, symbol names, and extended symbol indexes
   * are checked on access.
   */
  int mulib_elf_open (mulib_elf_t *view, const void *data, size_t size);
  /** @brief Decode and validate an indexed section header. */
  int mulib_elf_section (const mulib_elf_t *view, size_t index,
                         mulib_elf_section_t *section);
  /** @brief Decode and validate an indexed program header. */
  int mulib_elf_program (const mulib_elf_t *view, size_t index,
                         mulib_elf_program_t *program);
  /** @brief Borrow a checked file range. One-past-end is allowed for a zero
   * length. */
  int mulib_elf_bytes (const mulib_elf_t *view, uint64_t offset, uint64_t size,
                       const unsigned char **bytes);
  /** @brief Borrow a NUL-terminated string in a STRTAB section, excluding NUL
   * in length. */
  int mulib_elf_string (const mulib_elf_t *view, size_t string_section,
                        uint64_t offset, const char **text, size_t *length);
  /** @brief Borrow a section name; ENOTFOUND if no section-name table exists.
   */
  int mulib_elf_section_name (const mulib_elf_t *view, size_t index,
                              const char **text, size_t *length);
  /** @brief Count symbols in SYMTAB (2) or DYNSYM (11), validating its linked
   * STRTAB. */
  int mulib_elf_symbol_count (const mulib_elf_t *view, size_t symbol_section,
                              size_t *count);
  /** @brief Decode an indexed symbol (including an optional SYMTAB_SHNDX
   * section). */
  int mulib_elf_symbol (const mulib_elf_t *view, size_t symbol_section,
                        size_t index, mulib_elf_symbol_t *symbol);
  /** @brief Decode a symbol and borrow its name from the associated string
   * table. */
  int mulib_elf_symbol_name (const mulib_elf_t *view, size_t symbol_section,
                             size_t index, const char **text, size_t *length);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_ELF_H */

#if defined(MULIB_IMPL_ELF) && !defined(MULIB_ELF_IMPLEMENTED)
#define MULIB_ELF_IMPLEMENTED
#include "mulib-endian.h"
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif
  static uint16_t
  mulib_elf_u16_ (const mulib_elf_t *v, const unsigned char *p)
  {
    return v->little_endian ? (uint16_t)((uint16_t)p[0] | (uint16_t)p[1] << 8)
                            : (uint16_t)((uint16_t)p[1] | (uint16_t)p[0] << 8);
  }
  static uint32_t
  mulib_elf_u32_ (const mulib_elf_t *v, const unsigned char *p)
  {
    return v->little_endian ? mulib_endian_load_le32 (p)
                            : mulib_endian_load_be32 (p);
  }
  static uint64_t
  mulib_elf_u64_ (const mulib_elf_t *v, const unsigned char *p)
  {
    uint64_t a = mulib_elf_u32_ (v, p), b = mulib_elf_u32_ (v, p + 4);
    return v->little_endian ? a | b << 32 : a << 32 | b;
  }
  static int
  mulib_elf_range_ (const mulib_elf_t *v, uint64_t offset, uint64_t size)
  {
    return offset <= v->size && size <= v->size - (size_t)offset;
  }
  static int
  mulib_elf_alignment_ (uint64_t n)
  {
    return !n || !(n & (n - 1));
  }
  static int
  mulib_elf_table_ (const mulib_elf_t *v, uint64_t off, size_t count,
                    uint16_t stride)
  {
    return off <= v->size
           && (!count
               || (stride && count <= (v->size - (size_t)off) / stride));
  }
  static void
  mulib_elf_section_decode_ (const mulib_elf_t *v, const unsigned char *p,
                             mulib_elf_section_t *s)
  {
    s->name = mulib_elf_u32_ (v, p);
    s->type = mulib_elf_u32_ (v, p + 4);
    if (v->bits == 32)
      {
        s->flags = mulib_elf_u32_ (v, p + 8);
        s->address = mulib_elf_u32_ (v, p + 12);
        s->offset = mulib_elf_u32_ (v, p + 16);
        s->size = mulib_elf_u32_ (v, p + 20);
        s->link = mulib_elf_u32_ (v, p + 24);
        s->info = mulib_elf_u32_ (v, p + 28);
        s->alignment = mulib_elf_u32_ (v, p + 32);
        s->entsize = mulib_elf_u32_ (v, p + 36);
      }
    else
      {
        s->flags = mulib_elf_u64_ (v, p + 8);
        s->address = mulib_elf_u64_ (v, p + 16);
        s->offset = mulib_elf_u64_ (v, p + 24);
        s->size = mulib_elf_u64_ (v, p + 32);
        s->link = mulib_elf_u32_ (v, p + 40);
        s->info = mulib_elf_u32_ (v, p + 44);
        s->alignment = mulib_elf_u64_ (v, p + 48);
        s->entsize = mulib_elf_u64_ (v, p + 56);
      }
  }
  int
  mulib_elf_bytes (const mulib_elf_t *v, uint64_t off, uint64_t size,
                   const unsigned char **bytes)
  {
    if (!v || !v->data || !bytes)
      return MULIB_ELF_EINVAL;
    if (!mulib_elf_range_ (v, off, size))
      return MULIB_ELF_EBOUNDS;
    *bytes = v->data + (size_t)off;
    return MULIB_ELF_OK;
  }
  int
  mulib_elf_section (const mulib_elf_t *v, size_t index,
                     mulib_elf_section_t *out)
  {
    mulib_elf_section_t s;
    const unsigned char *p;
    if (!v || !v->data || !out)
      return MULIB_ELF_EINVAL;
    if (index >= v->shnum
        || !mulib_elf_table_ (v, v->shoff, v->shnum, v->shentsize)
        || v->shentsize < (v->bits == 32 ? 40 : 64))
      return MULIB_ELF_EBOUNDS;
    p = v->data + (size_t)v->shoff + index * v->shentsize;
    mulib_elf_section_decode_ (v, p, &s);
    if (s.type != 0 && s.type != 8 && !mulib_elf_range_ (v, s.offset, s.size))
      return MULIB_ELF_EBOUNDS;
    if (!mulib_elf_alignment_ (s.alignment))
      return MULIB_ELF_EFORMAT;
    *out = s;
    return MULIB_ELF_OK;
  }
  int
  mulib_elf_program (const mulib_elf_t *v, size_t index,
                     mulib_elf_program_t *out)
  {
    mulib_elf_program_t p;
    const unsigned char *b;
    if (!v || !v->data || !out)
      return MULIB_ELF_EINVAL;
    if (index >= v->phnum
        || !mulib_elf_table_ (v, v->phoff, v->phnum, v->phentsize)
        || v->phentsize < (v->bits == 32 ? 32 : 56))
      return MULIB_ELF_EBOUNDS;
    b = v->data + (size_t)v->phoff + index * v->phentsize;
    p.type = mulib_elf_u32_ (v, b);
    if (v->bits == 32)
      {
        p.offset = mulib_elf_u32_ (v, b + 4);
        p.vaddr = mulib_elf_u32_ (v, b + 8);
        p.paddr = mulib_elf_u32_ (v, b + 12);
        p.filesz = mulib_elf_u32_ (v, b + 16);
        p.memsz = mulib_elf_u32_ (v, b + 20);
        p.flags = mulib_elf_u32_ (v, b + 24);
        p.alignment = mulib_elf_u32_ (v, b + 28);
      }
    else
      {
        p.flags = mulib_elf_u32_ (v, b + 4);
        p.offset = mulib_elf_u64_ (v, b + 8);
        p.vaddr = mulib_elf_u64_ (v, b + 16);
        p.paddr = mulib_elf_u64_ (v, b + 24);
        p.filesz = mulib_elf_u64_ (v, b + 32);
        p.memsz = mulib_elf_u64_ (v, b + 40);
        p.alignment = mulib_elf_u64_ (v, b + 48);
      }
    if (!mulib_elf_range_ (v, p.offset, p.filesz))
      return MULIB_ELF_EBOUNDS;
    if (!mulib_elf_alignment_ (p.alignment)
        || (p.type == 1 && p.filesz > p.memsz))
      return MULIB_ELF_EFORMAT;
    if (p.type == 1 && p.alignment > 1
        && p.vaddr % p.alignment != p.offset % p.alignment)
      return MULIB_ELF_EFORMAT;
    *out = p;
    return MULIB_ELF_OK;
  }
  int
  mulib_elf_string (const mulib_elf_t *v, size_t section, uint64_t off,
                    const char **text, size_t *length)
  {
    mulib_elf_section_t s;
    const unsigned char *start, *end;
    int rc;
    if (!text || !length)
      return MULIB_ELF_EINVAL;
    rc = mulib_elf_section (v, section, &s);
    if (rc)
      return rc;
    if (s.type != 3)
      return MULIB_ELF_ETYPE;
    if (off >= s.size)
      return MULIB_ELF_EBOUNDS;
    start = v->data + (size_t)s.offset + (size_t)off;
    end = (const unsigned char *)memchr (start, 0, (size_t)(s.size - off));
    if (!end)
      return MULIB_ELF_EFORMAT;
    *text = (const char *)start;
    *length = (size_t)(end - start);
    return MULIB_ELF_OK;
  }
  int
  mulib_elf_section_name (const mulib_elf_t *v, size_t index,
                          const char **text, size_t *length)
  {
    mulib_elf_section_t s;
    int rc;
    if (!v)
      return MULIB_ELF_EINVAL;
    rc = mulib_elf_section (v, index, &s);
    if (rc)
      return rc;
    if (!v->shstrndx)
      return MULIB_ELF_ENOTFOUND;
    return mulib_elf_string (v, v->shstrndx, s.name, text, length);
  }
  int
  mulib_elf_symbol_count (const mulib_elf_t *v, size_t section, size_t *count)
  {
    mulib_elf_section_t s, strings;
    int rc;
    if (!count)
      return MULIB_ELF_EINVAL;
    rc = mulib_elf_section (v, section, &s);
    if (rc)
      return rc;
    if (s.type != 2 && s.type != 11)
      return MULIB_ELF_ETYPE;
    if (s.entsize < (v->bits == 32 ? 16u : 24u) || s.size % s.entsize)
      return MULIB_ELF_EFORMAT;
    rc = mulib_elf_section (v, s.link, &strings);
    if (rc)
      return rc;
    if (strings.type != 3)
      return MULIB_ELF_ETYPE;
    *count = (size_t)(s.size / s.entsize);
    return MULIB_ELF_OK;
  }
  int
  mulib_elf_symbol (const mulib_elf_t *v, size_t section, size_t index,
                    mulib_elf_symbol_t *out)
  {
    mulib_elf_section_t s;
    mulib_elf_symbol_t symbol;
    const unsigned char *p;
    size_t count, i;
    int rc;
    if (!out)
      return MULIB_ELF_EINVAL;
    rc = mulib_elf_symbol_count (v, section, &count);
    if (rc)
      return rc;
    if (index >= count)
      return MULIB_ELF_EBOUNDS;
    rc = mulib_elf_section (v, section, &s);
    if (rc)
      return rc;
    p = v->data + (size_t)s.offset + index * (size_t)s.entsize;
    symbol.name = mulib_elf_u32_ (v, p);
    if (v->bits == 32)
      {
        symbol.value = mulib_elf_u32_ (v, p + 4);
        symbol.size = mulib_elf_u32_ (v, p + 8);
        symbol.info = p[12];
        symbol.other = p[13];
        symbol.section_index = mulib_elf_u16_ (v, p + 14);
      }
    else
      {
        symbol.info = p[4];
        symbol.other = p[5];
        symbol.section_index = mulib_elf_u16_ (v, p + 6);
        symbol.value = mulib_elf_u64_ (v, p + 8);
        symbol.size = mulib_elf_u64_ (v, p + 16);
      }
    if (symbol.section_index == 0xffffu)
      {
        int found = 0;
        for (i = 0; i < v->shnum; ++i)
          {
            mulib_elf_section_t extended;
            rc = mulib_elf_section (v, i, &extended);
            if (rc)
              return rc;
            if (extended.type == 18 && extended.link == section)
              {
                uint64_t stride = extended.entsize;
                if (found || stride < 4 || extended.size % stride
                    || extended.size / stride != count)
                  return MULIB_ELF_EFORMAT;
                symbol.section_index
                    = mulib_elf_u32_ (v, v->data + (size_t)extended.offset
                                             + index * (size_t)stride);
                found = 1;
              }
          }
        if (!found || symbol.section_index >= v->shnum)
          return MULIB_ELF_EFORMAT;
      }
    else if (symbol.section_index < 0xff00u && symbol.section_index >= v->shnum
             && symbol.section_index != 0)
      {
        return MULIB_ELF_EFORMAT;
      }
    *out = symbol;
    return MULIB_ELF_OK;
  }
  int
  mulib_elf_symbol_name (const mulib_elf_t *v, size_t section, size_t index,
                         const char **text, size_t *length)
  {
    mulib_elf_symbol_t symbol;
    mulib_elf_section_t s;
    int rc = mulib_elf_symbol (v, section, index, &symbol);
    if (rc)
      return rc;
    rc = mulib_elf_section (v, section, &s);
    if (rc)
      return rc;
    return mulib_elf_string (v, s.link, symbol.name, text, length);
  }
  int
  mulib_elf_open (mulib_elf_t *out, const void *data, size_t size)
  {
    mulib_elf_t v;
    mulib_elf_section_t zero, section;
    mulib_elf_program_t program;
    const unsigned char *p = (const unsigned char *)data;
    uint16_t ehsize, phnum, shnum, shstrndx;
    uint64_t actual_phnum, actual_shnum, actual_shstrndx;
    size_t i;
    int rc;
    if (!out || !p)
      return MULIB_ELF_EINVAL;
    if (size < 16 || memcmp (p, "\177ELF", 4) || (p[4] != 1 && p[4] != 2)
        || (p[5] != 1 && p[5] != 2) || p[6] != 1)
      return MULIB_ELF_EFORMAT;
    memset (&v, 0, sizeof (v));
    v.data = p;
    v.size = size;
    v.bits = p[4] == 1 ? 32u : 64u;
    v.little_endian = p[5] == 1;
    if (size < (v.bits == 32 ? 52u : 64u))
      return MULIB_ELF_EBOUNDS;
    v.type = mulib_elf_u16_ (&v, p + 16);
    v.machine = mulib_elf_u16_ (&v, p + 18);
    if (mulib_elf_u32_ (&v, p + 20) != 1)
      return MULIB_ELF_EFORMAT;
    if (v.bits == 32)
      {
        v.entry = mulib_elf_u32_ (&v, p + 24);
        v.phoff = mulib_elf_u32_ (&v, p + 28);
        v.shoff = mulib_elf_u32_ (&v, p + 32);
        v.flags = mulib_elf_u32_ (&v, p + 36);
        ehsize = mulib_elf_u16_ (&v, p + 40);
        v.phentsize = mulib_elf_u16_ (&v, p + 42);
        phnum = mulib_elf_u16_ (&v, p + 44);
        v.shentsize = mulib_elf_u16_ (&v, p + 46);
        shnum = mulib_elf_u16_ (&v, p + 48);
        shstrndx = mulib_elf_u16_ (&v, p + 50);
      }
    else
      {
        v.entry = mulib_elf_u64_ (&v, p + 24);
        v.phoff = mulib_elf_u64_ (&v, p + 32);
        v.shoff = mulib_elf_u64_ (&v, p + 40);
        v.flags = mulib_elf_u32_ (&v, p + 48);
        ehsize = mulib_elf_u16_ (&v, p + 52);
        v.phentsize = mulib_elf_u16_ (&v, p + 54);
        phnum = mulib_elf_u16_ (&v, p + 56);
        v.shentsize = mulib_elf_u16_ (&v, p + 58);
        shnum = mulib_elf_u16_ (&v, p + 60);
        shstrndx = mulib_elf_u16_ (&v, p + 62);
      }
    if (ehsize < (v.bits == 32 ? 52u : 64u) || ehsize > size)
      return MULIB_ELF_EFORMAT;
    actual_phnum = phnum;
    actual_shnum = shnum;
    actual_shstrndx = shstrndx;
    if (v.shoff)
      {
        if (v.shentsize < (v.bits == 32 ? 40u : 64u)
            || !mulib_elf_table_ (&v, v.shoff, 1, v.shentsize))
          return MULIB_ELF_EBOUNDS;
        mulib_elf_section_decode_ (&v, p + (size_t)v.shoff, &zero);
        if (zero.type != 0)
          return MULIB_ELF_EFORMAT;
        if (!shnum)
          actual_shnum = zero.size;
        if (phnum == 0xffffu)
          actual_phnum = zero.info;
        if (shstrndx == 0xffffu)
          actual_shstrndx = zero.link;
        if (!actual_shnum)
          return MULIB_ELF_EFORMAT;
      }
    else if (shnum || shstrndx || phnum == 0xffffu)
      return MULIB_ELF_EFORMAT;
    if (actual_shnum > SIZE_MAX || actual_phnum > SIZE_MAX
        || actual_shstrndx > SIZE_MAX)
      return MULIB_ELF_EBOUNDS;
    v.shnum = (size_t)actual_shnum;
    v.phnum = (size_t)actual_phnum;
    v.shstrndx = (size_t)actual_shstrndx;
    if ((v.phnum && (!v.phoff || v.phentsize < (v.bits == 32 ? 32u : 56u)))
        || !mulib_elf_table_ (&v, v.phoff, v.phnum, v.phentsize)
        || !mulib_elf_table_ (&v, v.shoff, v.shnum, v.shentsize))
      return MULIB_ELF_EBOUNDS;
    if (v.shstrndx && v.shstrndx >= v.shnum)
      return MULIB_ELF_EFORMAT;
    for (i = 0; i < v.shnum; ++i)
      {
        rc = mulib_elf_section (&v, i, &section);
        if (rc)
          return rc;
        if (i == v.shstrndx && i && section.type != 3)
          return MULIB_ELF_ETYPE;
        if (section.type == 2 || section.type == 11)
          {
            size_t count;
            rc = mulib_elf_symbol_count (&v, i, &count);
            if (rc)
              return rc;
          }
      }
    for (i = 0; i < v.phnum; ++i)
      {
        rc = mulib_elf_program (&v, i, &program);
        if (rc)
          return rc;
      }
    *out = v;
    return MULIB_ELF_OK;
  }
#ifdef __cplusplus
}
#endif
#endif /* implementation */

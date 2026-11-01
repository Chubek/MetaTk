/**
 * @file mulib-dsymspec.h
 * @brief Owned queries over the supplied schema-1 architecture metadata.
 * Define MULIB_IMPL_DSYMSPEC, JSON, PARSEC, STRBUF and UTF8 explicitly once.
 * The generated snapshot is derived from dsymspec/, not an invented ISA table.
 */
#ifndef MULIB_DSYMSPEC_H
#define MULIB_DSYMSPEC_H
#include "mulib-json.h"
#include <stddef.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_dsymspec mulib_dsymspec_t;
  /** @brief Static message; offset is a JSON byte offset, or zero for
   * validation. */
  typedef struct mulib_dsymspec_error
  {
    size_t offset;
    const char *message;
  } mulib_dsymspec_error_t;
  /** @brief Number of generated architectures; all supplied YAML files are
   * included. */
  size_t mulib_dsymspec_builtin_count (void);
  /** @brief Borrow an architecture identifier, or NULL for an invalid index.
   */
  const char *mulib_dsymspec_builtin_name (size_t index);
  /** @brief Load an owning copy of a generated architecture by exact
   * identifier. */
  mulib_dsymspec_t *
  mulib_dsymspec_load_builtin (const char *architecture,
                               mulib_dsymspec_error_t *error);
  /** @brief Parse schema-1 JSON (not YAML), copying all data, with core
   * validation. tools/dsymspec_generate.py --json ARCH produces
   * runtime-loadable JSON including resolved executable forms. Raw schema-1
   * JSON is queryable without those forms. Not a general JSON Schema
   * validator; see the manual for validation boundaries.
   */
  mulib_dsymspec_t *mulib_dsymspec_load_json (const char *source,
                                              size_t length,
                                              mulib_dsymspec_error_t *error);
  /** @brief Destroy metadata and invalidate all borrowed query results. */
  void mulib_dsymspec_destroy (mulib_dsymspec_t *specification);
  /** @brief Borrow the immutable complete metadata DOM (including
   * pseudo-code). */
  const mulib_json_t *
  mulib_dsymspec_document (const mulib_dsymspec_t *specification);
  /** @brief Count array records in a named root group; absent/non-array means
   * zero. */
  size_t mulib_dsymspec_count (const mulib_dsymspec_t *specification,
                               const char *group);
  /** @brief Borrow an indexed record, or NULL; never mutate borrowed metadata.
   */
  const mulib_json_t *
  mulib_dsymspec_record (const mulib_dsymspec_t *specification,
                         const char *group, size_t index);
  /** @brief Find by name; targets use arch and forms use mnemonic (first
   * match). Exact, case-sensitive matching; no alias expansion or pseudo-code
   * execution.
   */
  const mulib_json_t *
  mulib_dsymspec_find (const mulib_dsymspec_t *specification,
                       const char *group, const char *name);
  /** @brief Borrow the generated resolved asmtk encoding array, or NULL. */
  const mulib_json_t *
  mulib_dsymspec_encodings (const mulib_dsymspec_t *specification);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_DSYMSPEC) && !defined(MULIB_DSYMSPEC_IMPLEMENTED)
#define MULIB_DSYMSPEC_IMPLEMENTED
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifndef MULIB_DSYMSPEC_MAX_BYTES
#define MULIB_DSYMSPEC_MAX_BYTES (4U * 1024U * 1024U)
#endif
#ifdef __cplusplus
extern "C"
{
#endif
  struct mulib_dsymspec
  {
    mulib_json_t *document;
  };
  typedef struct mulib_dsymspec_builtin_
  {
    const char *name;
    const char *const *chunks;
  } mulib_dsymspec_builtin_;
#include "mulib-dsymspec-data.inc"
  static void
  mulib_dsymspec_error_ (mulib_dsymspec_error_t *e, size_t at,
                         const char *message)
  {
    if (e)
      {
        e->offset = at;
        e->message = message;
      }
  }
  static const char *
  mulib_dsymspec_text_ (const mulib_json_t *v)
  {
    size_t n;
    const char *text = mulib_json_text (v, &n);
    return text && !memchr (text, 0, n) ? text : NULL;
  }
  static int
  mulib_dsymspec_name_ (const mulib_json_t *v)
  {
    const char *s = mulib_dsymspec_text_ (v);
    size_t i, n;
    if (!s || !(n = strlen (s)) || n > 128)
      return 0;
    for (i = 0; i < n; ++i)
      if ((unsigned char)s[i] < 33 || (unsigned char)s[i] > 126)
        return 0;
    return 1;
  }
  static int
  mulib_dsymspec_integer_ (const mulib_json_t *v, unsigned min, unsigned max)
  {
    double n;
    if (mulib_json_typeof (v) != MULIB_JSON_NUMBER)
      return 0;
    n = mulib_json_numeric (v);
    return n >= min && n <= max && n == (double)(uint64_t)n;
  }
  static const mulib_json_t *
  mulib_dsymspec_lookup_ (const mulib_json_t *array, const char *key,
                          const char *name)
  {
    size_t i;
    for (i = 0; i < mulib_json_size (array); ++i)
      {
        const mulib_json_t *record = mulib_json_at (array, i);
        const char *s = mulib_dsymspec_text_ (mulib_json_get (record, key));
        if (s && !strcmp (s, name))
          return record;
      }
    return NULL;
  }
  static int
  mulib_dsymspec_member_ (const char *value, const char *const *choices,
                          size_t n)
  {
    size_t i;
    if (!value)
      return 0;
    for (i = 0; i < n; ++i)
      if (!strcmp (value, choices[i]))
        return 1;
    return 0;
  }
  static int
  mulib_dsymspec_validate_ (const mulib_json_t *root, const char **message)
  {
    static const char *const named[]
        = { "features", "registers", "mnemonics", "encoders",
            "syntaxes", "sections",  "linkages",  "relocs" };
    static const char *const classes[]
        = { "gpr",  "seg", "ctrl",   "debug",  "vec",
            "mask", "fp",  "status", "special" };
    static const char *const forms[]
        = { "Fixed",          "Relative",       "Indirect",
            "OpcodeRegister", "Unary",          "RegisterRegister",
            "Load",           "Store",          "MoveImmediate",
            "ShiftCL",        "ShiftImmediate", "ArithmeticImmediate" };
    const mulib_json_t *targets, *a, *codegen;
    size_t i, j, k;
    *message = "invalid schema-1 document or targets";
    targets = mulib_json_get (root, "targets");
    if (mulib_json_typeof (root) != MULIB_JSON_OBJECT
        || !mulib_dsymspec_integer_ (mulib_json_get (root, "schema"), 1, 1)
        || mulib_json_typeof (targets) != MULIB_JSON_ARRAY
        || !mulib_json_size (targets) || mulib_json_size (targets) > 4096)
      return 0;
    for (i = 0; i < mulib_json_size (targets); ++i)
      {
        const mulib_json_t *r = mulib_json_at (targets, i);
        const char *order
            = mulib_dsymspec_text_ (mulib_json_get (r, "byte_order"));
        if (!mulib_dsymspec_name_ (mulib_json_get (r, "arch"))
            || !mulib_dsymspec_integer_ (mulib_json_get (r, "addr_size"), 1,
                                         UINT32_MAX)
            || !order
            || (strcmp (order, "little") && strcmp (order, "le")
                && strcmp (order, "big") && strcmp (order, "be")))
          return 0;
        for (j = 0; j < i; ++j)
          {
            const char *previous = mulib_dsymspec_text_ (
                mulib_json_get (mulib_json_at (targets, j), "arch"));
            if (!strcmp (previous,
                         mulib_dsymspec_text_ (mulib_json_get (r, "arch"))))
              return 0;
          }
        a = mulib_json_get (r, "align");
        if (a)
          {
            uint64_t align;
            if (!mulib_dsymspec_integer_ (a, 1, UINT32_MAX))
              return 0;
            align = (uint64_t)mulib_json_numeric (a);
            if (align & (align - 1))
              return 0;
          }
      }
    *message = "invalid, duplicate, or oversized named metadata group";
    for (k = 0; k < sizeof (named) / sizeof (named[0]); ++k)
      {
        a = mulib_json_get (root, named[k]);
        if (!a)
          continue;
        if (mulib_json_typeof (a) != MULIB_JSON_ARRAY
            || mulib_json_size (a) > 4096)
          return 0;
        for (i = 0; i < mulib_json_size (a); ++i)
          {
            const mulib_json_t *r = mulib_json_at (a, i);
            const char *name;
            if (!mulib_dsymspec_name_ (mulib_json_get (r, "name")))
              return 0;
            name = mulib_dsymspec_text_ (mulib_json_get (r, "name"));
            for (j = 0; j < i; ++j)
              {
                const char *previous = mulib_dsymspec_text_ (
                    mulib_json_get (mulib_json_at (a, j), "name"));
                if (!strcmp (previous, name))
                  return 0;
              }
            if (!strcmp (named[k], "registers"))
              {
                unsigned w;
                const char *c
                    = mulib_dsymspec_text_ (mulib_json_get (r, "class"));
                if (!mulib_dsymspec_member_ (
                        c, classes, sizeof (classes) / sizeof (classes[0]))
                    || !mulib_dsymspec_integer_ (mulib_json_get (r, "width"),
                                                 1, 512)
                    || !mulib_dsymspec_integer_ (mulib_json_get (r, "enc"), 0,
                                                 UINT32_MAX))
                  return 0;
                w = (unsigned)mulib_json_numeric (mulib_json_get (r, "width"));
                if (w != 8 && w != 16 && w != 32 && w != 64 && w != 80
                    && w != 128 && w != 256 && w != 512)
                  return 0;
                if (mulib_json_get (r, "parent"))
                  {
                    const char *parent
                        = mulib_dsymspec_text_ (mulib_json_get (r, "parent"));
                    if (!parent || !mulib_dsymspec_lookup_ (a, "name", parent))
                      return 0;
                  }
              }
            else if (!strcmp (named[k], "encoders"))
              {
                const char *body
                    = mulib_dsymspec_text_ (mulib_json_get (r, "body"));
                if (!body || !*body)
                  return 0;
              }
            else if (!strcmp (named[k], "relocs")
                     && !mulib_dsymspec_integer_ (mulib_json_get (r, "width"),
                                                  1, UINT32_MAX))
              return 0;
          }
      }
    *message = "invalid form or unresolved mnemonic/encoder reference";
    a = mulib_json_get (root, "forms");
    if (a
        && (mulib_json_typeof (a) != MULIB_JSON_ARRAY
            || mulib_json_size (a) > 4096))
      return 0;
    for (i = 0; i < mulib_json_size (a); ++i)
      {
        const mulib_json_t *r = mulib_json_at (a, i);
        const char *mnemonic
            = mulib_dsymspec_text_ (mulib_json_get (r, "mnemonic"));
        const char *encoder
            = mulib_dsymspec_text_ (mulib_json_get (r, "encoder"));
        const mulib_json_t *ops = mulib_json_get (r, "operands");
        if (!mnemonic || !encoder
            || !mulib_dsymspec_lookup_ (mulib_json_get (root, "mnemonics"),
                                        "name", mnemonic)
            || !mulib_dsymspec_lookup_ (mulib_json_get (root, "encoders"),
                                        "name", encoder))
          return 0;
        if (ops && mulib_json_typeof (ops) != MULIB_JSON_ARRAY)
          return 0;
      }
    codegen = mulib_json_get (root, "_mulib_codegen");
    if (!codegen)
      return 1;
    *message = "invalid resolved codegen metadata";
    a = mulib_json_get (codegen, "encodings");
    if (!mulib_dsymspec_integer_ (mulib_json_get (codegen, "version"), 1, 1)
        || mulib_json_typeof (a) != MULIB_JSON_ARRAY
        || mulib_json_size (a) > 4096)
      return 0;
    for (i = 0; i < mulib_json_size (a); ++i)
      {
        const mulib_json_t *r = mulib_json_at (a, i);
        const mulib_json_t *op = mulib_json_get (r, "opcode"),
                           *widths = mulib_json_get (r, "widths");
        const char *f = mulib_dsymspec_text_ (mulib_json_get (r, "form"));
        if (!mulib_dsymspec_name_ (mulib_json_get (r, "mnemonic"))
            || !mulib_dsymspec_member_ (f, forms,
                                        sizeof (forms) / sizeof (forms[0]))
            || mulib_json_typeof (op) != MULIB_JSON_ARRAY
            || !mulib_json_size (op) || mulib_json_size (op) > 3
            || mulib_json_typeof (widths) != MULIB_JSON_ARRAY
            || mulib_json_size (widths) > 3
            || !mulib_dsymspec_integer_ (mulib_json_get (r, "group"), 0, 7)
            || !mulib_dsymspec_integer_ (mulib_json_get (r, "reg_operand"), 0,
                                         1)
            || mulib_json_typeof (mulib_json_get (r, "rex_w"))
                   != MULIB_JSON_BOOL)
          return 0;
        for (j = 0; j < mulib_json_size (op); ++j)
          if (!mulib_dsymspec_integer_ (mulib_json_at (op, j), 0, 255))
            return 0;
        for (j = 0; j < mulib_json_size (widths); ++j)
          {
            unsigned w;
            if (!mulib_dsymspec_integer_ (mulib_json_at (widths, j), 1, 64))
              return 0;
            w = (unsigned)mulib_json_numeric (mulib_json_at (widths, j));
            if (w != 8 && w != 32 && w != 64)
              return 0;
          }
        if (!strcmp (f, "Relative"))
          {
            const char *reloc
                = mulib_dsymspec_text_ (mulib_json_get (r, "relocation_name"));
            if (!reloc
                || !mulib_dsymspec_lookup_ (mulib_json_get (root, "relocs"),
                                            "name", reloc))
              return 0;
          }
      }
    return 1;
  }
  size_t
  mulib_dsymspec_builtin_count (void)
  {
    return sizeof (mulib_dsymspec_builtins_)
           / sizeof (mulib_dsymspec_builtins_[0]);
  }
  const char *
  mulib_dsymspec_builtin_name (size_t i)
  {
    return i < mulib_dsymspec_builtin_count ()
               ? mulib_dsymspec_builtins_[i].name
               : NULL;
  }
  mulib_dsymspec_t *
  mulib_dsymspec_load_json (const char *source, size_t n,
                            mulib_dsymspec_error_t *error)
  {
    mulib_json_error_t parse_error;
    mulib_json_t *root;
    mulib_dsymspec_t *spec;
    const char *message;
    mulib_dsymspec_error_ (error, 0, NULL);
    if ((!source && n) || n > MULIB_DSYMSPEC_MAX_BYTES)
      {
        mulib_dsymspec_error_ (
            error, 0, "invalid input or metadata byte limit exceeded");
        return NULL;
      }
    root = mulib_json_parse (source, n, &parse_error);
    if (!root)
      {
        mulib_dsymspec_error_ (error, parse_error.offset, parse_error.message);
        return NULL;
      }
    if (!mulib_dsymspec_validate_ (root, &message))
      {
        mulib_json_destroy (root);
        mulib_dsymspec_error_ (error, 0, message);
        return NULL;
      }
    spec = (mulib_dsymspec_t *)malloc (sizeof (*spec));
    if (!spec)
      {
        mulib_json_destroy (root);
        mulib_dsymspec_error_ (error, 0, "out of memory");
        return NULL;
      }
    spec->document = root;
    return spec;
  }
  mulib_dsymspec_t *
  mulib_dsymspec_load_builtin (const char *name, mulib_dsymspec_error_t *error)
  {
    size_t i, j, length = 0, at = 0;
    char *source;
    mulib_dsymspec_t *spec;
    const char *const *chunks = NULL;
    mulib_dsymspec_error_ (error, 0, NULL);
    if (name)
      for (i = 0; i < mulib_dsymspec_builtin_count (); ++i)
        if (!strcmp (name, mulib_dsymspec_builtins_[i].name))
          {
            chunks = mulib_dsymspec_builtins_[i].chunks;
            break;
          }
    if (!chunks)
      {
        mulib_dsymspec_error_ (error, 0, "unknown supplied architecture");
        return NULL;
      }
    for (j = 0; chunks[j]; ++j)
      {
        size_t n = strlen (chunks[j]);
        if (n > SIZE_MAX - length - 1 || n > MULIB_DSYMSPEC_MAX_BYTES - length)
          {
            mulib_dsymspec_error_ (error, 0, "metadata byte limit exceeded");
            return NULL;
          }
        length += n;
      }
    source = (char *)malloc (length + 1);
    if (!source)
      {
        mulib_dsymspec_error_ (error, 0, "out of memory");
        return NULL;
      }
    for (j = 0; chunks[j]; ++j)
      {
        size_t n = strlen (chunks[j]);
        memcpy (source + at, chunks[j], n);
        at += n;
      }
    source[length] = 0;
    spec = mulib_dsymspec_load_json (source, length, error);
    free (source);
    return spec;
  }
  void
  mulib_dsymspec_destroy (mulib_dsymspec_t *spec)
  {
    if (spec)
      {
        mulib_json_destroy (spec->document);
        free (spec);
      }
  }
  const mulib_json_t *
  mulib_dsymspec_document (const mulib_dsymspec_t *spec)
  {
    return spec ? spec->document : NULL;
  }
  size_t
  mulib_dsymspec_count (const mulib_dsymspec_t *spec, const char *group)
  {
    const mulib_json_t *a
        = spec && group ? mulib_json_get (spec->document, group) : NULL;
    return mulib_json_typeof (a) == MULIB_JSON_ARRAY ? mulib_json_size (a) : 0;
  }
  const mulib_json_t *
  mulib_dsymspec_record (const mulib_dsymspec_t *spec, const char *group,
                         size_t i)
  {
    return i < mulib_dsymspec_count (spec, group)
               ? mulib_json_at (mulib_json_get (spec->document, group), i)
               : NULL;
  }
  const mulib_json_t *
  mulib_dsymspec_find (const mulib_dsymspec_t *spec, const char *group,
                       const char *name)
  {
    const char *key;
    if (!spec || !group || !name)
      return NULL;
    key = !strcmp (group, "targets") ? "arch"
          : !strcmp (group, "forms") ? "mnemonic"
                                     : "name";
    if (!mulib_dsymspec_count (spec, group))
      return NULL;
    return mulib_dsymspec_lookup_ (mulib_json_get (spec->document, group), key,
                                   name);
  }
  const mulib_json_t *
  mulib_dsymspec_encodings (const mulib_dsymspec_t *spec)
  {
    return spec ? mulib_json_get (
                      mulib_json_get (spec->document, "_mulib_codegen"),
                      "encodings")
                : NULL;
  }
#ifdef __cplusplus
}
#endif
#endif

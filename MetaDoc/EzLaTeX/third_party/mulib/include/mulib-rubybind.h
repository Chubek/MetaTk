/**
 * @file mulib-rubybind.h
 * @brief Generate Ruby Fiddle source without linking to Ruby.
 *
 * Uses mulib-pybind.h's common C signature descriptors and validator. Define
 * MULIB_IMPL_RUBYBIND, MULIB_IMPL_PYBIND, and MULIB_IMPL_STRBUF in one C TU.
 */
#ifndef MULIB_RUBYBIND_H
#define MULIB_RUBYBIND_H
#include "mulib-pybind.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /**
   * @brief Emit an ASCII Ruby module named MuLIBBindings with singleton
   * methods.
   * @details Ownership, limits, and errors match mulib_pybind_generate. Ruby
   * names must start with a lowercase ASCII letter or underscore and cannot be
   * keywords, numbered-parameter names (_1 through _9), or singleton-method
   * hooks. CSTRING accepts String/nil and returns a copied String/nil. POINTER
   * returns a Fiddle::Pointer. Generated code uses Ruby's standard-library
   * Fiddle; the C generator itself has no Ruby dependency. Only native C ABI
   * is supported.
   */
  mulib_bind_status_t mulib_rubybind_generate (
      const char *library, const mulib_bind_function_t *functions,
      size_t count, char **source, size_t *length, mulib_bind_error_t *error);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_RUBYBIND_H */

#if defined(MULIB_IMPL_RUBYBIND) && !defined(MULIB_RUBYBIND_IMPLEMENTED)
#define MULIB_RUBYBIND_IMPLEMENTED
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif
  static mulib_bind_status_t
  mulib_rubybind__error (mulib_bind_error_t *e, mulib_bind_status_t s,
                         size_t f)
  {
    if (e)
      {
        e->status = s;
        e->function_index = f;
        e->argument_index = SIZE_MAX;
      }
    return s;
  }

  static int
  mulib_rubybind__keyword (const char *name)
  {
    static const char *const words[] = { "alias",
                                         "and",
                                         "begin",
                                         "break",
                                         "case",
                                         "class",
                                         "def",
                                         "defined",
                                         "do",
                                         "else",
                                         "elsif",
                                         "end",
                                         "ensure",
                                         "false",
                                         "for",
                                         "if",
                                         "in",
                                         "module",
                                         "next",
                                         "nil",
                                         "not",
                                         "or",
                                         "redo",
                                         "rescue",
                                         "retry",
                                         "return",
                                         "self",
                                         "super",
                                         "then",
                                         "true",
                                         "undef",
                                         "unless",
                                         "until",
                                         "when",
                                         "while",
                                         "yield",
                                         "__FILE__",
                                         "__LINE__",
                                         "__ENCODING__",
                                         "singleton_method_added",
                                         "singleton_method_removed",
                                         "singleton_method_undefined" };
    size_t i;
    if (!((name[0] >= 'a' && name[0] <= 'z') || name[0] == '_'))
      return 1;
    if (name[0] == '_' && name[1] >= '1' && name[1] <= '9' && !name[2])
      return 1;
    for (i = 0; i < sizeof (words) / sizeof (words[0]); ++i)
      if (!strcmp (name, words[i]))
        return 1;
    return 0;
  }

  static int
  mulib_rubybind__bytes (mulib_strbuf_t *b, const char *s)
  {
    static const char hex[] = "0123456789abcdef";
    const unsigned char *p = (const unsigned char *)s;
    if (!mulib_strbuf_putc (b, '"'))
      return 0;
    while (*p)
      {
        char escape[4] = { '\\', 'x', hex[*p >> 4], hex[*p & 15] };
        if (!mulib_strbuf_append_n (b, escape, sizeof (escape)))
          return 0;
        ++p;
      }
    return mulib_strbuf_append (b, "\".b");
  }

  static int
  mulib_rubybind__argument (mulib_strbuf_t *b, size_t i)
  {
    char number[32];
    int n = snprintf (number, sizeof (number), "a%zu", i);
    return n > 0 && (size_t)n < sizeof (number)
           && mulib_strbuf_append_n (b, number, (size_t)n);
  }

  mulib_bind_status_t
  mulib_rubybind_generate (const char *library, const mulib_bind_function_t *f,
                           size_t count, char **source, size_t *length,
                           mulib_bind_error_t *e)
  {
    static const char *const types[]
        = { "::Fiddle::TYPE_VOID",       "::Fiddle::TYPE_CHAR",
            "-::Fiddle::TYPE_CHAR",      "::Fiddle::TYPE_SHORT",
            "-::Fiddle::TYPE_SHORT",     "::Fiddle::TYPE_INT",
            "-::Fiddle::TYPE_INT",       "::Fiddle::TYPE_LONG",
            "-::Fiddle::TYPE_LONG",      "::Fiddle::TYPE_LONG_LONG",
            "-::Fiddle::TYPE_LONG_LONG", "::Fiddle::TYPE_FLOAT",
            "::Fiddle::TYPE_DOUBLE",     "::Fiddle::TYPE_VOIDP",
            "::Fiddle::TYPE_VOIDP" };
    mulib_strbuf_t b;
    mulib_bind_status_t status;
    size_t i, j;
    if (source)
      *source = NULL;
    if (length)
      *length = 0;
    if (!source || !library || !library[0])
      return mulib_rubybind__error (e, MULIB_BIND_INVALID_ARGUMENT, SIZE_MAX);
    for (i = 0; i <= MULIB_BIND_MAX_TEXT && library[i]; ++i)
      {
      }
    if (i > MULIB_BIND_MAX_TEXT)
      return mulib_rubybind__error (e, MULIB_BIND_INVALID_ARGUMENT, SIZE_MAX);
    status = mulib_bind_validate (f, count, e);
    if (status != MULIB_BIND_OK)
      return status;
    for (i = 0; i < count; ++i)
      if (mulib_rubybind__keyword (f[i].name))
        return mulib_rubybind__error (e, MULIB_BIND_INVALID_NAME, i);
    mulib_strbuf_init (&b);
    if (!mulib_strbuf_append (
            &b, "# Generated by MuLIB; native C ABI, borrowed pointers.\n"
                "require 'fiddle'\nmodule MuLIBBindings\n  LIBRARY = "
                "::Fiddle.dlopen(")
        || !mulib_rubybind__bytes (&b, library)
        || !mulib_strbuf_append (&b, ")\n  FUNCTIONS = {\n"))
      goto nomem;
    for (i = 0; i < count; ++i)
      {
        if (!mulib_strbuf_append (&b, "    ")
            || !mulib_rubybind__bytes (&b, f[i].name)
            || !mulib_strbuf_append (&b, " => ::Fiddle::Function.new(LIBRARY[")
            || !mulib_rubybind__bytes (&b,
                                       f[i].symbol ? f[i].symbol : f[i].name)
            || !mulib_strbuf_append (&b, "], ["))
          goto nomem;
        for (j = 0; j < f[i].argument_count; ++j)
          {
            if ((j && !mulib_strbuf_append (&b, ", "))
                || !mulib_strbuf_append (&b, types[f[i].arguments[j]]))
              goto nomem;
          }
        if (!mulib_strbuf_append (&b, "], ")
            || !mulib_strbuf_append (&b, types[f[i].result])
            || !mulib_strbuf_append (&b, "),\n"))
          goto nomem;
      }
    if (!mulib_strbuf_append (&b, "  }.freeze\n"))
      goto nomem;
    for (i = 0; i < count; ++i)
      {
        if (!mulib_strbuf_append (&b, "  def self.")
            || !mulib_strbuf_append (&b, f[i].name)
            || !mulib_strbuf_putc (&b, '('))
          goto nomem;
        for (j = 0; j < f[i].argument_count; ++j)
          if ((j && !mulib_strbuf_append (&b, ", "))
              || !mulib_rubybind__argument (&b, j))
            goto nomem;
        if (!mulib_strbuf_append (&b, ")\n    result = FUNCTIONS.fetch(")
            || !mulib_rubybind__bytes (&b, f[i].name)
            || !mulib_strbuf_append (&b, ").call("))
          goto nomem;
        for (j = 0; j < f[i].argument_count; ++j)
          if ((j && !mulib_strbuf_append (&b, ", "))
              || !mulib_rubybind__argument (&b, j))
            goto nomem;
        if (!mulib_strbuf_append (&b, ")\n    ")
            || !mulib_strbuf_append (&b,
                                     f[i].result == MULIB_BIND_CSTRING
                                         ? "result.null? ? nil : result.to_s\n"
                                         : "result\n")
            || !mulib_strbuf_append (&b, "  end\n"))
          goto nomem;
      }
    if (!mulib_strbuf_append (&b, "end\n"))
      goto nomem;
    *source = b.data;
    if (length)
      *length = b.length;
    return MULIB_BIND_OK;
  nomem:
    mulib_strbuf_free (&b);
    return mulib_rubybind__error (e, MULIB_BIND_NO_MEMORY, SIZE_MAX);
  }
#ifdef __cplusplus
}
#endif
#endif /* implementation */

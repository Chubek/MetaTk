/**
 * @file mulib-parsec.h
 * @brief Backtracking byte-parser combinators with transactional captures.
 * Define MULIB_IMPL_PARSEC in one C11 translation unit. No module
 * dependencies.
 */
#ifndef MULIB_PARSEC_H
#define MULIB_PARSEC_H
#include "mulib-ast.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct mulib_parsec_parser mulib_parsec_parser_t;
  /** @brief A byte predicate must be pure; nonzero means acceptance. */
  typedef int (*mulib_parsec_predicate_fn) (unsigned char byte, void *user);
  typedef enum mulib_parsec_kind
  {
    MULIB_PARSEC_EMPTY,
    MULIB_PARSEC_BYTE,
    MULIB_PARSEC_RANGE,
    MULIB_PARSEC_SET,
    MULIB_PARSEC_ANY,
    MULIB_PARSEC_LITERAL,
    MULIB_PARSEC_END,
    MULIB_PARSEC_SEQUENCE,
    MULIB_PARSEC_CHOICE,
    MULIB_PARSEC_REPEAT,
    MULIB_PARSEC_AND,
    MULIB_PARSEC_NOT,
    MULIB_PARSEC_CAPTURE,
    MULIB_PARSEC_REF,
    MULIB_PARSEC_PREDICATE
  } mulib_parsec_kind;
  /** @brief Borrowed, copyable parser description. Referents must outlive a
   * run. */
  struct mulib_parsec_parser
  {
    mulib_parsec_kind kind;
    const char *label;
    union
    {
      struct
      {
        unsigned char low, high;
      } range;
      struct
      {
        const unsigned char *bytes;
        size_t length;
      } text;
      struct
      {
        const mulib_parsec_parser_t *items;
        size_t count;
      } list;
      struct
      {
        const mulib_parsec_parser_t *item;
        size_t min, max, tag;
      } unary;
      const mulib_parsec_parser_t *const *reference;
      struct
      {
        mulib_parsec_predicate_fn function;
        void *user;
      } predicate;
    } as;
  };
  typedef enum mulib_parsec_error_code
  {
    MULIB_PARSEC_EXPECTED = 1,
    MULIB_PARSEC_INVALID,
    MULIB_PARSEC_MEMORY,
    MULIB_PARSEC_LIMIT,
    MULIB_PARSEC_NO_PROGRESS
  } mulib_parsec_error_code;
  typedef struct mulib_parsec_error
  {
    mulib_parsec_error_code code;
    size_t offset, line, column;
    const char *expected;
  } mulib_parsec_error_t;
  /** @brief Postorder capture; [first,index) comprises all its descendants. */
  typedef struct mulib_parsec_capture
  {
    size_t tag, offset, length, first;
  } mulib_parsec_capture_t;
  /** @brief Zero fields select defaults: depth 256, steps/captures 1000000
   * each. */
  typedef struct mulib_parsec_options
  {
    size_t max_depth, max_steps, max_captures;
  } mulib_parsec_options_t;
  /** @brief Owned captures on success; free before reusing a result object. */
  typedef struct mulib_parsec_result
  {
    size_t consumed, count;
    mulib_parsec_capture_t *captures;
    mulib_parsec_error_t error;
  } mulib_parsec_result_t;

  /** @brief Match an empty sequence. */
  mulib_parsec_parser_t mulib_parsec_empty (void);
  /** @brief Match exactly one specified byte (including NUL). */
  mulib_parsec_parser_t mulib_parsec_byte (unsigned char byte);
  /** @brief Match one byte in an inclusive range. */
  mulib_parsec_parser_t mulib_parsec_range (unsigned char low,
                                            unsigned char high);
  /** @brief Match a byte in a borrowed 256-bit bitmap, low bit first. */
  mulib_parsec_parser_t mulib_parsec_set (const unsigned char bitmap[32]);
  /** @brief Match any one byte, including NUL and newline. */
  mulib_parsec_parser_t mulib_parsec_any (void);
  /** @brief Match borrowed literal bytes. NULL is valid only at length zero.
   */
  mulib_parsec_parser_t mulib_parsec_literal (const char *bytes,
                                              size_t length);
  /** @brief Match only at the end of input. */
  mulib_parsec_parser_t mulib_parsec_end (void);
  /** @brief Match every child, rolling back the entire sequence on failure. */
  mulib_parsec_parser_t
  mulib_parsec_sequence (const mulib_parsec_parser_t *items, size_t count);
  /** @brief Ordered, fully backtracking choice; empty choice never matches. */
  mulib_parsec_parser_t
  mulib_parsec_choice (const mulib_parsec_parser_t *items, size_t count);
  /** @brief Greedy PEG repetition; SIZE_MAX is unbounded. Does not give bytes
   * back. */
  mulib_parsec_parser_t mulib_parsec_repeat (const mulib_parsec_parser_t *item,
                                             size_t min, size_t max);
  /** @brief Positive or negative lookahead; neither consumes nor captures. */
  mulib_parsec_parser_t
  mulib_parsec_lookahead (const mulib_parsec_parser_t *item, int positive);
  /** @brief Record a successful child's span after its descendant captures. */
  mulib_parsec_parser_t
  mulib_parsec_capture (const mulib_parsec_parser_t *item, size_t tag);
  /** @brief Late-bound reference, permitting recursive grammars (not left
   * recursion). */
  mulib_parsec_parser_t
  mulib_parsec_ref (const mulib_parsec_parser_t *const *target);
  /** @brief Match one byte satisfying a pure user predicate. */
  mulib_parsec_parser_t
  mulib_parsec_satisfy (mulib_parsec_predicate_fn predicate, void *user,
                        const char *label);
  /** @brief Give a parser a borrowed diagnostic label. */
  mulib_parsec_parser_t mulib_parsec_named (mulib_parsec_parser_t parser,
                                            const char *label);
  /** @brief Run a prefix parser: 1 match, 0 mismatch, -1
   * invalid/limit/allocation error. Result is initialized even on failure.
   * Input/grammar remain borrowed; labels in diagnostics remain borrowed.
   * Append end() to demand entire-input parsing.
   */
  int mulib_parsec_run (const mulib_parsec_parser_t *parser,
                        const char *source, size_t length,
                        const mulib_parsec_options_t *options,
                        mulib_parsec_result_t *result);
  /** @brief Release captures and clear a result. NULL is harmless. */
  void mulib_parsec_result_free (mulib_parsec_result_t *result);
  /** @brief Owned scannerless grammar API. Define MULIB_IMPL_PARSER in
   * addition to PARSEC, PEG, STRBUF, GLOB and AST in one translation unit.
   * This opt-in keeps the original combinator implementation dependency-free.
   */
  typedef struct mulib_parser mulib_parser_t;
  typedef enum mulib_parser_error_code
  {
    MULIB_PARSER_OK,
    MULIB_PARSER_IO,
    MULIB_PARSER_SYNTAX,
    MULIB_PARSER_MATCH,
    MULIB_PARSER_MEMORY,
    MULIB_PARSER_ACTION,
    MULIB_PARSER_INVALID
  } mulib_parser_error_code_t;
  typedef struct mulib_parser_error
  {
    mulib_parser_error_code_t code;
    size_t offset, line, column;
    const char *message; /* Static message; valid after grammar destruction. */
  } mulib_parser_error_t;
  /** Glob-classify *.peg and *.ebnf paths, then read files or compile inline
   * rules. Assignments identify inline rules; extensions/path separators
   * identify paths. Existing extensionless files are also accepted. Missing
   * paths do not fall back to grammar text. NULL on IO, syntax, recursion or
   * allocation errors.
   */
  mulib_parser_t *mulib_parser_from_peg (const char *file_or_grammar);
  mulib_parser_t *mulib_parser_from_ebnf (const char *file_or_grammar);
  mulib_parser_t *mulib_parser_from_peg_ex (const char *file_or_grammar,
                                            mulib_parser_error_t *error);
  mulib_parser_t *mulib_parser_from_ebnf_ex (const char *file_or_grammar,
                                             mulib_parser_error_t *error);
  void mulib_parser_destroy (mulib_parser_t *parser);
  /** Entire-input parse; NULL start selects the first production. Copies
   * source into an owned AST, then invokes actions in postorder. The walker
   * outlives the grammar and source. Actions match production names or
   * explicit -> aliases.
   */
  mulib_ast_walker_t *mulib_parser_parse (
      const mulib_parser_t *parser, const char *start, const char *source,
      size_t length, const mulib_ast_walk_t *actions, size_t action_count,
      const mulib_parsec_options_t *options, mulib_parser_error_t *error);
  /** Load a source file, parse the first production, and return an owned
   * walker. */
  mulib_ast_walker_t *
  mulib_parser_parse_to_walker (const mulib_parser_t *parser,
                                const char *filename,
                                mulib_parser_error_t *error);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_PARSEC_H */

#if defined(MULIB_IMPL_PARSEC) && !defined(MULIB_PARSEC_IMPLEMENTED)
#define MULIB_PARSEC_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif
  static mulib_parsec_parser_t
  mulib_parsec_make_ (mulib_parsec_kind kind)
  {
    mulib_parsec_parser_t p;
    memset (&p, 0, sizeof p);
    p.kind = kind;
    return p;
  }
  mulib_parsec_parser_t
  mulib_parsec_empty (void)
  {
    return mulib_parsec_make_ (MULIB_PARSEC_EMPTY);
  }
  mulib_parsec_parser_t
  mulib_parsec_byte (unsigned char b)
  {
    mulib_parsec_parser_t p = mulib_parsec_make_ (MULIB_PARSEC_BYTE);
    p.as.range.low = p.as.range.high = b;
    return p;
  }
  mulib_parsec_parser_t
  mulib_parsec_range (unsigned char lo, unsigned char hi)
  {
    mulib_parsec_parser_t p = mulib_parsec_make_ (MULIB_PARSEC_RANGE);
    p.as.range.low = lo;
    p.as.range.high = hi;
    return p;
  }
  mulib_parsec_parser_t
  mulib_parsec_set (const unsigned char b[32])
  {
    mulib_parsec_parser_t p = mulib_parsec_make_ (MULIB_PARSEC_SET);
    p.as.text.bytes = b;
    return p;
  }
  mulib_parsec_parser_t
  mulib_parsec_any (void)
  {
    return mulib_parsec_make_ (MULIB_PARSEC_ANY);
  }
  mulib_parsec_parser_t
  mulib_parsec_literal (const char *s, size_t n)
  {
    mulib_parsec_parser_t p = mulib_parsec_make_ (MULIB_PARSEC_LITERAL);
    p.as.text.bytes = (const unsigned char *)s;
    p.as.text.length = n;
    return p;
  }
  mulib_parsec_parser_t
  mulib_parsec_end (void)
  {
    return mulib_parsec_make_ (MULIB_PARSEC_END);
  }
  mulib_parsec_parser_t
  mulib_parsec_sequence (const mulib_parsec_parser_t *a, size_t n)
  {
    mulib_parsec_parser_t p = mulib_parsec_make_ (MULIB_PARSEC_SEQUENCE);
    p.as.list.items = a;
    p.as.list.count = n;
    return p;
  }
  mulib_parsec_parser_t
  mulib_parsec_choice (const mulib_parsec_parser_t *a, size_t n)
  {
    mulib_parsec_parser_t p = mulib_parsec_sequence (a, n);
    p.kind = MULIB_PARSEC_CHOICE;
    return p;
  }
  mulib_parsec_parser_t
  mulib_parsec_repeat (const mulib_parsec_parser_t *a, size_t min, size_t max)
  {
    mulib_parsec_parser_t p = mulib_parsec_make_ (MULIB_PARSEC_REPEAT);
    p.as.unary.item = a;
    p.as.unary.min = min;
    p.as.unary.max = max;
    return p;
  }
  mulib_parsec_parser_t
  mulib_parsec_lookahead (const mulib_parsec_parser_t *a, int positive)
  {
    mulib_parsec_parser_t p
        = mulib_parsec_make_ (positive ? MULIB_PARSEC_AND : MULIB_PARSEC_NOT);
    p.as.unary.item = a;
    return p;
  }
  mulib_parsec_parser_t
  mulib_parsec_capture (const mulib_parsec_parser_t *a, size_t tag)
  {
    mulib_parsec_parser_t p = mulib_parsec_make_ (MULIB_PARSEC_CAPTURE);
    p.as.unary.item = a;
    p.as.unary.tag = tag;
    return p;
  }
  mulib_parsec_parser_t
  mulib_parsec_ref (const mulib_parsec_parser_t *const *a)
  {
    mulib_parsec_parser_t p = mulib_parsec_make_ (MULIB_PARSEC_REF);
    p.as.reference = a;
    return p;
  }
  mulib_parsec_parser_t
  mulib_parsec_satisfy (mulib_parsec_predicate_fn f, void *u,
                        const char *label)
  {
    mulib_parsec_parser_t p = mulib_parsec_make_ (MULIB_PARSEC_PREDICATE);
    p.as.predicate.function = f;
    p.as.predicate.user = u;
    p.label = label;
    return p;
  }
  mulib_parsec_parser_t
  mulib_parsec_named (mulib_parsec_parser_t p, const char *label)
  {
    p.label = label;
    return p;
  }
  typedef struct mulib_parsec_state_
  {
    const unsigned char *source;
    size_t length, pos, depth, steps, capacity;
    mulib_parsec_options_t limits;
    mulib_parsec_result_t *result;
    int fatal, have_error;
  } mulib_parsec_state_;
  static int
  mulib_parsec_fail_ (mulib_parsec_state_ *s, size_t at,
                      mulib_parsec_error_code code, const char *what)
  {
    if (code != MULIB_PARSEC_EXPECTED)
      s->fatal = 1;
    if (!s->have_error || s->fatal || at > s->result->error.offset)
      {
        s->result->error.code = code;
        s->result->error.offset = at;
        s->result->error.expected = what;
        s->have_error = 1;
      }
    return 0;
  }
  static int
  mulib_parsec_exec_ (mulib_parsec_state_ *s, const mulib_parsec_parser_t *p)
  {
    size_t begin = s->pos, first = s->result->count, i;
    int ok = 0;
    const char *label;
    if (!p)
      return mulib_parsec_fail_ (s, begin, MULIB_PARSEC_INVALID,
                                 "null parser");
    if (s->steps >= s->limits.max_steps || s->depth >= s->limits.max_depth)
      return mulib_parsec_fail_ (s, begin, MULIB_PARSEC_LIMIT,
                                 "parser resource limit");
    ++s->steps;
    ++s->depth;
    label = p->label ? p->label : "matching byte";
    switch (p->kind)
      {
      case MULIB_PARSEC_EMPTY:
        ok = 1;
        break;
      case MULIB_PARSEC_BYTE:
      case MULIB_PARSEC_RANGE:
        if (p->as.range.low > p->as.range.high)
          {
            mulib_parsec_fail_ (s, begin, MULIB_PARSEC_INVALID,
                                "invalid byte range");
            break;
          }
        if (begin < s->length && s->source[begin] >= p->as.range.low
            && s->source[begin] <= p->as.range.high)
          {
            ++s->pos;
            ok = 1;
          }
        break;
      case MULIB_PARSEC_SET:
        if (!p->as.text.bytes)
          {
            mulib_parsec_fail_ (s, begin, MULIB_PARSEC_INVALID,
                                "null byte set");
            break;
          }
        if (begin < s->length
            && (p->as.text.bytes[s->source[begin] >> 3]
                & (1u << (s->source[begin] & 7))))
          {
            ++s->pos;
            ok = 1;
          }
        break;
      case MULIB_PARSEC_ANY:
        if (begin < s->length)
          {
            ++s->pos;
            ok = 1;
          }
        break;
      case MULIB_PARSEC_LITERAL:
        label = p->label ? p->label : "literal";
        if (!p->as.text.bytes && p->as.text.length)
          {
            mulib_parsec_fail_ (s, begin, MULIB_PARSEC_INVALID,
                                "null literal");
            break;
          }
        for (i = 0; i < p->as.text.length; ++i)
          {
            if (i == s->length - begin
                || s->source[begin + i] != p->as.text.bytes[i])
              break;
          }
        if (i == p->as.text.length)
          {
            s->pos += i;
            ok = 1;
          }
        else
          mulib_parsec_fail_ (s, begin + i, MULIB_PARSEC_EXPECTED, label);
        break;
      case MULIB_PARSEC_END:
        label = p->label ? p->label : "end of input";
        ok = begin == s->length;
        break;
      case MULIB_PARSEC_SEQUENCE:
      case MULIB_PARSEC_CHOICE:
        if (!p->as.list.items && p->as.list.count)
          {
            mulib_parsec_fail_ (s, begin, MULIB_PARSEC_INVALID,
                                "null parser list");
            break;
          }
        ok = p->kind == MULIB_PARSEC_SEQUENCE;
        for (i = 0; i < p->as.list.count; ++i)
          {
            int child = mulib_parsec_exec_ (s, &p->as.list.items[i]);
            if (p->kind == MULIB_PARSEC_SEQUENCE)
              {
                if (!child)
                  {
                    ok = 0;
                    break;
                  }
              }
            else if (child)
              {
                ok = 1;
                break;
              }
            if (s->fatal)
              break;
          }
        break;
      case MULIB_PARSEC_REPEAT:
        if (!p->as.unary.item || p->as.unary.min > p->as.unary.max)
          {
            mulib_parsec_fail_ (s, begin, MULIB_PARSEC_INVALID,
                                "invalid repetition");
            break;
          }
        ok = 1;
        for (i = 0; i < p->as.unary.max; ++i)
          {
            size_t before = s->pos;
            if (!mulib_parsec_exec_ (s, p->as.unary.item))
              {
                if (s->fatal || i < p->as.unary.min)
                  ok = 0;
                break;
              }
            if (s->pos == before && p->as.unary.max > 1)
              {
                ok = mulib_parsec_fail_ (
                    s, before, MULIB_PARSEC_NO_PROGRESS,
                    "repeated parser accepted empty input");
                break;
              }
          }
        break;
      case MULIB_PARSEC_AND:
      case MULIB_PARSEC_NOT:
        {
          mulib_parsec_error_t old = s->result->error;
          int had = s->have_error;
          ok = mulib_parsec_exec_ (s, p->as.unary.item);
          s->pos = begin;
          s->result->count = first;
          if (!s->fatal)
            {
              if (p->kind == MULIB_PARSEC_NOT)
                {
                  s->result->error = old;
                  s->have_error = had;
                  ok = !ok;
                  label = p->label ? p->label : "negative lookahead";
                }
              else if (ok)
                {
                  s->result->error = old;
                  s->have_error = had;
                }
            }
          else
            ok = 0;
          break;
        }
      case MULIB_PARSEC_CAPTURE:
        ok = mulib_parsec_exec_ (s, p->as.unary.item);
        if (ok)
          {
            mulib_parsec_capture_t *c;
            if (s->result->count >= s->limits.max_captures)
              {
                ok = mulib_parsec_fail_ (s, begin, MULIB_PARSEC_LIMIT,
                                         "capture limit");
                break;
              }
            if (s->result->count == s->capacity)
              {
                size_t cap = s->capacity ? s->capacity : 16;
                if (cap <= SIZE_MAX / 2 && s->capacity)
                  cap *= 2;
                if (cap > s->limits.max_captures)
                  cap = s->limits.max_captures;
                if (cap <= s->capacity || cap > SIZE_MAX / sizeof *c)
                  {
                    ok = mulib_parsec_fail_ (s, begin, MULIB_PARSEC_MEMORY,
                                             "capture allocation overflow");
                    break;
                  }
                c = (mulib_parsec_capture_t *)realloc (s->result->captures,
                                                       cap * sizeof *c);
                if (!c)
                  {
                    ok = mulib_parsec_fail_ (s, begin, MULIB_PARSEC_MEMORY,
                                             "out of memory");
                    break;
                  }
                s->result->captures = c;
                s->capacity = cap;
              }
            c = &s->result->captures[s->result->count++];
            c->tag = p->as.unary.tag;
            c->offset = begin;
            c->length = s->pos - begin;
            c->first = first;
          }
        break;
      case MULIB_PARSEC_REF:
        if (!p->as.reference || !*p->as.reference)
          mulib_parsec_fail_ (s, begin, MULIB_PARSEC_INVALID,
                              "unbound parser reference");
        else
          ok = mulib_parsec_exec_ (s, *p->as.reference);
        break;
      case MULIB_PARSEC_PREDICATE:
        if (!p->as.predicate.function)
          {
            mulib_parsec_fail_ (s, begin, MULIB_PARSEC_INVALID,
                                "null predicate");
            break;
          }
        if (begin < s->length
            && p->as.predicate.function (s->source[begin],
                                         p->as.predicate.user))
          {
            ++s->pos;
            ok = 1;
          }
        break;
      default:
        mulib_parsec_fail_ (s, begin, MULIB_PARSEC_INVALID,
                            "invalid parser kind");
        break;
      }
    --s->depth;
    if (!ok)
      {
        if (!s->fatal)
          {
            mulib_parsec_fail_ (s, begin, MULIB_PARSEC_EXPECTED, label);
            if (p->label && s->result->error.offset == begin)
              s->result->error.expected = p->label;
          }
        s->pos = begin;
        s->result->count = first;
      }
    return ok;
  }
  int
  mulib_parsec_run (const mulib_parsec_parser_t *p, const char *source,
                    size_t n, const mulib_parsec_options_t *options,
                    mulib_parsec_result_t *r)
  {
    mulib_parsec_state_ s;
    size_t i;
    int ok;
    if (!r)
      return -1;
    memset (r, 0, sizeof *r);
    memset (&s, 0, sizeof s);
    s.source = (const unsigned char *)source;
    s.length = n;
    s.result = r;
    s.limits.max_depth
        = options && options->max_depth ? options->max_depth : 256;
    s.limits.max_steps
        = options && options->max_steps ? options->max_steps : 1000000;
    s.limits.max_captures
        = options && options->max_captures ? options->max_captures : 1000000;
    if ((!source && n) || n == SIZE_MAX)
      ok = mulib_parsec_fail_ (&s, 0, MULIB_PARSEC_INVALID, "invalid input");
    else
      ok = mulib_parsec_exec_ (&s, p);
    if (ok)
      {
        r->consumed = s.pos;
        memset (&r->error, 0, sizeof r->error);
        return 1;
      }
    free (r->captures);
    r->captures = NULL;
    r->count = 0;
    r->error.line = r->error.column = 1;
    for (i = 0; i < r->error.offset; ++i)
      {
        if (source[i] == '\n')
          {
            ++r->error.line;
            r->error.column = 1;
          }
        else
          ++r->error.column;
      }
    return s.fatal ? -1 : 0;
  }
  void
  mulib_parsec_result_free (mulib_parsec_result_t *r)
  {
    if (r)
      {
        free (r->captures);
        memset (r, 0, sizeof *r);
      }
  }
#ifdef __cplusplus
}
#endif
#endif /* implementation */

#if defined(MULIB_IMPL_PARSER) && !defined(MULIB_PARSER_IMPLEMENTED)          \
    && (!defined(MULIB_PEG_H) || defined(MULIB_PEG_IMPLEMENTED))
#define MULIB_PARSER_IMPLEMENTED
#include "mulib-glob.h"
#include "mulib-peg.h"
#include "mulib-strbuf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef __cplusplus
extern "C"
{
#endif
  struct mulib_parser
  {
    mulib_peg_t *grammar;
  };
  static void
  mulib_parser_error_ (mulib_parser_error_t *e, mulib_parser_error_code_t code,
                       const char *message)
  {
    if (e)
      {
        memset (e, 0, sizeof *e);
        e->code = code;
        e->message = message;
        if (code)
          e->line = e->column = 1;
      }
  }
  static char *
  mulib_parser_read_ (const char *path, size_t *length,
                      mulib_parser_error_t *e)
  {
    FILE *f;
    struct stat info;
    mulib_strbuf_t b;
    char chunk[4096];
    size_t n;
    if (!path || stat (path, &info) || !S_ISREG (info.st_mode)
        || !(f = fopen (path, "rb")))
      {
        mulib_parser_error_ (
            e, MULIB_PARSER_IO,
            "source file does not exist, is not regular, or cannot be opened");
        return NULL;
      }
    mulib_strbuf_init (&b);
    while ((n = fread (chunk, 1, sizeof chunk, f)) != 0)
      {
        if (!mulib_strbuf_append_n (&b, chunk, n))
          {
            fclose (f);
            mulib_strbuf_free (&b);
            mulib_parser_error_ (e, MULIB_PARSER_MEMORY, "out of memory");
            return NULL;
          }
      }
    {
      int bad = ferror (f);
      if (fclose (f))
        bad = 1;
      if (bad)
        {
          mulib_strbuf_free (&b);
          mulib_parser_error_ (e, MULIB_PARSER_IO, "file read failed");
          return NULL;
        }
    }
    if (!mulib_strbuf_append_n (&b, "", 0))
      {
        mulib_strbuf_free (&b);
        mulib_parser_error_ (e, MULIB_PARSER_MEMORY, "out of memory");
        return NULL;
      }
    *length = b.length;
    return b.data;
  }
  static mulib_parser_t *
  mulib_parser_load_ (const char *input, int ebnf, mulib_parser_error_t *e)
  {
    char *owned = NULL;
    size_t length;
    mulib_parser_t *p;
    mulib_peg_t *g;
    mulib_peg_error_t pe;
    struct stat info;
    int path;
    mulib_parser_error_ (e, MULIB_PARSER_OK, NULL);
    if (!input)
      {
        mulib_parser_error_ (e, MULIB_PARSER_INVALID, "null grammar");
        return NULL;
      }
    path = mulib_glob_match ("*.peg", input)
           || mulib_glob_match ("*.ebnf", input)
           || mulib_glob_match ("*.*", input)
           || mulib_glob_match ("*/*", input)
           || mulib_glob_match ("*\\*", input);
    /* An assignment identifies inline text even if its last literal resembles
     * a filename. Glob matching is classification, not filesystem expansion.
     */
    if (strstr (input, "<-") || strchr (input, '='))
      path = 0;
    else if (!path)
      path = stat (input, &info) == 0;
    length = strlen (input);
    if (path)
      {
        owned = mulib_parser_read_ (input, &length, e);
        if (!owned)
          return NULL;
        input = owned;
      }
    g = ebnf ? mulib_peg_compile_ebnf (input, length, &pe)
             : mulib_peg_compile (input, length, &pe);
    free (owned);
    if (!g)
      {
        mulib_parser_error_ (e,
                             pe.message && strstr (pe.message, "memory")
                                 ? MULIB_PARSER_MEMORY
                                 : MULIB_PARSER_SYNTAX,
                             pe.message);
        if (e)
          {
            e->offset = pe.offset;
            e->line = pe.line;
            e->column = pe.column;
          }
        return NULL;
      }
    p = (mulib_parser_t *)malloc (sizeof *p);
    if (!p)
      {
        mulib_peg_destroy (g);
        mulib_parser_error_ (e, MULIB_PARSER_MEMORY, "out of memory");
        return NULL;
      }
    p->grammar = g;
    return p;
  }
  mulib_parser_t *
  mulib_parser_from_peg (const char *s)
  {
    return mulib_parser_load_ (s, 0, NULL);
  }
  mulib_parser_t *
  mulib_parser_from_ebnf (const char *s)
  {
    return mulib_parser_load_ (s, 1, NULL);
  }
  mulib_parser_t *
  mulib_parser_from_peg_ex (const char *s, mulib_parser_error_t *e)
  {
    return mulib_parser_load_ (s, 0, e);
  }
  mulib_parser_t *
  mulib_parser_from_ebnf_ex (const char *s, mulib_parser_error_t *e)
  {
    return mulib_parser_load_ (s, 1, e);
  }
  void
  mulib_parser_destroy (mulib_parser_t *p)
  {
    if (p)
      {
        mulib_peg_destroy (p->grammar);
        free (p);
      }
  }
  mulib_ast_walker_t *
  mulib_parser_parse (const mulib_parser_t *p, const char *start,
                      const char *s, size_t len,
                      const mulib_ast_walk_t *actions, size_t count,
                      const mulib_parsec_options_t *options,
                      mulib_parser_error_t *e)
  {
    mulib_parsec_result_t r;
    mulib_ast_t **nodes;
    mulib_ast_walker_t *w;
    size_t i, j;
    int status;
    mulib_parser_error_ (e, MULIB_PARSER_OK, NULL);
    if (!p || (!s && len) || (!actions && count))
      {
        mulib_parser_error_ (e, MULIB_PARSER_INVALID,
                             "invalid parse arguments");
        return NULL;
      }
    status = mulib_peg_run (p->grammar, start, s, len, 1, options, &r);
    if (status != 1)
      {
        mulib_parser_error_ (
            e,
            r.error.code == MULIB_PARSEC_MEMORY ? MULIB_PARSER_MEMORY
            : status == 0                       ? MULIB_PARSER_MATCH
                                                : MULIB_PARSER_INVALID,
            r.error.code == MULIB_PARSEC_MEMORY ? "out of memory"
            : status == 0 ? "input does not match grammar"
                          : "parser resource limit or invalid arguments");
        if (e)
          {
            e->offset = r.error.offset;
            e->line = r.error.line;
            e->column = r.error.column;
          }
        mulib_parsec_result_free (&r);
        return NULL;
      }
    w = (mulib_ast_walker_t *)calloc (1, sizeof *w);
    nodes = r.count <= SIZE_MAX / sizeof *nodes
                ? (mulib_ast_t **)calloc (r.count, sizeof *nodes)
                : NULL;
    if (!w || !nodes)
      {
        free (w);
        free (nodes);
        mulib_parsec_result_free (&r);
        mulib_parser_error_ (e, MULIB_PARSER_MEMORY, "out of memory");
        return NULL;
      }
    for (i = 0; i < r.count; ++i)
      {
        mulib_parsec_capture_t *c = &r.captures[i];
        nodes[i] = mulib_ast_create (mulib_peg_rule_name (p->grammar, c->tag),
                                     s ? s + c->offset : "", c->length);
        if (!nodes[i])
          goto memory;
        nodes[i]->offset = c->offset;
        if (!mulib_ast_set_action (nodes[i],
                                   mulib_peg_rule_action (p->grammar, c->tag)))
          goto memory;
        /* Descendants are postorder. Unattached roots in this interval are
         * exactly the immediate children, already in source order. */
        for (j = c->first; j < i; ++j)
          if (!nodes[j]->parent)
            if (!mulib_ast_append (nodes[i], nodes[j]))
              goto memory;
      }
    w->root = nodes[r.count - 1];
    free (nodes);
    mulib_parsec_result_free (&r);
    if (count && mulib_ast_walker_walk (w, actions, count) != 1)
      {
        mulib_ast_walker_destroy (w);
        mulib_parser_error_ (e, MULIB_PARSER_ACTION,
                             "semantic action stopped traversal");
        return NULL;
      }
    return w;
  memory:
    /* Destroy disjoint roots only. Descendants may have lower array indices.
     */
    for (i = 0; i < r.count; ++i)
      if (nodes[i] && nodes[i]->parent)
        nodes[i] = NULL;
    for (i = 0; i < r.count; ++i)
      mulib_ast_destroy (nodes[i]);
    free (nodes);
    free (w);
    mulib_parsec_result_free (&r);
    mulib_parser_error_ (e, MULIB_PARSER_MEMORY,
                         "AST allocation or depth limit");
    return NULL;
  }
  mulib_ast_walker_t *
  mulib_parser_parse_to_walker (const mulib_parser_t *p, const char *path,
                                mulib_parser_error_t *e)
  {
    size_t length;
    char *s = mulib_parser_read_ (path, &length, e);
    mulib_ast_walker_t *w;
    if (!s)
      return NULL;
    w = mulib_parser_parse (p, NULL, s, length, NULL, 0, NULL, e);
    free (s);
    return w;
  }
#ifdef __cplusplus
}
#endif
#endif

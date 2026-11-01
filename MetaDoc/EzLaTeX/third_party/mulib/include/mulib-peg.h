/**
 * @file mulib-peg.h
 * @brief Compile byte-oriented PEG grammars into real parsec combinators.
 * Define MULIB_IMPL_PEG; also implement PARSEC and STRBUF explicitly.
 */
#ifndef MULIB_PEG_H
#define MULIB_PEG_H
#include "mulib-parsec.h"
#include <stddef.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_peg mulib_peg_t;
  typedef struct mulib_peg_error
  {
    size_t offset, line, column;
    const char *message;
  } mulib_peg_error_t;
  /** @brief Compile rules `name <- expression;`; returns an owned grammar or
   * NULL. Supports /, sequence, & !, ? * +, grouping, literals, byte classes,
   * dot, forward references and # comments. Rejects left recursion and
   * nullable loops.
   */
  mulib_peg_t *mulib_peg_compile (const char *source, size_t length,
                                  mulib_peg_error_t *error);
  /** EBNF dialect: =, |, comma concatenation, [optional], {repeat}; uppercase
   * rules are lexemes, with explicit whitespace and no implicit tokenization.
   * Both dialects accept -> action before a rule's semicolon and @(...)
   * lexical S-expressions. EBNF also accepts ? (...) ? special sequences.
   */
  mulib_peg_t *mulib_peg_compile_ebnf (const char *source, size_t length,
                                       mulib_peg_error_t *error);
  /** @brief Destroy a compiled grammar. NULL is harmless. */
  void mulib_peg_destroy (mulib_peg_t *grammar);
  /** @brief Return the number of named rules. */
  size_t mulib_peg_rule_count (const mulib_peg_t *grammar);
  /** @brief Borrow a rule name; its zero-based index is its parsec capture
   * tag. */
  const char *mulib_peg_rule_name (const mulib_peg_t *grammar, size_t index);
  const char *mulib_peg_rule_action (const mulib_peg_t *grammar, size_t index);
  /** @brief Match using parsec: 1 match, 0 mismatch, -1 resource/argument
   * error. NULL start selects the first rule. entire requires end of input.
   * Captures include every matched rule in postorder; free result with
   * parsec_result_free. Compiled grammars are immutable and can be used
   * concurrently.
   */
  int mulib_peg_run (const mulib_peg_t *grammar, const char *start,
                     const char *source, size_t length, int entire,
                     const mulib_parsec_options_t *options,
                     mulib_parsec_result_t *result);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_PEG_H */

#if defined(MULIB_IMPL_PEG) && !defined(MULIB_PEG_IMPLEMENTED)
#define MULIB_PEG_IMPLEMENTED
#include "mulib-strbuf.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifndef MULIB_PEG_MAX_DEPTH
#define MULIB_PEG_MAX_DEPTH 128
#endif
#ifndef MULIB_PEG_MAX_NODES
#define MULIB_PEG_MAX_NODES 100000
#endif
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_peg_node_ mulib_peg_node_;
  struct mulib_peg_node_
  {
    mulib_parsec_parser_t parser;
    const mulib_parsec_parser_t *entry;
    mulib_peg_node_ *next;
    mulib_peg_node_ **children;
    mulib_parsec_parser_t *parts;
    size_t count, offset, reference;
    char *text;
    int nullable;
  };
  typedef struct mulib_peg_rule_
  {
    char *name, *action;
    const mulib_parsec_parser_t *entry;
    mulib_peg_node_ *body;
    unsigned mark;
  } mulib_peg_rule_;
  struct mulib_peg
  {
    mulib_peg_node_ *nodes;
    size_t node_count, count, capacity;
    mulib_peg_rule_ *rules;
  };
  typedef struct mulib_peg_reader_
  {
    const char *source;
    size_t length, pos, depth;
    mulib_peg_t *grammar;
    mulib_peg_error_t *error;
    int failed, ebnf;
  } mulib_peg_reader_;
  static void
  mulib_peg_fail_ (mulib_peg_reader_ *r, size_t at, const char *message)
  {
    size_t i;
    if (r->failed)
      return;
    r->failed = 1;
    if (!r->error)
      return;
    r->error->offset = at;
    r->error->line = r->error->column = 1;
    r->error->message = message;
    for (i = 0; i < at; ++i)
      {
        if (r->source[i] == '\n')
          {
            ++r->error->line;
            r->error->column = 1;
          }
        else
          ++r->error->column;
      }
  }
  static void
  mulib_peg_space_ (mulib_peg_reader_ *r)
  {
    while (r->pos < r->length)
      {
        unsigned char c = (unsigned char)r->source[r->pos];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
          ++r->pos;
        else if (c == '#')
          {
            while (r->pos < r->length && r->source[r->pos] != '\n')
              ++r->pos;
          }
        else if (r->ebnf && c == '(' && r->pos + 1 < r->length
                 && r->source[r->pos + 1] == '*')
          {
            size_t start = r->pos;
            r->pos += 2;
            while (
                r->pos + 1 < r->length
                && !(r->source[r->pos] == '*' && r->source[r->pos + 1] == ')'))
              ++r->pos;
            if (r->pos + 1 >= r->length)
              {
                mulib_peg_fail_ (r, start, "unterminated EBNF comment");
                r->pos = r->length;
                return;
              }
            r->pos += 2;
          }
        else
          break;
      }
  }
  static int
  mulib_peg_ident_first_ (unsigned char c)
  {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
  }
  static int
  mulib_peg_ident_more_ (unsigned char c)
  {
    return mulib_peg_ident_first_ (c) || (c >= '0' && c <= '9');
  }
  static char *
  mulib_peg_name_ (mulib_peg_reader_ *r)
  {
    size_t begin, n;
    char *name;
    mulib_peg_space_ (r);
    begin = r->pos;
    if (begin == r->length
        || !mulib_peg_ident_first_ ((unsigned char)r->source[begin]))
      {
        mulib_peg_fail_ (r, begin, "expected rule name");
        return NULL;
      }
    while (r->pos < r->length
           && mulib_peg_ident_more_ ((unsigned char)r->source[r->pos]))
      ++r->pos;
    n = r->pos - begin;
    name = (char *)malloc (n + 1);
    if (!name)
      {
        mulib_peg_fail_ (r, begin, "out of memory");
        return NULL;
      }
    memcpy (name, r->source + begin, n);
    name[n] = '\0';
    return name;
  }
  static mulib_peg_node_ *
  mulib_peg_node_new_ (mulib_peg_reader_ *r)
  {
    mulib_peg_node_ *n;
    if (r->grammar->node_count >= (size_t)MULIB_PEG_MAX_NODES)
      {
        mulib_peg_fail_ (r, r->pos, "grammar node limit");
        return NULL;
      }
    n = (mulib_peg_node_ *)calloc (1, sizeof *n);
    if (!n)
      {
        mulib_peg_fail_ (r, r->pos, "out of memory");
        return NULL;
      }
    n->offset = r->pos;
    n->entry = &n->parser;
    n->reference = SIZE_MAX;
    n->next = r->grammar->nodes;
    r->grammar->nodes = n;
    ++r->grammar->node_count;
    return n;
  }
  static mulib_peg_node_ *
  mulib_peg_compose_ (mulib_peg_reader_ *r, mulib_parsec_kind kind,
                      mulib_peg_node_ *const *items, size_t count)
  {
    mulib_peg_node_ *n = mulib_peg_node_new_ (r);
    size_t i;
    if (!n)
      return NULL;
    if (count)
      {
        if (count > SIZE_MAX / sizeof *n->parts
            || count > SIZE_MAX / sizeof *n->children)
          {
            mulib_peg_fail_ (r, r->pos, "grammar allocation overflow");
            return NULL;
          }
        n->children = (mulib_peg_node_ **)malloc (count * sizeof *n->children);
        n->parts = (mulib_parsec_parser_t *)malloc (count * sizeof *n->parts);
        if (!n->children || !n->parts)
          {
            mulib_peg_fail_ (r, r->pos, "out of memory");
            return NULL;
          }
        for (i = 0; i < count; ++i)
          {
            n->children[i] = items[i];
            n->parts[i] = mulib_parsec_ref (&items[i]->entry);
          }
      }
    n->count = count;
    if (kind == MULIB_PARSEC_SEQUENCE)
      n->parser = mulib_parsec_sequence (n->parts, count);
    else if (kind == MULIB_PARSEC_CHOICE)
      n->parser = mulib_parsec_choice (n->parts, count);
    else if (kind == MULIB_PARSEC_REPEAT)
      n->parser = mulib_parsec_repeat (n->parts, 0, SIZE_MAX);
    else
      n->parser = mulib_parsec_lookahead (n->parts, kind == MULIB_PARSEC_AND);
    return n;
  }
  static int
  mulib_peg_hex_ (unsigned char c)
  {
    if (c >= '0' && c <= '9')
      return c - '0';
    if (c >= 'a' && c <= 'f')
      return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
      return c - 'A' + 10;
    return -1;
  }
  static int
  mulib_peg_char_ (mulib_peg_reader_ *r, unsigned char *out)
  {
    unsigned char c;
    if (r->pos == r->length)
      {
        mulib_peg_fail_ (r, r->pos, "unterminated literal or class");
        return 0;
      }
    c = (unsigned char)r->source[r->pos++];
    if (c == '\\')
      {
        if (r->pos == r->length)
          {
            mulib_peg_fail_ (r, r->pos, "truncated escape");
            return 0;
          }
        c = (unsigned char)r->source[r->pos++];
        switch (c)
          {
          case 'n':
            c = '\n';
            break;
          case 'r':
            c = '\r';
            break;
          case 't':
            c = '\t';
            break;
          case 'b':
            c = '\b';
            break;
          case 'f':
            c = '\f';
            break;
          case 'v':
            c = '\v';
            break;
          case '0':
            c = 0;
            break;
          case 'x':
            {
              int a, b;
              if (r->length - r->pos < 2
                  || (a = mulib_peg_hex_ ((unsigned char)r->source[r->pos]))
                         < 0
                  || (b
                      = mulib_peg_hex_ ((unsigned char)r->source[r->pos + 1]))
                         < 0)
                {
                  mulib_peg_fail_ (r, r->pos,
                                   "expected two hexadecimal digits");
                  return 0;
                }
              c = (unsigned char)(a * 16 + b);
              r->pos += 2;
              break;
            }
          case '\\':
          case '\'':
          case '"':
          case '[':
          case ']':
          case '-':
          case '^':
            break;
          default:
            mulib_peg_fail_ (r, r->pos - 1, "unsupported escape");
            return 0;
          }
      }
    *out = c;
    return 1;
  }
  static mulib_peg_node_ *mulib_peg_expression_ (mulib_peg_reader_ *r);
  static mulib_peg_node_ *mulib_peg_primary_ (mulib_peg_reader_ *r);
  /* Deliberately small scannerless lexical language, compiled to combinators.
   */
  static mulib_peg_node_ *
  mulib_peg_lex_ (mulib_peg_reader_ *r)
  {
    char *op = NULL;
    mulib_peg_node_ **items = NULL, *n = NULL;
    size_t count = 0, capacity = 0;
    mulib_parsec_kind kind;
    mulib_peg_space_ (r);
    if (r->depth >= (size_t)MULIB_PEG_MAX_DEPTH)
      {
        mulib_peg_fail_ (r, r->pos, "lexical nesting limit");
        return NULL;
      }
    if (r->pos == r->length || r->source[r->pos] != '(')
      {
        ++r->depth;
        n = mulib_peg_primary_ (r);
        --r->depth;
        return n;
      }
    ++r->pos;
    ++r->depth;
    op = mulib_peg_name_ (r);
    if (!op)
      goto done;
    for (;;)
      {
        mulib_peg_node_ **p;
        mulib_peg_space_ (r);
        if (r->pos == r->length)
          {
            mulib_peg_fail_ (r, r->pos, "unterminated lexical form");
            goto done;
          }
        if (r->source[r->pos] == ')')
          {
            ++r->pos;
            break;
          }
        n = mulib_peg_lex_ (r);
        if (!n)
          goto done;
        if (count == capacity)
          {
            size_t cap = capacity ? capacity * 2 : 4;
            if (cap < capacity || cap > SIZE_MAX / sizeof *p)
              {
                mulib_peg_fail_ (r, r->pos, "lexical allocation overflow");
                goto done;
              }
            p = (mulib_peg_node_ **)realloc (items, cap * sizeof *p);
            if (!p)
              {
                mulib_peg_fail_ (r, r->pos, "out of memory");
                goto done;
              }
            items = p;
            capacity = cap;
          }
        items[count++] = n;
      }
    n = NULL;
    if (!strcmp (op, "seq") || !strcmp (op, "or"))
      {
        kind = !strcmp (op, "seq") ? MULIB_PARSEC_SEQUENCE
                                   : MULIB_PARSEC_CHOICE;
        n = mulib_peg_compose_ (r, kind, items, count);
      }
    else if ((!strcmp (op, "any") || !strcmp (op, "eof")) && !count)
      {
        n = mulib_peg_node_new_ (r);
        if (n)
          n->parser = !strcmp (op, "any") ? mulib_parsec_any ()
                                          : mulib_parsec_end ();
      }
    else if (!strcmp (op, "range") && count == 2
             && items[0]->parser.kind == MULIB_PARSEC_LITERAL
             && items[0]->parser.as.text.length == 1
             && items[1]->parser.kind == MULIB_PARSEC_LITERAL
             && items[1]->parser.as.text.length == 1
             && items[0]->parser.as.text.bytes[0]
                    <= items[1]->parser.as.text.bytes[0])
      {
        n = mulib_peg_node_new_ (r);
        if (n)
          n->parser = mulib_parsec_range (items[0]->parser.as.text.bytes[0],
                                          items[1]->parser.as.text.bytes[0]);
      }
    else if (count == 1)
      {
        if (!strcmp (op, "lit")
            && items[0]->parser.kind == MULIB_PARSEC_LITERAL)
          n = items[0];
        else if (!strcmp (op, "set")
                 && items[0]->parser.kind == MULIB_PARSEC_LITERAL)
          {
            size_t i;
            n = mulib_peg_node_new_ (r);
            if (n)
              {
                n->text = (char *)calloc (32, 1);
                if (!n->text)
                  {
                    mulib_peg_fail_ (r, r->pos, "out of memory");
                    n = NULL;
                  }
                else
                  {
                    for (i = 0; i < items[0]->parser.as.text.length; ++i)
                      {
                        unsigned char byte = items[0]->parser.as.text.bytes[i];
                        ((unsigned char *)n->text)[byte >> 3]
                            |= (unsigned char)(1u << (byte & 7));
                      }
                    n->parser
                        = mulib_parsec_set ((const unsigned char *)n->text);
                  }
              }
          }
        else if (!strcmp (op, "star") || !strcmp (op, "plus")
                 || !strcmp (op, "opt"))
          {
            n = mulib_peg_compose_ (r, MULIB_PARSEC_REPEAT, items, 1);
            if (n)
              {
                n->parser.as.unary.min = !strcmp (op, "plus") ? 1 : 0;
                n->parser.as.unary.max = !strcmp (op, "opt") ? 1 : SIZE_MAX;
              }
          }
        else if (!strcmp (op, "not") || !strcmp (op, "and"))
          n = mulib_peg_compose_ (
              r, !strcmp (op, "not") ? MULIB_PARSEC_NOT : MULIB_PARSEC_AND,
              items, 1);
      }
    if (!n && !r->failed)
      mulib_peg_fail_ (r, r->pos,
                       "unknown lexical operator or invalid operands");
  done:
    free (op);
    free (items);
    --r->depth;
    return r->failed ? NULL : n;
  }
  static mulib_peg_node_ *
  mulib_peg_primary_ (mulib_peg_reader_ *r)
  {
    unsigned char c;
    mulib_peg_node_ *n;
    mulib_peg_space_ (r);
    if (r->pos == r->length)
      {
        mulib_peg_fail_ (r, r->pos, "expected expression");
        return NULL;
      }
    c = (unsigned char)r->source[r->pos];
    if (c == '@' || (r->ebnf && c == '?'))
      {
        int special = c == '?';
        ++r->pos;
        n = mulib_peg_lex_ (r);
        mulib_peg_space_ (r);
        if (special)
          {
            if (r->pos == r->length || r->source[r->pos] != '?')
              {
                mulib_peg_fail_ (r, r->pos,
                                 "expected closing special-sequence ?");
                return NULL;
              }
            ++r->pos;
          }
        return n;
      }
    if (r->ebnf && (c == '[' || c == '{'))
      {
        char close = c == '[' ? ']' : '}';
        ++r->pos;
        n = mulib_peg_expression_ (r);
        mulib_peg_space_ (r);
        if (!n)
          return NULL;
        if (r->pos == r->length || r->source[r->pos] != close)
          {
            mulib_peg_fail_ (r, r->pos, "expected EBNF closing delimiter");
            return NULL;
          }
        ++r->pos;
        n = mulib_peg_compose_ (r, MULIB_PARSEC_REPEAT, &n, 1);
        if (n && c == '[')
          n->parser.as.unary.max = 1;
        return n;
      }
    if (c == '(')
      {
        ++r->pos;
        n = mulib_peg_expression_ (r);
        mulib_peg_space_ (r);
        if (!n)
          return NULL;
        if (r->pos == r->length || r->source[r->pos] != ')')
          {
            mulib_peg_fail_ (r, r->pos, "expected closing parenthesis");
            return NULL;
          }
        ++r->pos;
        return n;
      }
    n = mulib_peg_node_new_ (r);
    if (!n)
      return NULL;
    if (c == '\'' || c == '"')
      {
        mulib_strbuf_t b;
        ++r->pos;
        mulib_strbuf_init (&b);
        while (r->pos < r->length && (unsigned char)r->source[r->pos] != c)
          {
            unsigned char byte;
            if (!mulib_peg_char_ (r, &byte))
              {
                mulib_strbuf_free (&b);
                return NULL;
              }
            if (!mulib_strbuf_putc (&b, (char)byte))
              {
                mulib_peg_fail_ (r, r->pos, "out of memory");
                mulib_strbuf_free (&b);
                return NULL;
              }
          }
        if (r->pos == r->length)
          {
            mulib_peg_fail_ (r, r->pos, "unterminated literal");
            mulib_strbuf_free (&b);
            return NULL;
          }
        ++r->pos;
        n->text = b.data;
        n->parser = mulib_parsec_literal (b.data, b.length);
        return n;
      }
    if (c == '[')
      {
        unsigned char bits[32] = { 0 };
        int negate = 0, any = 0;
        ++r->pos;
        if (r->pos < r->length && r->source[r->pos] == '^')
          {
            negate = 1;
            ++r->pos;
          }
        while (r->pos < r->length && r->source[r->pos] != ']')
          {
            unsigned char lo, hi;
            unsigned k;
            if (!mulib_peg_char_ (r, &lo))
              return NULL;
            hi = lo;
            if (r->length - r->pos >= 2 && r->source[r->pos] == '-'
                && r->source[r->pos + 1] != ']')
              {
                ++r->pos;
                if (!mulib_peg_char_ (r, &hi))
                  return NULL;
                if (lo > hi)
                  {
                    mulib_peg_fail_ (r, r->pos, "reversed character range");
                    return NULL;
                  }
              }
            for (k = lo; k <= (unsigned)hi; ++k)
              bits[k >> 3] |= (unsigned char)(1u << (k & 7));
            any = 1;
          }
        if (r->pos == r->length || !any)
          {
            mulib_peg_fail_ (r, r->pos, "unterminated or empty byte class");
            return NULL;
          }
        ++r->pos;
        if (negate)
          {
            size_t k;
            for (k = 0; k < sizeof bits; ++k)
              bits[k] = (unsigned char)~bits[k];
          }
        n->text = (char *)malloc (32);
        if (!n->text)
          {
            mulib_peg_fail_ (r, r->pos, "out of memory");
            return NULL;
          }
        memcpy (n->text, bits, sizeof bits);
        n->parser = mulib_parsec_set ((const unsigned char *)n->text);
        return n;
      }
    if (c == '.')
      {
        ++r->pos;
        n->parser = mulib_parsec_any ();
        return n;
      }
    if (mulib_peg_ident_first_ (c))
      {
        n->text = mulib_peg_name_ (r);
        if (!n->text)
          return NULL;
        n->parser = mulib_parsec_ref (NULL);
        return n;
      }
    mulib_peg_fail_ (r, r->pos,
                     "expected literal, class, group or rule reference");
    return NULL;
  }
  static mulib_peg_node_ *
  mulib_peg_prefix_ (mulib_peg_reader_ *r)
  {
    mulib_peg_node_ *n, *wrapped;
    char prefix = 0, suffix = 0;
    mulib_peg_space_ (r);
    if (r->depth >= (size_t)MULIB_PEG_MAX_DEPTH)
      {
        mulib_peg_fail_ (r, r->pos, "grammar nesting limit");
        return NULL;
      }
    ++r->depth;
    if (r->pos < r->length
        && (r->source[r->pos] == '&' || r->source[r->pos] == '!'))
      {
        prefix = r->source[r->pos++];
        n = mulib_peg_prefix_ (r);
        if (n)
          n = mulib_peg_compose_ (
              r, prefix == '&' ? MULIB_PARSEC_AND : MULIB_PARSEC_NOT, &n, 1);
        --r->depth;
        return n;
      }
    n = mulib_peg_primary_ (r);
    --r->depth;
    if (!n)
      return NULL;
    mulib_peg_space_ (r);
    if (r->pos < r->length
        && (r->source[r->pos] == '?' || r->source[r->pos] == '*'
            || r->source[r->pos] == '+'))
      suffix = r->source[r->pos++];
    if (!suffix)
      return n;
    wrapped = mulib_peg_compose_ (r, MULIB_PARSEC_REPEAT, &n, 1);
    if (!wrapped)
      return NULL;
    wrapped->parser.as.unary.min = suffix == '+' ? 1 : 0;
    wrapped->parser.as.unary.max = suffix == '?' ? 1 : SIZE_MAX;
    return wrapped;
  }
  static int
  mulib_peg_push_ (mulib_peg_reader_ *r, mulib_peg_node_ ***items,
                   size_t *count, size_t *capacity, mulib_peg_node_ *n)
  {
    if (*count == *capacity)
      {
        size_t cap = *capacity ? *capacity : 8;
        mulib_peg_node_ **p;
        if (*capacity)
          {
            if (cap > SIZE_MAX / 2)
              {
                mulib_peg_fail_ (r, r->pos, "grammar allocation overflow");
                return 0;
              }
            cap *= 2;
          }
        if (cap > SIZE_MAX / sizeof *p)
          {
            mulib_peg_fail_ (r, r->pos, "grammar allocation overflow");
            return 0;
          }
        p = (mulib_peg_node_ **)realloc (*items, cap * sizeof *p);
        if (!p)
          {
            mulib_peg_fail_ (r, r->pos, "out of memory");
            return 0;
          }
        *items = p;
        *capacity = cap;
      }
    (*items)[(*count)++] = n;
    return 1;
  }
  static mulib_peg_node_ *
  mulib_peg_sequence_ (mulib_peg_reader_ *r)
  {
    mulib_peg_node_ **items = NULL, *n = NULL;
    size_t count = 0, capacity = 0;
    for (;;)
      {
        char c;
        mulib_peg_space_ (r);
        if (r->pos == r->length)
          break;
        c = r->source[r->pos];
        if (c == '/' || c == ')' || c == ';'
            || (c == '-' && r->pos + 1 < r->length
                && r->source[r->pos + 1] == '>')
            || (r->ebnf && (c == '|' || c == ']' || c == '}')))
          break;
        if (r->ebnf && c == ',')
          {
            if (!count)
              {
                mulib_peg_fail_ (r, r->pos, "unexpected comma");
                free (items);
                return NULL;
              }
            ++r->pos;
            n = mulib_peg_prefix_ (r);
            if (!n || !mulib_peg_push_ (r, &items, &count, &capacity, n))
              {
                free (items);
                return NULL;
              }
            continue;
          }
        n = mulib_peg_prefix_ (r);
        if (!n || !mulib_peg_push_ (r, &items, &count, &capacity, n))
          {
            free (items);
            return NULL;
          }
      }
    n = count == 1
            ? items[0]
            : mulib_peg_compose_ (r, MULIB_PARSEC_SEQUENCE, items, count);
    free (items);
    return n;
  }
  static mulib_peg_node_ *
  mulib_peg_expression_ (mulib_peg_reader_ *r)
  {
    mulib_peg_node_ **items = NULL, *n;
    size_t count = 0, capacity = 0;
    do
      {
        n = mulib_peg_sequence_ (r);
        if (!n || !mulib_peg_push_ (r, &items, &count, &capacity, n))
          {
            free (items);
            return NULL;
          }
        mulib_peg_space_ (r);
        if (r->pos == r->length
            || (r->source[r->pos] != '/'
                && (!r->ebnf || r->source[r->pos] != '|')))
          break;
        ++r->pos;
      }
    while (!r->failed);
    n = count == 1 ? items[0]
                   : mulib_peg_compose_ (r, MULIB_PARSEC_CHOICE, items, count);
    free (items);
    return n;
  }
  void
  mulib_peg_destroy (mulib_peg_t *g)
  {
    size_t i;
    if (!g)
      return;
    while (g->nodes)
      {
        mulib_peg_node_ *n = g->nodes;
        g->nodes = n->next;
        free (n->children);
        free (n->parts);
        free (n->text);
        free (n);
      }
    for (i = 0; i < g->count; ++i)
      {
        free (g->rules[i].name);
        free (g->rules[i].action);
      }
    free (g->rules);
    free (g);
  }
  static int
  mulib_peg_nullable_ (const mulib_peg_t *g, const mulib_peg_node_ *n)
  {
    size_t i;
    switch (n->parser.kind)
      {
      case MULIB_PARSEC_LITERAL:
        return n->parser.as.text.length == 0;
      case MULIB_PARSEC_END:
        return 1;
      case MULIB_PARSEC_AND:
      case MULIB_PARSEC_NOT:
        return 1;
      case MULIB_PARSEC_REPEAT:
        return !n->parser.as.unary.min || n->children[0]->nullable;
      case MULIB_PARSEC_REF:
        return g->rules[n->reference].body->nullable;
      case MULIB_PARSEC_SEQUENCE:
        for (i = 0; i < n->count; ++i)
          if (!n->children[i]->nullable)
            return 0;
        return 1;
      case MULIB_PARSEC_CHOICE:
        for (i = 0; i < n->count; ++i)
          if (n->children[i]->nullable)
            return 1;
        return 0;
      case MULIB_PARSEC_CAPTURE:
        return n->children[0]->nullable;
      default:
        return 0;
      }
  }
  static int mulib_peg_visit_rule_ (mulib_peg_reader_ *r, size_t index,
                                    size_t depth);
  static int
  mulib_peg_visit_node_ (mulib_peg_reader_ *r, const mulib_peg_node_ *n,
                         size_t depth)
  {
    size_t i;
    if (depth > (size_t)MULIB_PEG_MAX_DEPTH * 4u)
      {
        mulib_peg_fail_ (r, n->offset, "grammar reference nesting limit");
        return 0;
      }
    switch (n->parser.kind)
      {
      case MULIB_PARSEC_REF:
        return mulib_peg_visit_rule_ (r, n->reference, depth + 1);
      case MULIB_PARSEC_SEQUENCE:
      case MULIB_PARSEC_CHOICE:
        for (i = 0; i < n->count; ++i)
          {
            if (!mulib_peg_visit_node_ (r, n->children[i], depth + 1))
              return 0;
            if (n->parser.kind == MULIB_PARSEC_SEQUENCE
                && !n->children[i]->nullable)
              break;
          }
        break;
      case MULIB_PARSEC_REPEAT:
      case MULIB_PARSEC_AND:
      case MULIB_PARSEC_NOT:
      case MULIB_PARSEC_CAPTURE:
        return mulib_peg_visit_node_ (r, n->children[0], depth + 1);
      default:
        break;
      }
    return 1;
  }
  static int
  mulib_peg_visit_rule_ (mulib_peg_reader_ *r, size_t index, size_t depth)
  {
    mulib_peg_rule_ *rule = &r->grammar->rules[index];
    if (rule->mark == 1)
      {
        mulib_peg_fail_ (r, rule->body->offset, "left-recursive grammar");
        return 0;
      }
    if (rule->mark == 2)
      return 1;
    rule->mark = 1;
    if (!mulib_peg_visit_node_ (r, rule->body, depth + 1))
      return 0;
    rule->mark = 2;
    return 1;
  }
  static mulib_peg_t *
  mulib_peg_compile_ (const char *source, size_t length,
                      mulib_peg_error_t *error, int ebnf)
  {
    mulib_peg_reader_ r;
    mulib_peg_t *g;
    mulib_peg_node_ *n;
    size_t i;
    int changed;
    if (error)
      memset (error, 0, sizeof *error);
    memset (&r, 0, sizeof r);
    r.source = source;
    r.length = length;
    r.error = error;
    r.ebnf = ebnf;
    if ((!source && length) || length == SIZE_MAX)
      {
        mulib_peg_fail_ (&r, 0, "invalid grammar input");
        return NULL;
      }
    g = (mulib_peg_t *)calloc (1, sizeof *g);
    if (!g)
      {
        mulib_peg_fail_ (&r, 0, "out of memory");
        return NULL;
      }
    r.grammar = g;
    for (;;)
      {
        char *name, *action = NULL;
        mulib_peg_node_ *body, *capture;
        mulib_peg_space_ (&r);
        if (r.pos == length)
          break;
        name = mulib_peg_name_ (&r);
        if (!name)
          goto fail;
        for (i = 0; i < g->count; ++i)
          if (!strcmp (name, g->rules[i].name))
            break;
        if (i < g->count)
          {
            free (name);
            mulib_peg_fail_ (&r, r.pos, "duplicate rule name");
            goto fail;
          }
        mulib_peg_space_ (&r);
        if (ebnf)
          {
            if (r.pos == length || source[r.pos] != '=')
              {
                free (name);
                mulib_peg_fail_ (&r, r.pos, "expected =");
                goto fail;
              }
            ++r.pos;
          }
        else
          {
            if (length - r.pos < 2 || source[r.pos] != '<'
                || source[r.pos + 1] != '-')
              {
                free (name);
                mulib_peg_fail_ (&r, r.pos, "expected <-");
                goto fail;
              }
            r.pos += 2;
          }
        body = mulib_peg_expression_ (&r);
        if (!body)
          {
            free (name);
            goto fail;
          }
        mulib_peg_space_ (&r);
        if (length - r.pos >= 2 && source[r.pos] == '-'
            && source[r.pos + 1] == '>')
          {
            r.pos += 2;
            action = mulib_peg_name_ (&r);
            if (!action)
              {
                free (name);
                goto fail;
              }
            mulib_peg_space_ (&r);
          }
        if (r.pos == length || source[r.pos] != ';')
          {
            free (name);
            free (action);
            mulib_peg_fail_ (&r, r.pos, "expected rule-ending semicolon");
            goto fail;
          }
        ++r.pos;
        if (g->count == g->capacity)
          {
            size_t cap = g->capacity ? g->capacity : 8;
            mulib_peg_rule_ *rules;
            if (g->capacity)
              {
                if (cap > SIZE_MAX / 2)
                  {
                    free (name);
                    free (action);
                    mulib_peg_fail_ (&r, r.pos, "rule allocation overflow");
                    goto fail;
                  }
                cap *= 2;
              }
            if (cap > SIZE_MAX / sizeof *rules)
              {
                free (name);
                free (action);
                mulib_peg_fail_ (&r, r.pos, "rule allocation overflow");
                goto fail;
              }
            rules = (mulib_peg_rule_ *)realloc (g->rules, cap * sizeof *rules);
            if (!rules)
              {
                free (name);
                free (action);
                mulib_peg_fail_ (&r, r.pos, "out of memory");
                goto fail;
              }
            g->rules = rules;
            g->capacity = cap;
          }
        capture = mulib_peg_compose_ (&r, MULIB_PARSEC_REPEAT, &body, 1);
        if (!capture)
          {
            free (name);
            free (action);
            goto fail;
          }
        capture->parser = mulib_parsec_capture (capture->parts, g->count);
        g->rules[g->count].name = name;
        g->rules[g->count].action = action;
        g->rules[g->count].body = body;
        g->rules[g->count].entry = &capture->parser;
        g->rules[g->count].mark = 0;
        ++g->count;
      }
    if (r.failed)
      goto fail;
    if (!g->count)
      {
        mulib_peg_fail_ (&r, 0, "grammar has no rules");
        goto fail;
      }
    /* Resolve only after the rule array stops moving. Every composite child is
     * itself a stable parsec reference, so forward-reference patches
     * propagate. */
    for (n = g->nodes; n; n = n->next)
      if (n->parser.kind == MULIB_PARSEC_REF)
        {
          for (i = 0; i < g->count; ++i)
            if (!strcmp (n->text, g->rules[i].name))
              break;
          if (i == g->count)
            {
              mulib_peg_fail_ (&r, n->offset, "undefined rule reference");
              goto fail;
            }
          n->reference = i;
          n->parser = mulib_parsec_ref (&g->rules[i].entry);
        }
    do
      {
        changed = 0;
        for (n = g->nodes; n; n = n->next)
          if (!n->nullable && mulib_peg_nullable_ (g, n))
            {
              n->nullable = 1;
              changed = 1;
            }
      }
    while (changed);
    for (n = g->nodes; n; n = n->next)
      if (n->parser.kind == MULIB_PARSEC_REPEAT && n->parser.as.unary.max > 1
          && n->children[0]->nullable)
        {
          mulib_peg_fail_ (&r, n->offset, "repetition can accept empty input");
          goto fail;
        }
    for (i = 0; i < g->count; ++i)
      if (!mulib_peg_visit_rule_ (&r, i, 0))
        goto fail;
    return g;
  fail:
    mulib_peg_destroy (g);
    return NULL;
  }
  size_t
  mulib_peg_rule_count (const mulib_peg_t *g)
  {
    return g ? g->count : 0;
  }
  mulib_peg_t *
  mulib_peg_compile (const char *s, size_t n, mulib_peg_error_t *e)
  {
    return mulib_peg_compile_ (s, n, e, 0);
  }
  mulib_peg_t *
  mulib_peg_compile_ebnf (const char *s, size_t n, mulib_peg_error_t *e)
  {
    return mulib_peg_compile_ (s, n, e, 1);
  }
  const char *
  mulib_peg_rule_name (const mulib_peg_t *g, size_t i)
  {
    return g && i < g->count ? g->rules[i].name : NULL;
  }
  const char *
  mulib_peg_rule_action (const mulib_peg_t *g, size_t i)
  {
    return g && i < g->count ? g->rules[i].action : NULL;
  }
  int
  mulib_peg_run (const mulib_peg_t *g, const char *start, const char *source,
                 size_t length, int entire,
                 const mulib_parsec_options_t *options,
                 mulib_parsec_result_t *result)
  {
    size_t i = 0;
    mulib_parsec_parser_t parts[2], root;
    if (!result)
      return -1;
    if (g && start)
      {
        for (i = 0; i < g->count; ++i)
          if (!strcmp (start, g->rules[i].name))
            break;
      }
    if (!g || i >= g->count)
      {
        memset (result, 0, sizeof *result);
        result->error.code = MULIB_PARSEC_INVALID;
        result->error.line = result->error.column = 1;
        result->error.expected = "unknown PEG start rule";
        return -1;
      }
    if (!entire)
      return mulib_parsec_run (g->rules[i].entry, source, length, options,
                               result);
    parts[0] = mulib_parsec_ref (&g->rules[i].entry);
    parts[1] = mulib_parsec_end ();
    root = mulib_parsec_sequence (parts, 2);
    return mulib_parsec_run (&root, source, length, options, result);
  }
#ifdef __cplusplus
}
#endif
#endif /* implementation */

/* Complete the optional owned loader when this header was included first. */
#if defined(MULIB_IMPL_PARSER) && defined(MULIB_PEG_IMPLEMENTED)              \
    && !defined(MULIB_PARSER_IMPLEMENTED)
#include "mulib-parsec.h"
#endif

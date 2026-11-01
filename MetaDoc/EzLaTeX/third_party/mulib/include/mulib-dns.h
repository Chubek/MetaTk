/**
 * @file mulib-dns.h
 * @brief Allocation-free, bounded DNS wire codec for a documented RR subset.
 * Define MULIB_IMPL_DNS in one translation unit. No implementation
 * dependencies.
 */
#ifndef MULIB_DNS_H
#define MULIB_DNS_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  enum
  {
    MULIB_DNS_END = 0,
    MULIB_DNS_OK = 1,
    MULIB_DNS_INVALID = -1,
    MULIB_DNS_LIMIT = -2,
    MULIB_DNS_UNSUPPORTED = -3,
    MULIB_DNS_MISMATCH = -4,
    MULIB_DNS_TRUNCATED = -5,
    MULIB_DNS_NOTFOUND = -6,
    MULIB_DNS_TRANSPORT = -7,
    MULIB_DNS_RESPONSE = -8
  };
  enum
  {
    MULIB_DNS_A = 1,
    MULIB_DNS_NS = 2,
    MULIB_DNS_CNAME = 5,
    MULIB_DNS_PTR = 12,
    MULIB_DNS_MX = 15,
    MULIB_DNS_TXT = 16,
    MULIB_DNS_AAAA = 28,
    MULIB_DNS_ANY = 255,
    MULIB_DNS_IN = 1,
    MULIB_DNS_QR = 0x8000,
    MULIB_DNS_AA = 0x0400,
    MULIB_DNS_TC = 0x0200,
    MULIB_DNS_RD = 0x0100,
    MULIB_DNS_RA = 0x0080,
    MULIB_DNS_MAX_RECORDS = 1024,
    MULIB_DNS_NAME_CAPACITY = 256
  };
  /** @brief Header fields in host byte order; counts are
   * question/answer/authority/additional. */
  typedef struct mulib_dns_header
  {
    uint16_t id, flags, counts[4];
  } mulib_dns_header_t;
  /** @brief Canonical lowercase name, without final dot; root is ".". */
  typedef struct mulib_dns_question
  {
    char name[256];
    uint16_t type, class_;
  } mulib_dns_question_t;
  /**
   * @brief RR owner and interpreted target; data borrows the input packet.
   * A/AAAA/TXT use data/data_length. NS/CNAME/PTR/MX use target; MX also
   * preference. On decode data/data_length retain original RDATA, including
   * compression.
   */
  typedef struct mulib_dns_rr
  {
    char name[256];
    uint16_t type, class_;
    uint32_t ttl;
    const unsigned char *data;
    size_t data_length;
    char target[256];
    uint16_t preference;
  } mulib_dns_rr_t;
  /** @brief Validated view borrowing an immutable complete packet (not TCP
   * framing). */
  typedef struct mulib_dns_message
  {
    const unsigned char *data;
    size_t length;
    mulib_dns_header_t header;
  } mulib_dns_message_t;
  /** @brief Linear iterator. Consume questions before RRs; do not modify its
   * fields. */
  typedef struct mulib_dns_iterator
  {
    const mulib_dns_message_t *message;
    size_t offset, index;
  } mulib_dns_iterator_t;
  /** @brief Caller-owned writer; bounded suffix dictionary, no heap
   * allocation. */
  typedef struct mulib_dns_writer
  {
    unsigned char *data;
    size_t capacity, length;
    uint16_t counts[4];
    unsigned section;
    size_t suffixes[128], suffix_count;
  } mulib_dns_writer_t;
  /** @brief Validate/canonicalize ASCII LDH/underscore labels; output
   * unchanged on error. */
  int mulib_dns_name_normalize (const char *text, size_t length,
                                char out[256]);
  /** @brief Compare two bounded NUL-terminated presentation names,
   * case/final-dot insensitive. */
  int mulib_dns_name_equal (const char *a, const char *b);
  /** @brief Decode a name with backward compression pointers; consumed counts
   * original bytes. */
  int mulib_dns_name_decode (const unsigned char *packet, size_t length,
                             size_t offset, char out[256], size_t *consumed);
  /** @brief Encode an uncompressed name; written and output unchanged on
   * error. */
  int mulib_dns_name_encode (const char *text, size_t length,
                             unsigned char *out, size_t capacity,
                             size_t *written);
  /** @brief Validate a complete packet, including supported RDATA; zero
   * max_records means 1024. */
  int mulib_dns_message_parse (const unsigned char *packet, size_t length,
                               size_t max_records, mulib_dns_message_t *out);
  /** @brief Initialize iteration over an already validated immutable view. */
  void mulib_dns_iterator_init (mulib_dns_iterator_t *iterator,
                                const mulib_dns_message_t *message);
  /** @brief Read next question: OK, END after questions, or negative error. */
  int mulib_dns_next_question (mulib_dns_iterator_t *iterator,
                               mulib_dns_question_t *out);
  /** @brief Read next RR, reporting section 1..3; END at packet end. */
  int mulib_dns_next_rr (mulib_dns_iterator_t *iterator, mulib_dns_rr_t *out,
                         unsigned *section);
  /** @brief Validate an application-supplied RR for the supported subset. */
  int mulib_dns_rr_validate (const mulib_dns_rr_t *record);
  /** @brief Start a message in caller storage, overwriting its first 12 bytes.
   */
  int mulib_dns_writer_init (mulib_dns_writer_t *writer, unsigned char *packet,
                             size_t capacity, uint16_t id, uint16_t flags);
  /** @brief Append a question before any RR; bounded suffix compression is
   * automatic. */
  int mulib_dns_writer_question (mulib_dns_writer_t *writer,
                                 const mulib_dns_question_t *question);
  /** @brief Append an RR in nondecreasing section order (1..3). */
  int mulib_dns_writer_rr (mulib_dns_writer_t *writer, unsigned section,
                           const mulib_dns_rr_t *record);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_DNS) && !defined(MULIB_DNS_IMPLEMENTED)
#define MULIB_DNS_IMPLEMENTED
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif
  static size_t
  mulib_dns_text_length_ (const char *s)
  {
    size_t n = 0;
    if (!s)
      return 256;
    while (n < 256 && s[n])
      ++n;
    return n;
  }
  static int
  mulib_dns_char_ (unsigned char c)
  {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
           || (c >= '0' && c <= '9') || c == '-' || c == '_';
  }
  static unsigned char
  mulib_dns_lower_ (unsigned char c)
  {
    return c >= 'A' && c <= 'Z' ? (unsigned char)(c + ('a' - 'A')) : c;
  }
  static uint16_t
  mulib_dns_u16_ (const unsigned char *p)
  {
    return (uint16_t)((unsigned)p[0] * 256u + p[1]);
  }
  static uint32_t
  mulib_dns_u32_ (const unsigned char *p)
  {
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8
           | p[3];
  }
  static void
  mulib_dns_put16_ (unsigned char *p, uint16_t v)
  {
    p[0] = (unsigned char)(v >> 8);
    p[1] = (unsigned char)v;
  }
  static void
  mulib_dns_put32_ (unsigned char *p, uint32_t v)
  {
    p[0] = (unsigned char)(v >> 24);
    p[1] = (unsigned char)(v >> 16);
    p[2] = (unsigned char)(v >> 8);
    p[3] = (unsigned char)v;
  }
  int
  mulib_dns_name_normalize (const char *s, size_t n, char out[256])
  {
    char text[256];
    size_t label = 0;
    if (!s || !out || !n || n > 254)
      return MULIB_DNS_INVALID;
    if (n == 1 && s[0] == '.')
      {
        text[0] = '.';
        text[1] = 0;
        memcpy (out, text, 2);
        return MULIB_DNS_OK;
      }
    if (s[n - 1] == '.')
      --n;
    if (!n || n > 253)
      return MULIB_DNS_INVALID;
    for (size_t i = 0; i < n; ++i)
      {
        unsigned char c = (unsigned char)s[i];
        if (c == '.')
          {
            if (!label || label > 63)
              return MULIB_DNS_INVALID;
            label = 0;
          }
        else
          {
            if (!mulib_dns_char_ (c) || ++label > 63)
              return MULIB_DNS_INVALID;
          }
        text[i] = (char)mulib_dns_lower_ (c);
      }
    if (!label)
      return MULIB_DNS_INVALID;
    text[n] = 0;
    memcpy (out, text, n + 1);
    return MULIB_DNS_OK;
  }
  int
  mulib_dns_name_equal (const char *a, const char *b)
  {
    char x[256], y[256];
    return mulib_dns_name_normalize (a, mulib_dns_text_length_ (a), x)
               == MULIB_DNS_OK
           && mulib_dns_name_normalize (b, mulib_dns_text_length_ (b), y)
                  == MULIB_DNS_OK
           && !strcmp (x, y);
  }
  int
  mulib_dns_name_decode (const unsigned char *p, size_t n, size_t offset,
                         char out[256], size_t *used)
  {
    char text[256];
    size_t pos = offset, length = 0, wire = 1, consumed = 0, jumps = 0;
    int jumped = 0;
    if (!p || !out || !used || n > 65535 || offset >= n)
      return MULIB_DNS_INVALID;
    for (;;)
      {
        unsigned c;
        if (pos >= n)
          return MULIB_DNS_INVALID;
        c = p[pos];
        if ((c & 0xc0u) == 0xc0u)
          {
            size_t target;
            if (n - pos < 2)
              return MULIB_DNS_INVALID;
            target = (size_t)(c & 63u) * 256 + p[pos + 1];
            if (target >= pos)
              return MULIB_DNS_INVALID;
            if (++jumps > 128)
              return MULIB_DNS_LIMIT;
            if (!jumped)
              consumed += 2;
            jumped = 1;
            pos = target;
            continue;
          }
        if (c & 0xc0u)
          return MULIB_DNS_UNSUPPORTED;
        ++pos;
        if (!jumped)
          ++consumed;
        if (!c)
          break;
        if (c > n - pos || wire + c + 1 > 255)
          return MULIB_DNS_INVALID;
        wire += c + 1;
        if (length)
          text[length++] = '.';
        for (unsigned i = 0; i < c; ++i)
          {
            if (!mulib_dns_char_ (p[pos + i]))
              return MULIB_DNS_UNSUPPORTED;
            text[length++] = (char)mulib_dns_lower_ (p[pos + i]);
          }
        pos += c;
        if (!jumped)
          consumed += c;
      }
    if (!length)
      text[length++] = '.';
    text[length] = 0;
    memcpy (out, text, length + 1);
    *used = consumed;
    return MULIB_DNS_OK;
  }
  int
  mulib_dns_name_encode (const char *s, size_t n, unsigned char *out,
                         size_t cap, size_t *written)
  {
    char name[256];
    unsigned char wire[255];
    size_t pos = 0, start = 0, len;
    int rc;
    if (!out || !written)
      return MULIB_DNS_INVALID;
    rc = mulib_dns_name_normalize (s, n, name);
    if (rc != MULIB_DNS_OK)
      return rc;
    len = strlen (name);
    if (name[0] != '.' || name[1])
      {
        while (start < len)
          {
            size_t end = start;
            while (end < len && name[end] != '.')
              ++end;
            wire[pos++] = (unsigned char)(end - start);
            memcpy (wire + pos, name + start, end - start);
            pos += end - start;
            start = end + 1;
          }
      }
    wire[pos++] = 0;
    if (cap < pos)
      return MULIB_DNS_LIMIT;
    memcpy (out, wire, pos);
    *written = pos;
    return MULIB_DNS_OK;
  }
  void
  mulib_dns_iterator_init (mulib_dns_iterator_t *it,
                           const mulib_dns_message_t *m)
  {
    if (it)
      {
        it->message = m;
        it->offset = 12;
        it->index = 0;
      }
  }
  int
  mulib_dns_next_question (mulib_dns_iterator_t *it, mulib_dns_question_t *out)
  {
    mulib_dns_question_t q;
    const mulib_dns_message_t *m;
    size_t used, pos;
    int rc;
    if (!it || !out || !(m = it->message) || it->offset > m->length)
      return MULIB_DNS_INVALID;
    if (it->index >= m->header.counts[0])
      return MULIB_DNS_END;
    memset (&q, 0, sizeof (q));
    pos = it->offset;
    rc = mulib_dns_name_decode (m->data, m->length, pos, q.name, &used);
    if (rc != MULIB_DNS_OK)
      return rc;
    pos += used;
    if (m->length - pos < 4)
      return MULIB_DNS_INVALID;
    q.type = mulib_dns_u16_ (m->data + pos);
    q.class_ = mulib_dns_u16_ (m->data + pos + 2);
    if (!q.type || !q.class_)
      return MULIB_DNS_INVALID;
    it->offset = pos + 4;
    ++it->index;
    *out = q;
    return MULIB_DNS_OK;
  }
  static int
  mulib_dns_txt_valid_ (const unsigned char *data, size_t n)
  {
    size_t pos = 0;
    if (!data || !n)
      return MULIB_DNS_INVALID;
    while (pos < n)
      {
        size_t count = data[pos++];
        if (count > n - pos)
          return MULIB_DNS_INVALID;
        pos += count;
      }
    return MULIB_DNS_OK;
  }
  int
  mulib_dns_rr_validate (const mulib_dns_rr_t *rr)
  {
    char text[256];
    int rc;
    if (!rr || !rr->class_)
      return MULIB_DNS_INVALID;
    rc = mulib_dns_name_normalize (rr->name, mulib_dns_text_length_ (rr->name),
                                   text);
    if (rc != MULIB_DNS_OK)
      return rc;
    switch (rr->type)
      {
      case MULIB_DNS_A:
        return rr->data && rr->data_length == 4 ? MULIB_DNS_OK
                                                : MULIB_DNS_INVALID;
      case MULIB_DNS_AAAA:
        return rr->data && rr->data_length == 16 ? MULIB_DNS_OK
                                                 : MULIB_DNS_INVALID;
      case MULIB_DNS_TXT:
        if (rr->data_length > 65535)
          return MULIB_DNS_LIMIT;
        return mulib_dns_txt_valid_ (rr->data, rr->data_length);
      case MULIB_DNS_NS:
      case MULIB_DNS_CNAME:
      case MULIB_DNS_PTR:
      case MULIB_DNS_MX:
        return mulib_dns_name_normalize (
            rr->target, mulib_dns_text_length_ (rr->target), text);
      default:
        return MULIB_DNS_UNSUPPORTED;
      }
  }
  int
  mulib_dns_next_rr (mulib_dns_iterator_t *it, mulib_dns_rr_t *out,
                     unsigned *section)
  {
    const mulib_dns_message_t *m;
    mulib_dns_rr_t rr;
    size_t pos, used, end, first, total;
    unsigned sec;
    int rc;
    if (!it || !out || !section || !(m = it->message)
        || it->offset > m->length)
      return MULIB_DNS_INVALID;
    first = m->header.counts[0];
    total = first;
    for (unsigned i = 1; i < 4; ++i)
      total += m->header.counts[i];
    if (it->index < first)
      return MULIB_DNS_INVALID;
    if (it->index >= total)
      return MULIB_DNS_END;
    for (sec = 1; sec < 3 && it->index >= first + m->header.counts[sec]; ++sec)
      first += m->header.counts[sec];
    memset (&rr, 0, sizeof (rr));
    pos = it->offset;
    rc = mulib_dns_name_decode (m->data, m->length, pos, rr.name, &used);
    if (rc != MULIB_DNS_OK)
      return rc;
    pos += used;
    if (m->length - pos < 10)
      return MULIB_DNS_INVALID;
    rr.type = mulib_dns_u16_ (m->data + pos);
    rr.class_ = mulib_dns_u16_ (m->data + pos + 2);
    rr.ttl = mulib_dns_u32_ (m->data + pos + 4);
    rr.data_length = mulib_dns_u16_ (m->data + pos + 8);
    pos += 10;
    if (rr.data_length > m->length - pos)
      return MULIB_DNS_INVALID;
    rr.data = m->data + pos;
    end = pos + rr.data_length;
    if (rr.type == MULIB_DNS_NS || rr.type == MULIB_DNS_CNAME
        || rr.type == MULIB_DNS_PTR || rr.type == MULIB_DNS_MX)
      {
        if (rr.type == MULIB_DNS_MX)
          {
            if (end - pos < 2)
              return MULIB_DNS_INVALID;
            rr.preference = mulib_dns_u16_ (m->data + pos);
            pos += 2;
          }
        rc = mulib_dns_name_decode (m->data, m->length, pos, rr.target, &used);
        if (rc != MULIB_DNS_OK)
          return rc;
        if (used != end - pos)
          return MULIB_DNS_INVALID;
      }
    rc = mulib_dns_rr_validate (&rr);
    if (rc != MULIB_DNS_OK)
      return rc;
    it->offset = end;
    ++it->index;
    *out = rr;
    *section = sec;
    return MULIB_DNS_OK;
  }
  int
  mulib_dns_message_parse (const unsigned char *p, size_t n,
                           size_t max_records, mulib_dns_message_t *out)
  {
    mulib_dns_message_t m;
    mulib_dns_iterator_t it;
    mulib_dns_question_t q;
    mulib_dns_rr_t rr;
    size_t total = 0;
    unsigned sec;
    int rc;
    if (!p || !out || n < 12)
      return MULIB_DNS_INVALID;
    if (n > 65535)
      return MULIB_DNS_LIMIT;
    if (!max_records || max_records > MULIB_DNS_MAX_RECORDS)
      max_records = MULIB_DNS_MAX_RECORDS;
    memset (&m, 0, sizeof (m));
    m.data = p;
    m.length = n;
    m.header.id = mulib_dns_u16_ (p);
    m.header.flags = mulib_dns_u16_ (p + 2);
    for (unsigned i = 0; i < 4; ++i)
      {
        m.header.counts[i] = mulib_dns_u16_ (p + 4 + 2 * i);
        total += m.header.counts[i];
      }
    if (total > max_records)
      return MULIB_DNS_LIMIT;
    mulib_dns_iterator_init (&it, &m);
    while ((rc = mulib_dns_next_question (&it, &q)) == MULIB_DNS_OK)
      {
      }
    if (rc != MULIB_DNS_END)
      return rc;
    while ((rc = mulib_dns_next_rr (&it, &rr, &sec)) == MULIB_DNS_OK)
      {
      }
    if (rc != MULIB_DNS_END)
      return rc;
    if (it.offset != n)
      return MULIB_DNS_INVALID;
    *out = m;
    return MULIB_DNS_OK;
  }
  static void
  mulib_dns_writer_counts_ (mulib_dns_writer_t *w)
  {
    for (unsigned i = 0; i < 4; ++i)
      mulib_dns_put16_ (w->data + 4 + 2 * i, w->counts[i]);
  }
  static int
  mulib_dns_writer_room_ (const mulib_dns_writer_t *w, size_t n)
  {
    return n <= w->capacity - w->length;
  }
  static int
  mulib_dns_writer_name_ (mulib_dns_writer_t *w, const char *name)
  {
    unsigned char wire[255];
    size_t n, prefix, match = SIZE_MAX, pos = 0;
    int rc;
    rc = mulib_dns_name_encode (name, mulib_dns_text_length_ (name), wire,
                                sizeof (wire), &n);
    if (rc != MULIB_DNS_OK)
      return rc;
    prefix = n;
    while (wire[pos])
      {
        char suffix[256];
        size_t used;
        rc = mulib_dns_name_decode (wire, n, pos, suffix, &used);
        if (rc != MULIB_DNS_OK)
          return rc;
        for (size_t i = 0; i < w->suffix_count; ++i)
          {
            char previous[256];
            size_t ignored;
            if (mulib_dns_name_decode (w->data, w->length, w->suffixes[i],
                                       previous, &ignored)
                    == MULIB_DNS_OK
                && !strcmp (suffix, previous))
              {
                prefix = pos;
                match = w->suffixes[i];
                break;
              }
          }
        if (match != SIZE_MAX)
          break;
        pos += (size_t)wire[pos] + 1;
      }
    if (!mulib_dns_writer_room_ (w, prefix + (match != SIZE_MAX ? 2u : 0u)))
      return MULIB_DNS_LIMIT;
    pos = 0;
    while (pos < prefix && wire[pos])
      {
        if (w->suffix_count < 128 && w->length + pos < 16384)
          w->suffixes[w->suffix_count++] = w->length + pos;
        pos += (size_t)wire[pos] + 1;
      }
    memcpy (w->data + w->length, wire, prefix);
    w->length += prefix;
    if (match != SIZE_MAX)
      {
        w->data[w->length++] = (unsigned char)(0xc0u | (match >> 8));
        w->data[w->length++] = (unsigned char)match;
      }
    return MULIB_DNS_OK;
  }
  int
  mulib_dns_writer_init (mulib_dns_writer_t *w, unsigned char *p, size_t cap,
                         uint16_t id, uint16_t flags)
  {
    mulib_dns_writer_t value;
    if (!w || !p)
      return MULIB_DNS_INVALID;
    if (cap < 12)
      return MULIB_DNS_LIMIT;
    memset (&value, 0, sizeof (value));
    value.data = p;
    value.capacity = cap > 65535 ? 65535 : cap;
    value.length = 12;
    memset (p, 0, 12);
    mulib_dns_put16_ (p, id);
    mulib_dns_put16_ (p + 2, flags);
    *w = value;
    return MULIB_DNS_OK;
  }
  static int
  mulib_dns_writer_valid_ (const mulib_dns_writer_t *w)
  {
    size_t count = 0;
    if (!w || !w->data || w->length < 12 || w->length > w->capacity
        || w->capacity > 65535 || w->suffix_count > 128 || w->section > 3)
      return MULIB_DNS_INVALID;
    for (unsigned i = 0; i < 4; ++i)
      count += w->counts[i];
    return count < MULIB_DNS_MAX_RECORDS ? MULIB_DNS_OK : MULIB_DNS_LIMIT;
  }
  int
  mulib_dns_writer_question (mulib_dns_writer_t *w,
                             const mulib_dns_question_t *q)
  {
    mulib_dns_writer_t saved;
    int rc = mulib_dns_writer_valid_ (w);
    if (rc != MULIB_DNS_OK)
      return rc;
    if (!q || !q->type || !q->class_ || w->section)
      return MULIB_DNS_INVALID;
    saved = *w;
    rc = mulib_dns_writer_name_ (w, q->name);
    if (rc == MULIB_DNS_OK && !mulib_dns_writer_room_ (w, 4))
      rc = MULIB_DNS_LIMIT;
    if (rc != MULIB_DNS_OK)
      {
        *w = saved;
        return rc;
      }
    mulib_dns_put16_ (w->data + w->length, q->type);
    mulib_dns_put16_ (w->data + w->length + 2, q->class_);
    w->length += 4;
    ++w->counts[0];
    mulib_dns_writer_counts_ (w);
    return MULIB_DNS_OK;
  }
  int
  mulib_dns_writer_rr (mulib_dns_writer_t *w, unsigned sec,
                       const mulib_dns_rr_t *rr)
  {
    mulib_dns_writer_t saved;
    size_t header, begin;
    int rc = mulib_dns_writer_valid_ (w);
    if (rc != MULIB_DNS_OK)
      return rc;
    if (sec < 1 || sec > 3 || sec < w->section)
      return MULIB_DNS_INVALID;
    rc = mulib_dns_rr_validate (rr);
    if (rc != MULIB_DNS_OK)
      return rc;
    saved = *w;
    rc = mulib_dns_writer_name_ (w, rr->name);
    if (rc == MULIB_DNS_OK && !mulib_dns_writer_room_ (w, 10))
      rc = MULIB_DNS_LIMIT;
    if (rc != MULIB_DNS_OK)
      {
        *w = saved;
        return rc;
      }
    header = w->length;
    w->length += 10;
    begin = w->length;
    if (rr->type == MULIB_DNS_A || rr->type == MULIB_DNS_AAAA
        || rr->type == MULIB_DNS_TXT)
      {
        if (!mulib_dns_writer_room_ (w, rr->data_length))
          rc = MULIB_DNS_LIMIT;
        else
          {
            memcpy (w->data + w->length, rr->data, rr->data_length);
            w->length += rr->data_length;
          }
      }
    else
      {
        if (rr->type == MULIB_DNS_MX)
          {
            if (!mulib_dns_writer_room_ (w, 2))
              rc = MULIB_DNS_LIMIT;
            else
              {
                mulib_dns_put16_ (w->data + w->length, rr->preference);
                w->length += 2;
              }
          }
        if (rc == MULIB_DNS_OK)
          rc = mulib_dns_writer_name_ (w, rr->target);
      }
    if (rc != MULIB_DNS_OK)
      {
        *w = saved;
        return rc;
      }
    mulib_dns_put16_ (w->data + header, rr->type);
    mulib_dns_put16_ (w->data + header + 2, rr->class_);
    mulib_dns_put32_ (w->data + header + 4, rr->ttl);
    mulib_dns_put16_ (w->data + header + 8, (uint16_t)(w->length - begin));
    ++w->counts[sec];
    w->section = sec;
    mulib_dns_writer_counts_ (w);
    return MULIB_DNS_OK;
  }
#ifdef __cplusplus
}
#endif
#endif

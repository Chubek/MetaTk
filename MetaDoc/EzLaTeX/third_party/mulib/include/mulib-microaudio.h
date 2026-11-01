/**
 * @file mulib-microaudio.h
 * @brief Portable interleaved float PCM, PCM8/16 WAV, tone generation and
 * mixing. Define MULIB_IMPL_MICROAUDIO once. No module dependencies; link libm
 * on Unix.
 */
#ifndef MULIB_MICROAUDIO_H
#define MULIB_MICROAUDIO_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef enum mulib_microaudio_status
  {
    MULIB_MICROAUDIO_OK,
    MULIB_MICROAUDIO_INVALID,
    MULIB_MICROAUDIO_MALFORMED,
    MULIB_MICROAUDIO_UNSUPPORTED,
    MULIB_MICROAUDIO_LIMIT,
    MULIB_MICROAUDIO_MEMORY
  } mulib_microaudio_status_t;
  /** @brief Owned samples in [-1,1], frame-major then channel; initialize to
   * zero. */
  typedef struct mulib_microaudio_pcm
  {
    float *samples;
    size_t frames;
    uint32_t sample_rate;
    unsigned channels;
  } mulib_microaudio_pcm_t;
  /** @brief NULL limits selects 10 million samples, 64 MiB encoded bytes. Zero
   * is zero. */
  typedef struct mulib_microaudio_limits
  {
    size_t samples, bytes;
  } mulib_microaudio_limits_t;
  /** @brief Allocate silent PCM; out must be empty and is unchanged on
   * failure. */
  mulib_microaudio_status_t
  mulib_microaudio_create (size_t frames, unsigned channels, uint32_t rate,
                           const mulib_microaudio_limits_t *limits,
                           mulib_microaudio_pcm_t *out);
  /** @brief Release and clear PCM. NULL is harmless. */
  void mulib_microaudio_free (mulib_microaudio_pcm_t *pcm);
  /** @brief Decode exact RIFF/WAVE PCM unsigned-8 or signed-16, skipping
   * unknown chunks. */
  mulib_microaudio_status_t
  mulib_microaudio_wav_decode (const void *bytes, size_t length,
                               const mulib_microaudio_limits_t *limits,
                               mulib_microaudio_pcm_t *out);
  /** @brief Encode PCM16 LE WAV into malloc-owned bytes; outputs unchanged on
   * failure. */
  mulib_microaudio_status_t
  mulib_microaudio_wav_encode (const mulib_microaudio_pcm_t *pcm,
                               const mulib_microaudio_limits_t *limits,
                               unsigned char **bytes, size_t *length);
  /** @brief Replace all samples by a sine; finite frequency [0,rate/2],
   * amplitude [0,1]. */
  mulib_microaudio_status_t mulib_microaudio_sine (mulib_microaudio_pcm_t *pcm,
                                                   double frequency,
                                                   double amplitude,
                                                   double phase_radians);
  /** @brief Add gain*source, saturating [-1,1]; same-format, overlapping
   * ranges supported. Borrowed subviews may have different sample bases.
   * Neither buffer is freed or retained; do not pass a borrowed subview to
   * mulib_microaudio_free().
   */
  mulib_microaudio_status_t
  mulib_microaudio_mix (mulib_microaudio_pcm_t *destination,
                        size_t destination_frame,
                        const mulib_microaudio_pcm_t *source,
                        size_t source_frame, size_t frames, double gain);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_MICROAUDIO) && !defined(MULIB_MICROAUDIO_IMPLEMENTED)
#define MULIB_MICROAUDIO_IMPLEMENTED
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif
  static mulib_microaudio_limits_t
  mulib_microaudio_limits_ (const mulib_microaudio_limits_t *l)
  {
    mulib_microaudio_limits_t d = { 10000000, 64u * 1024u * 1024u };
    return l ? *l : d;
  }
  static int
  mulib_microaudio_shape_ (const mulib_microaudio_pcm_t *p)
  {
    return p && p->channels && p->channels <= 32 && p->sample_rate
           && p->frames <= SIZE_MAX / p->channels / sizeof (float)
           && (!p->frames || p->samples);
  }
  static int
  mulib_microaudio_valid_ (const mulib_microaudio_pcm_t *p)
  {
    size_t i, n;
    if (!mulib_microaudio_shape_ (p))
      return 0;
    n = p->frames * p->channels;
    for (i = 0; i < n; ++i)
      if (!isfinite (p->samples[i]) || p->samples[i] < -1 || p->samples[i] > 1)
        return 0;
    return 1;
  }
  mulib_microaudio_status_t
  mulib_microaudio_create (size_t frames, unsigned channels, uint32_t rate,
                           const mulib_microaudio_limits_t *limits,
                           mulib_microaudio_pcm_t *out)
  {
    mulib_microaudio_limits_t l = mulib_microaudio_limits_ (limits);
    mulib_microaudio_pcm_t p;
    if (!out || out->samples || out->frames || !channels || channels > 32
        || !rate)
      return MULIB_MICROAUDIO_INVALID;
    if (frames > SIZE_MAX / channels / sizeof (float)
        || frames > l.samples / channels)
      return MULIB_MICROAUDIO_LIMIT;
    p.frames = frames;
    p.channels = channels;
    p.sample_rate = rate;
    p.samples = NULL;
    if (frames)
      {
        p.samples = (float *)malloc (frames * channels * sizeof (float));
        if (!p.samples)
          return MULIB_MICROAUDIO_MEMORY;
        /* C does not require all-bits-zero to represent floating zero. */
        {
          size_t i;
          for (i = 0; i < frames * channels; ++i)
            p.samples[i] = 0.0f;
        }
      }
    *out = p;
    return MULIB_MICROAUDIO_OK;
  }
  void
  mulib_microaudio_free (mulib_microaudio_pcm_t *p)
  {
    if (p)
      {
        free (p->samples);
        memset (p, 0, sizeof *p);
      }
  }
  static uint32_t
  mulib_microaudio_u32_ (const unsigned char *p)
  {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16
           | (uint32_t)p[3] << 24;
  }
  static unsigned
  mulib_microaudio_u16_ (const unsigned char *p)
  {
    return p[0] | (unsigned)p[1] << 8;
  }
  static void
  mulib_microaudio_w32_ (unsigned char *p, uint32_t n)
  {
    p[0] = (unsigned char)n;
    p[1] = (unsigned char)(n >> 8);
    p[2] = (unsigned char)(n >> 16);
    p[3] = (unsigned char)(n >> 24);
  }
  static void
  mulib_microaudio_w16_ (unsigned char *p, unsigned n)
  {
    p[0] = (unsigned char)n;
    p[1] = (unsigned char)(n >> 8);
  }
  mulib_microaudio_status_t
  mulib_microaudio_wav_decode (const void *bytes, size_t n,
                               const mulib_microaudio_limits_t *limits,
                               mulib_microaudio_pcm_t *out)
  {
    const unsigned char *s = (const unsigned char *)bytes, *fmt = NULL,
                        *data = NULL;
    size_t pos = 12, data_size = 0, i;
    unsigned channels, bits, align;
    uint32_t rate;
    mulib_microaudio_pcm_t p = { 0 };
    mulib_microaudio_limits_t l = mulib_microaudio_limits_ (limits);
    mulib_microaudio_status_t rc;
    if (!out || out->samples || out->frames || (!s && n))
      return MULIB_MICROAUDIO_INVALID;
    if (n > l.bytes)
      return MULIB_MICROAUDIO_LIMIT;
    if (CHAR_BIT != 8)
      return MULIB_MICROAUDIO_UNSUPPORTED;
    if (n < 12 || memcmp (s, "RIFF", 4) || memcmp (s + 8, "WAVE", 4)
        || (uintmax_t)mulib_microaudio_u32_ (s + 4) + 8 != (uintmax_t)n)
      return MULIB_MICROAUDIO_MALFORMED;
    while (pos < n)
      {
        size_t k, padded;
        if (n - pos < 8)
          return MULIB_MICROAUDIO_MALFORMED;
        k = mulib_microaudio_u32_ (s + pos + 4);
        pos += 8;
        if (k > n - pos || (k & 1u) > n - pos - k)
          return MULIB_MICROAUDIO_MALFORMED;
        padded = k + (k & 1u);
        if (!memcmp (s + pos - 8, "fmt ", 4))
          {
            if (fmt || (k != 16 && k != 18))
              return MULIB_MICROAUDIO_UNSUPPORTED;
            if (k == 18 && mulib_microaudio_u16_ (s + pos + 16))
              return MULIB_MICROAUDIO_UNSUPPORTED;
            fmt = s + pos;
          }
        else if (!memcmp (s + pos - 8, "data", 4))
          {
            if (data)
              return MULIB_MICROAUDIO_MALFORMED;
            data = s + pos;
            data_size = k;
          }
        pos += padded;
      }
    if (!fmt || !data)
      return MULIB_MICROAUDIO_MALFORMED;
    channels = mulib_microaudio_u16_ (fmt + 2);
    rate = mulib_microaudio_u32_ (fmt + 4);
    align = mulib_microaudio_u16_ (fmt + 12);
    bits = mulib_microaudio_u16_ (fmt + 14);
    if (mulib_microaudio_u16_ (fmt) != 1 || (bits != 8 && bits != 16)
        || channels > 32)
      return MULIB_MICROAUDIO_UNSUPPORTED;
    if (!channels || !rate || align != channels * (bits / 8)
        || data_size % align
        || (uint64_t)rate * align != mulib_microaudio_u32_ (fmt + 8))
      return MULIB_MICROAUDIO_MALFORMED;
    rc = mulib_microaudio_create (data_size / align, channels, rate, &l, &p);
    if (rc != MULIB_MICROAUDIO_OK)
      return rc;
    for (i = 0; i < p.frames * channels; ++i)
      {
        if (bits == 8)
          p.samples[i] = (float)((int)data[i] - 128) / 128.0f;
        else
          {
            unsigned u = mulib_microaudio_u16_ (data + i * 2);
            int32_t v = u >= 32768 ? (int32_t)u - 65536 : (int32_t)u;
            p.samples[i] = (float)v / 32768.0f;
          }
      }
    *out = p;
    return MULIB_MICROAUDIO_OK;
  }
  mulib_microaudio_status_t
  mulib_microaudio_wav_encode (const mulib_microaudio_pcm_t *p,
                               const mulib_microaudio_limits_t *limits,
                               unsigned char **bytes, size_t *length)
  {
    mulib_microaudio_limits_t l = mulib_microaudio_limits_ (limits);
    size_t count, n, i;
    unsigned char *s;
    if (!bytes || !length || !mulib_microaudio_shape_ (p))
      return MULIB_MICROAUDIO_INVALID;
    if (CHAR_BIT != 8)
      return MULIB_MICROAUDIO_UNSUPPORTED;
    count = p->frames * p->channels;
    if (count > l.samples || count > (SIZE_MAX - 44) / 2
        || count > (UINT32_MAX - 36u) / 2
        || (uint64_t)p->sample_rate * p->channels * 2 > UINT32_MAX)
      return MULIB_MICROAUDIO_LIMIT;
    n = 44 + count * 2;
    if (n > l.bytes)
      return MULIB_MICROAUDIO_LIMIT;
    if (!mulib_microaudio_valid_ (p))
      return MULIB_MICROAUDIO_INVALID;
    s = (unsigned char *)malloc (n);
    if (!s)
      return MULIB_MICROAUDIO_MEMORY;
    memcpy (s, "RIFF", 4);
    mulib_microaudio_w32_ (s + 4, (uint32_t)(n - 8));
    memcpy (s + 8, "WAVEfmt ", 8);
    mulib_microaudio_w32_ (s + 16, 16);
    mulib_microaudio_w16_ (s + 20, 1);
    mulib_microaudio_w16_ (s + 22, p->channels);
    mulib_microaudio_w32_ (s + 24, p->sample_rate);
    mulib_microaudio_w32_ (s + 28, p->sample_rate * p->channels * 2);
    mulib_microaudio_w16_ (s + 32, p->channels * 2);
    mulib_microaudio_w16_ (s + 34, 16);
    memcpy (s + 36, "data", 4);
    mulib_microaudio_w32_ (s + 40, (uint32_t)(count * 2));
    for (i = 0; i < count; ++i)
      {
        double d = (double)p->samples[i] * 32768.0;
        long v = d >= 32767 ? 32767 : d <= -32768 ? -32768 : lround (d);
        mulib_microaudio_w16_ (s + 44 + i * 2,
                               v < 0 ? (unsigned)(v + 65536) : (unsigned)v);
      }
    *bytes = s;
    *length = n;
    return MULIB_MICROAUDIO_OK;
  }
  mulib_microaudio_status_t
  mulib_microaudio_sine (mulib_microaudio_pcm_t *p, double frequency,
                         double amplitude, double phase)
  {
    size_t i;
    unsigned c;
    double step;
    const double tau = 6.283185307179586476925286766559;
    if (!mulib_microaudio_shape_ (p) || !isfinite (frequency)
        || !isfinite (amplitude) || !isfinite (phase) || frequency < 0
        || frequency > (double)p->sample_rate / 2 || amplitude < 0
        || amplitude > 1)
      return MULIB_MICROAUDIO_INVALID;
    phase = fmod (phase, tau);
    step = tau * (frequency / p->sample_rate);
    for (i = 0; i < p->frames; ++i)
      {
        float v = (float)(amplitude * sin (phase));
        for (c = 0; c < p->channels; ++c)
          p->samples[i * p->channels + c] = v;
        phase += step;
        if (phase >= tau)
          phase -= tau;
      }
    return MULIB_MICROAUDIO_OK;
  }
  mulib_microaudio_status_t
  mulib_microaudio_mix (mulib_microaudio_pcm_t *d, size_t di,
                        const mulib_microaudio_pcm_t *s, size_t si,
                        size_t frames, double gain)
  {
    size_t i, n, a, b;
    int reverse;
    if (!mulib_microaudio_shape_ (d) || !mulib_microaudio_shape_ (s)
        || !isfinite (gain) || d->channels != s->channels
        || d->sample_rate != s->sample_rate || di > d->frames || si > s->frames
        || frames > d->frames - di || frames > s->frames - si)
      return MULIB_MICROAUDIO_INVALID;
    a = di * d->channels;
    b = si * s->channels;
    n = frames * d->channels;
    for (i = 0; i < n; ++i)
      if (!isfinite (d->samples[a + i]) || !isfinite (s->samples[b + i])
          || d->samples[a + i] < -1 || d->samples[a + i] > 1
          || s->samples[b + i] < -1 || s->samples[b + i] > 1)
        return MULIB_MICROAUDIO_INVALID;
    /* Equality tests are valid even for unrelated arrays; relational pointer
     * comparisons (or assumptions about uintptr_t ordering) are not portable.
     * This extra linear walk also handles views with different sample bases.
     */
    reverse = 0;
    for (i = 1; i < n; ++i)
      if (d->samples + a == s->samples + b + i)
        {
          reverse = 1;
          break;
        }
    for (i = 0; i < n; ++i)
      {
        size_t k = reverse ? n - 1 - i : i;
        double v = d->samples[a + k] + gain * s->samples[b + k];
        d->samples[a + k] = (float)(v > 1 ? 1 : v < -1 ? -1 : v);
      }
    return MULIB_MICROAUDIO_OK;
  }
#ifdef __cplusplus
}
#endif
#endif

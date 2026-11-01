# microaudio: portable PCM and WAV utilities

`mulib-microaudio.h` provides interleaved float PCM storage, sine generation,
saturating mixing, and a focused RIFF/WAVE codec. It does **not** open audio
devices or decode compressed audio. There are no MuLIB or third-party dependencies.

## Integration

Define `MULIB_IMPL_MICROAUDIO` in one C11 unit. Other units include declarations
normally; C++ consumers have C linkage. Math functions may require `-lm`:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude examples/modules/microaudio.c -lm -o microaudio
./microaudio
```

The example generates 0.1 seconds of 440 Hz mono, encodes PCM16 in memory,
decodes it, and reports its format without touching devices or the filesystem.

## PCM contract

`mulib_microaudio_pcm_t` owns `frames * channels` float samples, indexed by
`samples[frame * channels + channel]`. Valid samples are finite and within [-1,1].
Channel count is 1..32; sample rate is any positive uint32 value. No particular
speaker layout is inferred. `create` allocates actual floating zeros, rather than
assuming all-bits-zero is a portable float representation. Zero frames is valid
and needs no sample allocation.

Initialize output PCM to `{0}`. Creation and decoding require an empty output
(no samples/frames) and leave it unchanged on failure. Successful output replaces
its metadata. `mulib_microaudio_free` releases and clears it; NULL is safe. Do
not shallow-copy ownership or free a borrowed view as if it owned an allocation.
The public structure allows hosts to fill samples directly, but correct pointer
capacities remain the host's responsibility.

```c
mulib_microaudio_pcm_t tone = {0};
unsigned char *wav = NULL;
size_t length = 0;
if (mulib_microaudio_create(8000, 1, 8000, NULL, &tone) == MULIB_MICROAUDIO_OK &&
    mulib_microaudio_sine(&tone, 440, 0.5, 0) == MULIB_MICROAUDIO_OK) {
    mulib_microaudio_status_t status =
        mulib_microaudio_wav_encode(&tone, NULL, &wav, &length);
    (void)status;
}
free(wav);
mulib_microaudio_free(&tone);
```

## WAV subset and quantization

Decoding requires exactly one little-endian `RIFF`/`WAVE` form. The RIFF size
must match the supplied length exactly; trailing bytes, truncated chunk headers,
payloads, or odd-byte padding are rejected. Unknown chunks are skipped after
their sizes/padding are checked. Padding's value is ignored.

There must be one `fmt ` and one `data` chunk, in either order. `fmt ` must have
16 bytes or 18 bytes with a zero extension-size field. Format code 1 only:
unsigned PCM8 or signed little-endian PCM16. Channel count, positive rate,
block alignment, byte rate, and whole-frame data length are validated. Compressed
formats, float WAV, PCM24/32, extensible WAV, RF64, RIFX, metadata interpretation,
and multiple data chunks are not supported. Empty data is valid.

PCM8 decodes as `(u-128)/128`; PCM16 as `signed_value/32768`. Encoding always
produces a canonical 44-byte-header PCM16 LE WAV without metadata. Values are
scaled by 32768, rounded with `lround` (halfway away from zero), and saturated
to [-32768,32767]. Consequently +1 roundtrips as 32767/32768; all decoded PCM16
samples re-encode exactly. Arbitrary source samples have at most one PCM16 step
roundtrip error. No dithering is added.

WAV operations require `CHAR_BIT == 8` and return UNSUPPORTED otherwise. They do
not depend on host endian, signed right shifts, unaligned access, two's-complement
signed storage, or IEEE float bit representation. Integer format fields are
assembled bytewise.

## Generation and mixing

`sine` replaces every frame/channel with the same sine sample. Frequency must be
finite in [0,rate/2], amplitude in [0,1], and phase must be finite radians. Phase
is reduced and advanced incrementally to avoid frame-index multiplication
overflow. There is no anti-aliasing/resampling or saved oscillator state between
calls; the phase argument controls each call's starting point.

`mix(dst,dst_frame,src,src_frame,frames,gain)` adds a finite gain times the source
to destination, saturating each result to [-1,1]. Rates and channel counts must
match; ranges must lie entirely inside their PCM. Gain may be negative. Input
ranges are validated for finite in-range samples **before any mutation**. Empty
ranges are valid. Overlapping ranges, including borrowed subviews with different
sample base pointers, are supported by choosing traversal direction, so the
source is read as it was before the call. Overlap detection uses a linear walk
of pointer equality checks, not nonportable pointer ordering. Borrowed subviews
must never be freed as owners; the original allocation must outlive the call.

## Failures and limits

Statuses distinguish invalid API/sample data, malformed WAV framing, unsupported
formats/platforms, resource limits, and allocation failure. Encoding returns
malloc-owned bytes/length, writing neither output argument on failure; free
the returned bytes. All memory sizes and WAV 32-bit sizes/rates are checked for
overflow before allocation. Decode validates the entire container before PCM
allocation. Mutating operations do not allocate and reject invalid parameters
without changing samples.

NULL limits mean at most 10 million **samples** (not frames) and 64 MiB encoded
bytes. Supplied limits are exact, including zero. The sample cap applies to
create, decode and encode; the byte cap applies to encoded input/output. The
sample cap times `sizeof(float)` bounds owned PCM storage. Generator/mixer work
is proportional to caller-requested valid ranges and does not take a separate
work budget. No global state, external I/O, callbacks, or realtime guarantees.

`tests/test_language_tools.c` covers sine values, mixing/clipping and overlap,
PCM16 quantization, PCM8 with odd chunks, every truncation of a valid WAV,
malformed/unsupported formats, empty data, nonfinite samples, overflow/resource
budgets, and allocation failure.

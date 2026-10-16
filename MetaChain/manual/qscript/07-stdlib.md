# 7. Standard library reference

The standard library is 25 QScript libraries embedded in `libQScript`. Import
with `use NAME;` and call functions unqualified or as `NAME::f`. Sources are
in `qscript/stdlib/` (`qscript lib NAME` prints one); each has a test program
in `tests/qscript-stdlib/`.

General rules: arguments and results are int64; arithmetic wraps unless a
function says otherwise; "bool" results are 0 or 1; functions with a stated
precondition `assert` it, so violating it traps with "assertion failed".

**Contents:** [ascii](#ascii) · [bits](#bits) · [checked](#checked) ·
[checksum](#checksum) · [codec](#codec) · [color](#color) · [combin](#combin) ·
[date](#date) · [digits](#digits) · [fixed](#fixed) · [geom](#geom) ·
[hash](#hash) · [logic](#logic) · [math](#math) · [ntheory](#ntheory) ·
[pack](#pack) · [prelude](#prelude) · [prime](#prime) · [random](#random) ·
[range](#range) · [round](#round) · [seq](#seq) · [testing](#testing) ·
[time](#time) · [units](#units)

---

## ascii

Character classification on ASCII codes. Codes outside 0..127 are never
letters, digits or punctuation.

| Function | Result |
|----------|--------|
| `is_digit(c)` | `'0'..'9'` |
| `is_upper(c)`, `is_lower(c)`, `is_alpha(c)`, `is_alnum(c)` | letter tests |
| `is_xdigit(c)` | hexadecimal digit |
| `is_space(c)` | space, `\t \n \v \f \r` |
| `is_print(c)`, `is_cntrl(c)`, `is_punct(c)` | printable 32..126 / control / printable non-alphanumeric non-space |
| `is_vowel(c)` | `aeiou`, either case |
| `to_upper(c)`, `to_lower(c)` | case conversion; other codes unchanged |
| `digit_value(c)` | `c - '0'` for digits, else -1 |
| `hex_value(c)` | 0..15 for hex digits, else -1 |
| `base36_value(c)` | 0..35 for `0-9a-zA-Z`, else -1 |
| `to_digit(d)` | `'0'..'9','a'..'z'` for 0..35 (asserts range) |
| `caesar(c, k)` | rotate letters by k (any sign), others unchanged |
| `rot13(c)` | `caesar(c, 13)` |

## bits

Bit manipulation on 64-bit words; bit 0 is least significant.

| Function | Result |
|----------|--------|
| `popcount(x)` | number of set bits |
| `parity(x)` | `popcount(x) & 1` |
| `clz(x)`, `ctz(x)` | leading / trailing zero bits; 64 for 0 |
| `bit_length(x)` | `64 - clz(x)` |
| `rotl(x, n)`, `rotr(x, n)` | rotate; n taken modulo 64 |
| `sar(x, n)` | arithmetic shift right (n in 0..63) |
| `mask(width)` | low `width` bits set; -1 for width >= 64 |
| `bit(x, i)` | bit i (0 or 1) |
| `set_bit(x, i)`, `clear_bit(x, i)`, `toggle_bit(x, i)` | single-bit updates |
| `extract(x, offset, width)` | bit field |
| `insert(x, value, offset, width)` | x with the field replaced by value |
| `lowbit(x)` | lowest set bit (`x & -x`) |
| `is_pow2(x)` | x > 0 and a power of two |
| `next_pow2(x)` | smallest power of two >= x (x <= 2**62) |
| `bswap16(x)`, `bswap32(x)`, `bswap64(x)` | byte swaps of the low 16/32/64 bits |
| `reverse(x)` | all 64 bits reversed |

## checked

Overflow-aware arithmetic. Constants `INT_MAX`, `INT_MIN`.

| Function | Result |
|----------|--------|
| `add_ok(a, b)`, `sub_ok(a, b)`, `mul_ok(a, b)` | 1 if the operation does not overflow |
| `neg_ok(a)` | `a != INT_MIN` |
| `div_ok(a, b)` | b != 0 and not `INT_MIN / -1` |
| `add_sat`, `sub_sat`, `mul_sat` `(a, b)` | result clamped to `INT_MIN..INT_MAX` |
| `neg_sat(a)`, `abs_sat(a)` | `-INT_MIN` and `abs(INT_MIN)` give `INT_MAX` |
| `add_checked`, `sub_checked`, `mul_checked`, `div_checked` `(a, b)` | exact result, or trap |

## checksum

Error-detecting codes. Byte-stream algorithms fold one byte (low 8 bits) into
a running state. Constants `CRC32_INIT`, `ADLER32_INIT`.

| Function | Result |
|----------|--------|
| `crc32_byte(crc, byte)` | CRC-32 (IEEE, reflected, as in zip/PNG); start from `CRC32_INIT` |
| `crc32_final(crc)` | final CRC-32 value (`"123456789"` gives 3421780262) |
| `crc8_byte(crc, byte)` | CRC-8, polynomial 0x07, start from 0 |
| `adler32_byte(state, byte)` | Adler-32; start from `ADLER32_INIT`; the state is the checksum |
| `fletcher16_byte(state, byte)` | Fletcher-16; start from 0 |
| `luhn_valid(n)` | Luhn check over the decimal digits of n |
| `luhn_digit(n)` | the check digit to append to n |
| `mod97_valid(n)` | ISO 7064 mod 97-10: `n % 97 == 1` |

```qscript
use checksum;
let c = CRC32_INIT;
for (my ch = '1'; ch <= '9'; ch++) c = crc32_byte(c, ch);
return crc32_final(c);    # 3421780262
```

## codec

| Function | Result |
|----------|--------|
| `zigzag(x)`, `unzigzag(z)` | protobuf zigzag encoding of signed integers |
| `gray(x)`, `ungray(g)` | binary reflected Gray code |
| `bcd(n)`, `unbcd(b)` | binary-coded decimal (n >= 0, up to 16 digits) |
| `varint_size(x)` | bytes in the unsigned LEB128 encoding of x |
| `morton(x, y)` | interleave the low 32 bits of x and y (Z-order) |
| `morton_x(m)`, `morton_y(m)` | de-interleave |

## color

Colors packed as `0xRRGGBB`, channels 0..255. Constants `BLACK`, `WHITE`.

| Function | Result |
|----------|--------|
| `rgb(r, g, b)` | packed color; channels clamped to 0..255 |
| `red(c)`, `green(c)`, `blue(c)` | channels |
| `rgba(r, g, b, a)`, `alpha(c)` | `0xRRGGBBAA` packing |
| `clamp_channel(v)` | clamp to 0..255 |
| `luminance(c)` | Rec. 601 luma, 0..255 |
| `gray(c)` | `rgb(l, l, l)` with l the luminance |
| `invert(c)` | complement of every channel |
| `mix(a, b, t)` | blend; t = 0 gives a, 255 gives b |
| `lighten(c, amount)`, `darken(c, amount)` | add / subtract from every channel |
| `distance2(a, b)` | squared RGB distance |

## combin

| Function | Result |
|----------|--------|
| `factorial(n)` | n! (exact up to n = 20) |
| `double_factorial(n)` | n!! |
| `binomial(n, k)` | C(n, k); 0 outside 0 <= k <= n |
| `multichoose(n, k)` | C(n + k - 1, k) |
| `permutations(n, k)` | n! / (n - k)! |
| `rising(x, k)` | x (x+1) ... (x+k-1) |
| `catalan(n)` | Catalan number |
| `fibonacci(n)`, `lucas(n)` | F(n) (exact up to 92), L(n) |
| `derangements(n)` | permutations with no fixed point |
| `stirling2(n, k)` | Stirling numbers of the second kind (recursive; small n) |

## date

Proleptic Gregorian calendar. Dates are `(y, m, d)` with months 1..12, or
packed as the integer `YYYYMMDD`. Days are counted from 1970-01-01. Weekdays
are 0 = Sunday .. 6 = Saturday.

| Function | Result |
|----------|--------|
| `is_leap(y)`, `days_in_year(y)`, `days_in_month(y, m)` | calendar facts |
| `is_valid(y, m, d)` | whether the date exists |
| `days_from_civil(y, m, d)` | days since 1970-01-01 (negative before) |
| `civil(days)` | packed date for a day number (years >= 0) |
| `pack(y, m, d)`, `year(p)`, `month(p)`, `day(p)` | packing |
| `to_days(p)` | day number of a packed date |
| `add_days(p, n)`, `diff_days(p1, p2)` | arithmetic on packed dates |
| `day_of_week(y, m, d)`, `day_of_year(y, m, d)` | weekday 0..6, ordinal 1..366 |
| `easter(y)` | Western Easter Sunday, packed |

```qscript
use date;
say day_of_week(2026, 10, 16);           # 5, a Friday
say add_days(pack(2024, 2, 28), 2);      # 20240301
return diff_days(20240101, 20250101);    # 366
```

## digits

Decimal digits; functions use the absolute value unless noted.

| Function | Result |
|----------|--------|
| `count(n)`, `count_base(n, base)` | number of digits (1 for 0) |
| `sum(n)`, `product(n)` | digit sum / product |
| `reverse(n)` | digits reversed, sign kept |
| `is_palindrome(n)` | n >= 0 and equal to its reverse |
| `digit(n, i)` | i-th digit from the right, starting at 0 |
| `digital_root(n)` | repeated digit sum (n >= 0) |
| `concat(a, b)` | digits of b appended to a (a, b >= 0) |
| `is_armstrong(n)`, `is_harshad(n)` | narcissistic / divisible by digit sum |

## fixed

Q16.16 fixed point: the integer v stands for v / 65536. Constants `SHIFT`
(16), `ONE` (65536), `HALF` (32768). Products are exact up to about 2**47 in
magnitude.

| Function | Result |
|----------|--------|
| `from_int(n)`, `to_int(x)` | conversion (to_int truncates toward zero) |
| `from_ratio(n, d)` | n/d as fixed point |
| `from_milli(m)`, `to_milli(x)` | thousandths, convenient for printing |
| `mul(a, b)`, `div(a, b)`, `recip(x)` | arithmetic, truncating |
| `floor(x)`, `ceil(x)`, `round(x)` | rounding, result in fixed point |
| `frac(x)` | `x - floor(x)` |
| `sqrt(x)` | square root (x >= 0) |
| `lerp(a, b, t)` | a + (b - a) t |

```qscript
use fixed;
let r = from_ratio(3, 2);
say to_milli(mul(r, r));             # 2250, i.e. 2.250
return to_milli(sqrt(from_int(2)));  # 1414
```

## geom

Integer 2-D geometry on separate coordinates.

| Function | Result |
|----------|--------|
| `manhattan(x1, y1, x2, y2)`, `chebyshev(...)` | L1 / L-infinity distance |
| `dist2(x1, y1, x2, y2)`, `dist(...)` | squared distance / floor of the distance |
| `dot(ax, ay, bx, by)`, `cross(ax, ay, bx, by)` | vector products |
| `orient(ax, ay, bx, by, cx, cy)` | 1 counter-clockwise, -1 clockwise, 0 collinear |
| `area2(ax, ay, bx, by, cx, cy)` | twice the triangle area |
| `in_rect(x, y, x0, y0, x1, y1)` | point in closed rectangle (corners in any order) |
| `in_circle(x, y, cx, cy, r)` | point in closed disc |
| `lattice_points(x1, y1, x2, y2)` | integer points on the closed segment |
| `segments_intersect(ax, ay, bx, by, cx, cy, dx, dy)` | closed segments ab and cd intersect |

## hash

Non-cryptographic hashing. Constants `FNV_OFFSET`, `FNV_PRIME`, `DJB2_INIT`.

| Function | Result |
|----------|--------|
| `fnv1a_byte(h, byte)` | FNV-1a 64 step; start from `FNV_OFFSET` |
| `fnv1a(x)` | FNV-1a of the 8 little-endian bytes of x |
| `djb2_byte(h, byte)` | djb2 step; start from `DJB2_INIT` |
| `mix64(x)`, `hash_int(x)` | MurmurHash3 64-bit finalizer |
| `combine(seed, v)` | fold v into a hash (boost style) |
| `hash2(a, b)`, `hash3(a, b, c)` | order-sensitive tuple hashes |
| `jenkins_byte(h, byte)`, `jenkins_final(h)` | Jenkins one-at-a-time, 32-bit |
| `bucket(h, n)` | map a hash to 0..n-1 |

## logic

Any non-zero value is true; results are 0 or 1. (`and`, `or`, `not` are
operators.)

| Function | Result |
|----------|--------|
| `truthy(x)` | `x != 0` |
| `xor`, `nand`, `nor`, `xnor`, `iff`, `implies` `(a, b)` | boolean connectives |
| `select(c, a, b)` | `c ? a : b` (both arguments are evaluated) |
| `majority(a, b, c)`, `count3(a, b, c)` | at least two true / number true |
| `all3`, `any3`, `none3` `(a, b, c)` | quantifiers over three values |
| `cmp(a, b)` | -1, 0 or 1 |

## math

Constants `INT_MAX`, `INT_MIN`.

| Function | Result |
|----------|--------|
| `abs(x)`, `sign(x)` | absolute value (wraps for INT_MIN), -1/0/1 |
| `min`, `max` `(a, b)`; `min3`, `max3` `(a, b, c)` | extrema |
| `clamp(x, lo, hi)` | x limited to lo..hi |
| `square(x)`, `cube(x)`, `pow(base, exp)` | powers (`pow` is `**`) |
| `is_even(x)`, `is_odd(x)` | parity |
| `dist(a, b)` | `|a - b|` |
| `gcd(a, b)`, `lcm(a, b)` | non-negative gcd; lcm (0 if either is 0) |
| `isqrt(n)` | floor(sqrt(n)), n >= 0, exact for all int64 |
| `icbrt(n)` | cube root rounded toward zero |
| `ilog2(n)`, `ilog10(n)` | floor of the logarithm, n > 0 |
| `div_floor(a, b)`, `div_ceil(a, b)` | division rounding down / up |
| `mod_floor(a, b)` | remainder with the sign of b |
| `average(a, b)` | floor((a + b) / 2) without overflow |

## ntheory

Modular arithmetic with positive moduli, overflow-safe for every modulus up to
`INT_MAX`.

| Function | Result |
|----------|--------|
| `mod(a, m)` | a mod m in 0..m-1 |
| `mod_add`, `mod_sub`, `mod_mul` `(a, b, m)` | modular arithmetic |
| `mod_pow(base, exp, m)` | base**exp mod m, exp >= 0 |
| `mod_inv(a, m)` | inverse of a mod m, or 0 if none |
| `is_coprime(a, b)` | gcd is 1 |
| `crt(a1, m1, a2, m2)` | x = a1 (mod m1), x = a2 (mod m2), coprime moduli; -1 otherwise |
| `jacobi(a, n)` | Jacobi symbol for odd n > 0 |
| `is_probable_prime(n)` | deterministic Miller-Rabin; exact for every int64 |

`is_probable_prime(9223372036854775783)` (the largest int64 prime) takes
about 2.8 million VM steps.

## pack

Several small integers in one 64-bit word.

| Function | Result |
|----------|--------|
| `pack32(hi, lo)`, `hi32(x)`, `lo32(x)` | two signed 32-bit halves |
| `sign_extend(x, bits)`, `zero_extend(x, bits)` | widen the low `bits` bits |
| `u8 i8 u16 i16 u32 i32` `(x)` | truncate to a width, unsigned / signed |
| `byte(x, i)`, `with_byte(x, i, b)` | byte i (0 = lowest) |
| `pack16(a, b, c, d)`, `field16(x, i)` | four unsigned 16-bit fields (a highest; field 0 lowest) |
| `pack_bytes(b7, ..., b0)` | eight bytes, most significant first |

## prelude

Constants only: `TRUE` (1), `FALSE` (0), `INT_MAX`, `INT_MIN`, `NEG_ONE`,
`MASK8`, `MASK16`, `MASK32`.

## prime

Trial division: exact for every int64, fast for small numbers.

| Function | Result |
|----------|--------|
| `is_prime(n)` | primality |
| `next_prime(n)`, `prev_prime(n)` | nearest prime above / below (prev needs n > 2) |
| `nth_prime(k)` | k-th prime, `nth_prime(1) == 2` |
| `prime_count(n)` | number of primes <= n |
| `smallest_factor(n)`, `largest_factor(n)` | prime factors, n >= 2 |
| `omega(n)`, `bigomega(n)` | distinct / total prime factors |
| `totient(n)` | Euler's phi |

## random

Deterministic pseudo-random numbers with explicit state (QScript has no
globals). `step` advances a state; `value`, `below`, `range`, `coin` and
`fraction` read one. Constant `GOLDEN`. Not for cryptography.

| Function | Result |
|----------|--------|
| `seed(s)` | initial state from any integer |
| `step(state)` | next SplitMix64 state |
| `value(state)`, `mix(z)` | 64-bit output / the SplitMix64 mixing function |
| `below(state, n)` | uniform in 0..n-1 |
| `range(state, lo, hi)` | uniform in lo..hi |
| `coin(state)` | 0 or 1 |
| `fraction(state)` | Q16.16 value in [0, 1) |
| `xorshift(x)` | Marsaglia xorshift64 step (x != 0) |
| `lcg(x)` | Knuth MMIX LCG step |

```qscript
use random;
let s = seed(2026);
for (my i = 0; i < 3; i++) { s = step(s); say range(s, 1, 6); }
```

## range

Closed integer intervals `[lo, hi]`.

| Function | Result |
|----------|--------|
| `contains(x, lo, hi)`, `length(lo, hi)` | membership, size (0 if empty) |
| `overlaps(a0, a1, b0, b1)`, `overlap(...)` | intersect? / size of the intersection |
| `wrap(x, lo, hi)` | x folded cyclically into the interval |
| `map(x, a0, a1, b0, b1)` | linear map between intervals |
| `lerp(a, b, t, scale)` | a + (b - a) t / scale |
| `sum(a, b)` | a + (a+1) + ... + b |
| `count_multiples(a, b, k)` | multiples of k in the interval |
| `steps(start, stop, step)` | iterations of `for (i = start; i < stop; i += step)` |

## round

| Function | Result |
|----------|--------|
| `trunc_div(a, b)` | `a / b` |
| `round_div(a, b)` | a / b rounded, halves away from zero |
| `round_half_even(a, b)` | a / b rounded, halves to even |
| `floor_to(x, m)`, `ceil_to(x, m)`, `round_to(x, m)`, `trunc_to(x, m)` | to a multiple of m |
| `percent(part, whole)`, `permille(part, whole)` | rounded ratio |
| `scale(x, num, den)` | x * num / den, rounded |

## seq

| Function | Result |
|----------|--------|
| `triangular(n)`, `pentagonal(n)`, `hexagonal(n)` | figurate numbers |
| `is_square(x)`, `is_triangular(x)`, `is_fibonacci(x)` | membership |
| `tribonacci(n)`, `pell(n)` | sequences (T(2) = 1, P(1) = 1) |
| `collatz_steps(n)` | steps to reach 1 |
| `sigma(n)`, `aliquot(n)` | sum of divisors / of proper divisors |
| `is_perfect(n)`, `is_abundant(n)` | classification |
| `arithmetic(a, d, n)`, `arithmetic_sum(a, d, n)` | n-th term (0-based) / sum of n terms |
| `geometric(a, r, n)`, `geometric_sum(a, r, n)` | the same for geometric sequences |
| `is_happy(n)` | happy number test |

## testing

Assertion helpers for test programs. The failing helpers print the
offending values, then trap with "assertion failed".

| Function | Result |
|----------|--------|
| `equal(actual, expected)`, `not_equal(a, b)` | equality checks |
| `less(a, b)`, `between(x, lo, hi)` | ordering checks |
| `is_true(x)`, `is_false(x)` | truth checks |
| `check(passed, condition, code)` | non-fatal: returns passed + 1, or prints code and returns passed |

```qscript
use testing, math;
equal(gcd(12, 18), 6);
let passed = 0;
passed = check(passed, isqrt(17) == 4, 101);
return passed;
```

## time

Seconds and Unix timestamps (seconds since 1970-01-01 UTC). Uses `date`.
Constants `MINUTE`, `HOUR`, `DAY`, `WEEK`.

| Function | Result |
|----------|--------|
| `hms(h, m, s)` | seconds |
| `hours(secs)`, `minutes(secs)`, `seconds(secs)` | components (minutes and seconds 0..59) |
| `clock(secs)` | time of day packed as HHMMSS |
| `unix(y, mo, d, h, mi, s)` | timestamp of a UTC date and time |
| `unix_days(t)`, `time_of_day(t)` | day number and second of day (floor) |
| `unix_date(t)`, `weekday(t)` | packed date, weekday 0..6 |
| `hour12(h)`, `is_pm(h)` | 12-hour clock |
| `to_ms(s)`, `from_ms(ms)` | milliseconds |

## units

Scaled-integer conversions; results truncate toward zero. Constants `KB`,
`MB`, `GB` (powers of 1000) and `KIB`, `MIB`, `GIB` (powers of 1024).

| Function | Units |
|----------|-------|
| `c_to_f`, `f_to_c`, `c_to_k`, `k_to_c` | milli-degrees (37000 = 37.000 °C) |
| `inches_to_mm`, `feet_to_mm`, `mm_to_inches` | inches / feet / mm |
| `miles_to_m`, `m_to_miles`, `km_to_m` | miles / metres / km |
| `pounds_to_g`, `g_to_pounds`, `ounces_to_g` | pounds / ounces / grams |
| `deg_to_mrad`, `mrad_to_deg` | degrees / milliradians |
| `kib(n)`, `mib(n)`, `gib(n)` | binary sizes to bytes |
| `to_kib(bytes)`, `to_mib(bytes)` | bytes to KiB / MiB, rounding up |

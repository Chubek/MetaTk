# QScript standard library

Each `*.qs` file here is a library: it contains only `fn`, `const` and `use`
declarations. The files are embedded into the QScript library at build time,
so `use NAME;` works without any files on disk. Names starting with `_` are
private to their library.

```qscript
use math, prime;
print gcd(84, 36), next_prime(100);
return math::isqrt(1000);
```

| Library | Contents |
|---------|----------|
| `ascii` | character classes and case conversion on ASCII codes (`is_digit`, `to_upper`, `hex_value`, `rot13`, ...) |
| `bits` | `popcount`, `clz`, `ctz`, rotations, arithmetic shift `sar`, bit fields, byte swaps, bit reversal |
| `checked` | overflow tests (`add_ok`, `mul_ok`, ...), saturating (`add_sat`, ...) and trapping (`add_checked`, ...) arithmetic |
| `checksum` | CRC-32, CRC-8, Adler-32, Fletcher-16 (byte at a time), Luhn, mod-97 |
| `codec` | zigzag, Gray code, BCD, LEB128 size, Morton (Z-order) codes |
| `color` | packed `0xRRGGBB` colors: channels, luminance, mixing, lighten/darken |
| `combin` | factorial, binomial, permutations, Catalan, Fibonacci, Lucas, derangements, Stirling |
| `date` | Gregorian calendar: leap years, days since 1970, weekday, day of year, Easter |
| `digits` | decimal digit count, sum, reverse, palindromes, digital root, Armstrong/Harshad |
| `fixed` | Q16.16 fixed point: `from_ratio`, `mul`, `div`, `sqrt`, `floor`/`ceil`/`round` |
| `geom` | integer 2-D geometry: distances, dot/cross, orientation, segment intersection |
| `hash` | FNV-1a, djb2, Jenkins one-at-a-time, Murmur3 finalizer, hash combining |
| `logic` | `xor`, `nand`, `implies`, `majority`, `select`, three-way `cmp` |
| `math` | `abs`, `min`/`max`/`clamp`, `gcd`/`lcm`, `isqrt`/`icbrt`, `ilog2`, floor/ceil division |
| `ntheory` | overflow-safe `mod_mul`/`mod_pow`, `mod_inv`, CRT, Jacobi, deterministic Miller-Rabin |
| `pack` | several small integers in one word: `pack32`, `hi32`/`lo32`, sign/zero extension, bytes |
| `prelude` | constants `TRUE`, `FALSE`, `INT_MAX`, `INT_MIN`, masks |
| `prime` | primality by trial division, next/previous/nth prime, factors, totient |
| `random` | SplitMix64 with explicit state (`seed`, `step`, `range`), xorshift, LCG |
| `range` | closed intervals: containment, overlap, wrapping, linear maps, sums |
| `round` | rounding division (half away from zero, half even), rounding to multiples, percentages |
| `seq` | figurate numbers, `is_square`, Collatz, divisor sums, perfect/happy numbers |
| `testing` | assertion helpers (`equal`, `between`, ...) and a non-fatal `check` counter |
| `time` | seconds/clock conversions and Unix timestamps (uses `date`) |
| `units` | temperature, length, mass, angle and data-size conversions in scaled integers |

Every library has a self-checking test program in `tests/qscript-stdlib/`,
run by `ctest`. The full reference, function by function, is in
[the QScript manual](../../manual/qscript/07-stdlib.md).

```sh
build/qscript libs            # list
build/qscript lib date        # print a library's source
build/qscript -L mylibs run program.qs   # also search mylibs/NAME.qs
```

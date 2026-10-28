# 12. Integers, operators, and overflow

Correct backend rules begin with the source arithmetic contract. Juniper's
program values are signed 64-bit integers, not C++ `int`, arbitrary-precision
integers, floating-point values, or automatically typed register widths.

## Arithmetic wraps where specified

<!-- check: juniper no-preprocess -->
```juniper
module integer_edges;
fn wrapped_maximum() {
  return add(9223372036854775807, 1);
}
fn negative_one_bits() {
  return 0xffffffffffffffff;
}
fn minimum_literal() {
  return -9223372036854775808;
}
```

Addition, subtraction, multiplication, and negation wrap modulo 2^64. Therefore
the first and third functions have the signed-minimum bit pattern, and the second
returns `-1`. These are semantic explanations, not evidence that Juniper provides
a function executor. Inspect `ConstantFolding` output to see the constant result.

```sh
"$J" optimize "$WORK/integer-edges.junir" --no-preprocess \
  --passes ConstantFolding -o "$WORK/folded-edges.junir"
"$J" metail "$WORK/folded-edges.junir" --no-preprocess
```

Save the preceding raw-IR example as `integer-edges.junir` first.

## Operator families

| Family | Canonical names | Infix/prefix spelling |
|---|---|---|
| Arithmetic | `add sub mul div mod neg` | `+ - * / % -x` |
| Bitwise | `band bor bxor bnot` | `& \| ^ ~x` |
| Shifts | `shl shr` | `<< >>` |
| Comparisons | `eq ne lt le gt ge` | `== != < <= > >=` |
| Logical | `land lor lnot` | `&& \|\| !x` |
| Conditional | `select` | `select(c, yes, no)` |

Comparisons and logical operators return `0` or `1`; conditions treat any nonzero
value as true. Comparisons are signed. `shr` is arithmetic right shift; `shl`
wraps for valid counts. Counts outside `0..63` are possible traps and are not
folded as though they had been masked into range.

<!-- check: juniper -->
```juniper
@fn(low_byte, (x))
  @return(band(x, 255))
@end
@fn(sign_and_low_bit, (x))
  @return(bor(shl(lt(x, 0), 1), band(x, 1)))
@end
```

Division and remainder truncate toward zero. Division by zero and the
signed-minimum/`-1` case remain possible traps. Do not replace signed division by
a power of two with arithmetic right shift without proving that their rounding
behavior agrees for the reachable values.

## Writing reference calculations safely

If your C++ tests model wrapping addition, do not use overflowing signed C++
arithmetic. A small reference fragment is:

```cpp
#include <bit>
#include <cstdint>

std::int64_t wrapping_add(std::int64_t a, std::int64_t b) {
    const auto bits = static_cast<std::uint64_t>(a) +
                      static_cast<std::uint64_t>(b);
    return std::bit_cast<std::int64_t>(bits);
}
```

Model traps and evaluation effects separately from the integer result. Matching
only the final number is insufficient for a transformation that might drop a call
or move division past another observable operation.

## Current unary-formatting corner and its workaround

The current MetaIL emitter can print negation of a negative literal with adjacent
`--` characters, such as `(--9223372036854775808)`. The parser interprets `--` as
a line comment, so that unoptimized emitted spelling does not round-trip. This
is a formatting limitation, not a different arithmetic contract.

Constant-fold the expression **before** emitting it as a saved interchange file:

<!-- check: juniper mode=optimize passes=ConstantFolding -->
```juniper
fn negated_minimum() {
  return neg(-9223372036854775808);
}
```

```sh
"$J" optimize "$WORK/negative-negation.jun" --passes ConstantFolding \
  -o "$WORK/negative-negation.folded.junir"
```

Save that block as `negative-negation.jun`. Alternatively, write the wrapping
equivalent `sub(0, -9223372036854775808)`, which does not emit adjacent unary
minus characters. Do not treat an unchecked raw emission of this edge case as
a valid canonical archive; test round trips for your actual program shapes.

[Previous](11-functions-and-calls.md) · [Index](README.md) · [Next](13-variables-and-statements.md)

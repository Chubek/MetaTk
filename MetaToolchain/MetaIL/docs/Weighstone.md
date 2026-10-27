# Weighstone-backed integer optimization

MetaIL uses the actual C++20 Weighstone headers through `lua/weighstone.cpp`.
`BitVector`, `KnownBits`, `Interval`, `Congruence`, linear-constraint `Verdict`
queries and modular inverses supply executable reasoning. Procedural rewrites
live in `lua/metail/transform.lua`; shared analysis lives in
`lua/metail/analysis.lua`. The public C/C++ API is unchanged.

## Build and use

```sh
cmake -S . -B build -DWEIGHSTONE_ROOT=../weighstone
cmake --build build
ctest --test-dir build --output-on-failure
build/metail verify
python3 tools/generate_kernels.py --check
build/metail optimize examples/weighstone.mi
build/metail analyze examples/weighstone.mi --passes WeighstoneIntegerFacts
build/metail kernels --tag weighstone --paths
```

Weighstone and Boost.Multiprecision are header-only, private build dependencies.
The embedded CLI and C/C++ runtime register `weighstone_native` themselves.
Installation also supplies a standalone Lua 5.4 `weighstone_native` module.
All native calculations finish and destroy C++ owning objects before calling
back into Lua, including error reporting, so Lua longjmp cannot bypass them.

## Optimization families

All five passes run in the default pipeline and can also be selected with
`--passes`. Each supports computed facts from preceding assignments in the same
basic block. A local application budget of four steps per expression bounds
cascading rewrites; the ordinary pipeline round limit still applies.

| Pass | Implemented transformations |
|---|---|
| `WeighstoneKnownBits` | Fold proven constants; eliminate redundant AND/OR masks on either side; eliminate zero XOR operands; propagate low-bit residues through add/subtract/negate/multiply; prove masked shifts and aligned arithmetic constant |
| `WeighstoneRanges` | Fold all six comparisons with separated/singleton ranges or conflicting bits; select a proven arm; simplify eager logical operators and boolean equality/inequality; replace safe division by -1 with negation; eliminate bounded remainders; fold zero quotients and known residues |
| `WeighstoneStrengthReduction` | Multiply by any signed power of two, including bit 63, on either side; divide by positive/negative powers of two for nonnegative or exactly divisible inputs; replace nonnegative power-of-two remainders with masks; eliminate proven-zero aligned remainders |
| `WeighstoneShifts` | Merge arbitrary valid nested left/right shifts; fold oversized composed left shifts to zero when discardable; saturate composed arithmetic right shifts at 63; replace right/left round trips with masks; remove left/right round trips only for representable ranges; remove sign-extension round trips under a low mask |
| `WeighstoneAlgebra` | Combine nested add/multiply/AND/OR/XOR constants in either position; combine mixed add/subtract constants; invert negated comparisons; move add/subtract/XOR constants through equality/inequality; invert negation/complement and odd constant multiplication in modular equalities; combine positive constant division chains without overflow; combine divisible positive remainder chains; collect equal scaled terms under purity guards |

Examples (each uses mathematical proof adapted to MetaIL's machine semantics):

```text
((x * 8) + 3) & 7          -> 3
(x & 511) < 512            -> 1
(x & 1023) / 32            -> (x & 1023) >> 5
(x << 5) / 32              -> (x << 5) >> 5
(x * 3) == 9               -> x == 3
(x >> 51) >> 27            -> x >> 63
(x % 21) % 7               -> x % 7
```

Counts describe procedural families, not thousands of independently registered
rules. There are six new generated declarations: five transformations and one
analysis, bringing the catalogue to 327. All 321 prior declarations are unchanged.

## Proof contracts

* **Wraparound:** mathematical arithmetic intervals are mapped modulo 2^64.
  If the range crosses a signed wrap boundary, the interval becomes the full
  signed range. Independently valid known bits can recover useful precision.
* **Congruences:** a known low prefix represents a power-of-two congruence.
  Arithmetic combines residues; `gcd(modulus, 2^64)` retains only facts invariant
  under wrapping. Congruences do not assert sign or non-overflow.
* **Comparisons:** Weighstone constraint queries return `proven`, `disproven`
  (false everywhere), or `unknown`. Unknown remains the boolean range `[0,1]`.
  Comparison differences are mathematical integers, never wrapped subtractions.
* **Division:** truncates toward zero. Zero divisors and `MIN/-1` (including
  remainder) retain traps. A negative dividend only permits arithmetic-shift
  division when known low bits establish exact divisibility.
* **Shifts:** each original count must be in `[0,63]`. Weighstone's totalized
  out-of-width shift semantics are never substituted for MetaIL's traps.
* **Effects:** value facts hold on successful evaluation. A known result does
  not permit dropping calls or traps: `(tick() & 0)` remains effectful. Rules
  preserve evaluation count/order or require discardable operands. Logical
  AND/OR remain eager; only `select` skips the unchosen arm.
* **Control flow:** facts propagate through sequential assignments and reset
  at labels and terminators. No assumptions about loop induction, predecessor
  ranges, `assume` statements, SSA, memory, or aliasing are made.

## Analysis output

`WeighstoneIntegerFacts` returns a per-function list with statement index,
formatted expression, signed `min`/`max`, `known_zero`, `known_one`, and
`discardable`. Masks are signed-64 decimal bit patterns (a negative mask has
bit 63 set). A previous assignment's value can be total to read even if computing
that assignment had effects. Existing `LiteralValueRange` remains a separate
syntactic analysis.

The low-level Lua module is an internal adapter. `transfer(op, lhs, rhs)` consumes
fact tables (`lo`, `hi`, `zero`, `one`); `nil` means unknown. It returns the same
fields and `total`, which describes that operator's trap behavior, not subtree
purity. `inverse(integer)` returns its inverse modulo 2^64 or `nil` when no
inverse exists. Malformed inputs raise errors. The analyzer, rather than callers
of the internal module, supplies subtree purity and assignment environments.

## Verification

`tests/test_weighstone.lua` requires positive rewrites to fire and checks
boundary and unknown/unsafe examples, all valid shift widths, deterministic
random expression trees, effect traces, trap outcomes, mutable-local invalidation,
and inferred ranges/bits against the separate C signed-64 evaluator. It also
compares `examples/weighstone.mi` against the original two-round optimizer,
including cleanup. This reports IR operation counts, not machine-speed claims.
C API and standalone Lua tests exercise module registration and discovery.
See [verification results](Verification.md) for compiler configurations and timings.

# MetaNum binding and numeric kernels

## Build and load

```sh
cmake -S . -B build -DMETANUM_ROOT=/mnt/nvme/metatk/MetaNum
cmake --build build
build/metail lua examples/numeric.lua
```

The C++17 bridge includes MetaNum's actual `all.hpp`/`numeric.hpp`. It builds into
the host and `build/metanum_native.so`. The public Lua facade is `lua/metanum.lua`.
For an ordinary Lua 5.4 interpreter from the source directory:

```sh
LUA_PATH='./lua/?.lua;;' LUA_CPATH='./build/?.so;;' lua5.4 examples/numeric.lua
```

Install with `cmake --install build --prefix PREFIX`. The module goes to
`PREFIX/lib/lua/5.4`, its facade to `PREFIX/share/lua/5.4`, and the host's Lua
files/kernels to `PREFIX/share/metail`. Configure Lua's `LUA_PATH`/`LUA_CPATH` if
PREFIX is outside its default search paths. MetaNum headers are needed only at
build time. The Lua ABI shim assumes standard Lua 5.4 with 64-bit integers and
double-precision numbers.

## Lua API

`local num = require('metanum')` or `require('lkopt').numeric`:

| Function | Meaning |
|---|---|
| `mean(x)` | Arithmetic mean |
| `variance(x, sample=true)` | Sample (`n-1`) or population (`n`) variance |
| `standard_deviation(x, sample=true)` | Sample/population standard deviation |
| `covariance(x, y, sample=true)` | Covariance of equally sized arrays |
| `correlation(x, y)` | Pearson correlation |
| `median(x)` | Middle value, or mean of two middle values |
| `quantile(x, p)` | Type-7 interpolated quantile, `0 <= p <= 1` |
| `min(x)`, `max(x)` | Extremes, with NaN propagation |
| `catalog()` | Fresh table of 40 categories and 298 operation descriptors |
| `max_samples` | Per-array limit: 1,000,000 |

Arrays must be dense, 1-based Lua tables containing numbers only. Sparse arrays,
extra keys, numeric strings, and non-boolean `sample` options are rejected. Arrays
are copied to Lua-owned scratch memory and never modified. Integers are converted
to binary64; values beyond 2^53 can lose low bits. Floating values, infinities, and
NaNs follow MetaNum's contract. Sample estimates/correlation require at least two
values; population estimates and other statistics require at least one.
Correlation of constant finite data is a domain error.

Input and native exceptions become catchable Lua errors (`pcall`). C++ exception
objects and scratch vectors are destroyed before raising a Lua error. Temporary
input buffers remain owned by Lua's garbage collector, including on errors.

Each catalog category contains `id`, `library`, `description`, `callable=false`,
and `operations` with `name`, `symbols`, and `description`. These are descriptors
for upstream GSL/ISL/PPL APIs, not executable bindings to those libraries.

## The 120 added kernels

All are generated in `optimize-kernels/numeric/`. The directory name follows the
existing project layout. Select each by its registered name via `--passes`.

### 24 MetaNum-backed analyses

Names are `Numeric{Source}{Statistic}` with two sources:

* `Literals`: every integer-literal occurrence in each function's expression ASTs.
* `ExpressionSizes`: node count of each statement's root expression.

And twelve statistics:

`Mean`, `SampleVariance`, `PopulationVariance`, `SampleStdDev`, `PopulationStdDev`,
`Median`, `Minimum`, `Maximum`, `Quantile10`, `Quantile25`, `Quantile75`, `Quantile90`.

These are syntactic descriptive statistics, including unreachable statements and
unevaluated select arms. They do not infer runtime value distributions or ranges.
Reports are keyed by function and contain `count`, `source`, `statistic`,
`precision="float64"`, `status`, and (for `status="ok"`) `value`. Other statuses
are `insufficient-data`, `nan`, `positive-infinity`, and `negative-infinity`.
JSON numbers retain binary64 round-trip precision; non-finite values are never
emitted as invalid JSON. Reports do not modify IR or participate in rewrite guards.

### 96 signed-64 numeric rewrites

24 general rules, all prefixed `Numeric`:

* `MultiplyMinusOneRight`, `MultiplyMinusOneLeft`, `MultiplyNegatedBoth`
* `AddComplementRight`, `AddComplementLeft`, `NegateIncrement`,
  `ComplementIncrement`, `NegatedMinusOne`, `SubtractNegatedSelf`, `DoubleAsShift`
* `FactorAddRight`, `FactorAddLeft`, `FactorSubtractRight`, `FactorSubtractLeft`
* `CancelNegatedProductLeft`, `CancelNegatedProductRight`
* `CompareComplementLT`, `CompareComplementLE`, `CompareComplementGT`, `CompareComplementGE`
* `XorComplements`, `AndContradiction`, `OrTautology`, `XorCancelOuter`

72 width-specialized rules: the following twelve families, prefixed `Numeric`
and suffixed with each width `08`, `16`, `24`, `32`, `40`, `48`:

* `MaskIdempotence`, `MaskOrSaturation`, `DisjointMasks`
* `MaskedLeftShiftVanish`, `MaskedRightShiftVanish`
* `MaskedDivision`, `MaskedRemainder`, `MaskedModuloIdentity`
* `MaskedShiftRoundTrip`, `ShiftRoundTripMask`
* `MultiplyNegativePowerRight`, `MultiplyNegativePowerLeft`

Masks prove nonnegativity before division/remainder strength reduction. Arithmetic
identities use modulo-2^64 semantics, never floating-point approximations.
Purity guards protect discarded or repeated computations, including traps.
Unknown function calls retain their evaluation order. All 96 added rewrites and
the original 134 participate in the same mandatory equivalence verifier.

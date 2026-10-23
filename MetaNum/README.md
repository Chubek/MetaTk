# MetaNum headers

`numeric.hpp` provides portable, C++17 header-only implementations of
mean, sample/population variance, standard deviation, covariance, correlation,
median, interpolated quantile, minimum, and maximum.

The manifest also names 298 operations from ISL, GSL, and PPL. The 40 category
headers describe these APIs as compile-time catalogs. To invoke those operations,
include the upstream headers and link the corresponding libraries. Catalog entries
preserve operation names and descriptions; repeated native symbols are deduplicated.
Every category independently validates its fields, symbol lists, metadata, and
storage at compile time. Catalog views are non-owning; user-supplied strings and
operation arrays must outlive their views.

Top-level include: `#include "all.hpp"` (add this directory to your include path).
Individual headers may also be included independently.

Numeric inputs are never modified. Invalid input shapes or probabilities throw
`std::invalid_argument`; correlation of constant finite data throws
`std::domain_error`. NaNs propagate. Variance, covariance, standard deviation, and
correlation use power-of-two scaling and compensated sums. Standard deviation is
computed before converting to `double`, so a representable result is preserved
even when its variance would overflow or underflow `double`.

See `tests/README.txt` for build and regeneration commands. Changes to generated
headers belong in `tools/gen_catalogs.py` or `MANIFEST.yaml`.

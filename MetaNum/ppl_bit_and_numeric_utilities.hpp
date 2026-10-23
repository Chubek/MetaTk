// MetaNum catalog: ppl.bit_and_numeric_utilities
// Auxiliary numeric utilities.
//
// GENERATED from MANIFEST.yaml by gen_catalogs.py - edit the manifest, not this file.
// Describes the native PPL API (Parma Polyhedra Library); it does not reimplement or link it.
// Symbol names follow the manifest and may differ across PPL versions; check
// the installed upstream headers before relying on an exact symbol.
#ifndef METANUM_PPL_BIT_AND_NUMERIC_UTILITIES_HPP
#define METANUM_PPL_BIT_AND_NUMERIC_UTILITIES_HPP

#include "catalog_core.hpp"

namespace metanum::catalog {

inline constexpr std::array<operation, 2> ppl_bit_and_numeric_utilities = {{
    operation{"exact_compare", "cmp", "Exact comparison of coefficients."},
    operation{"stride_check", "stride", "Stride of a dimension in a grid."},
}};

inline constexpr category ppl_bit_and_numeric_utilities_category{
    "ppl.bit_and_numeric_utilities", library::ppl, "Auxiliary numeric utilities.", ppl_bit_and_numeric_utilities.data(), ppl_bit_and_numeric_utilities.size()};

static_assert(detail::fields_well_formed(ppl_bit_and_numeric_utilities), "ppl_bit_and_numeric_utilities: empty or malformed name/symbol/description");
static_assert(detail::names_unique(ppl_bit_and_numeric_utilities), "ppl_bit_and_numeric_utilities: duplicate operation name");
static_assert(detail::symbols_well_formed(ppl_bit_and_numeric_utilities), "ppl_bit_and_numeric_utilities: malformed symbol list");
static_assert(detail::symbols_unique_per_operation(ppl_bit_and_numeric_utilities), "ppl_bit_and_numeric_utilities: symbol listed twice in one operation");
static_assert(detail::category_metadata_well_formed(ppl_bit_and_numeric_utilities_category), "ppl_bit_and_numeric_utilities: malformed category metadata");
static_assert(ppl_bit_and_numeric_utilities_category.begin() == ppl_bit_and_numeric_utilities.data() && ppl_bit_and_numeric_utilities_category.size() == ppl_bit_and_numeric_utilities.size(),
              "ppl_bit_and_numeric_utilities: category storage mismatch");
static_assert(!ppl_bit_and_numeric_utilities.empty(), "ppl_bit_and_numeric_utilities: empty catalog");

}  // namespace metanum::catalog

#endif  // METANUM_PPL_BIT_AND_NUMERIC_UTILITIES_HPP

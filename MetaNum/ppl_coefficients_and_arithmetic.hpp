// MetaNum catalog: ppl.coefficients_and_arithmetic
// Coefficient types and arbitrary precision arithmetic.
//
// GENERATED from MANIFEST.yaml by gen_catalogs.py - edit the manifest, not this file.
// Describes the native PPL API (Parma Polyhedra Library); it does not reimplement or link it.
// Symbol names follow the manifest and may differ across PPL versions; check
// the installed upstream headers before relying on an exact symbol.
#ifndef METANUM_PPL_COEFFICIENTS_AND_ARITHMETIC_HPP
#define METANUM_PPL_COEFFICIENTS_AND_ARITHMETIC_HPP

#include "catalog_core.hpp"

namespace metanum::catalog {

inline constexpr std::array<operation, 7> ppl_coefficients_and_arithmetic = {{
    operation{"coefficient_types", "Coefficient, mpz_class, mpq_class", "Integer and rational coefficient types."},
    operation{"numerator_denominator", "numer_denom", "Extract numerator and denominator."},
    operation{"gcd_lcm", "gcd, lcm", "Greatest common divisor and least common multiple."},
    operation{"integer_division", "exact_div, div_mod, div_exact", "Exact and quotient-remainder integer division."},
    operation{"rounding", "rounding modes (ROUND_IGNORE, ROUND_DOWN, ROUND_UP, ROUND_NEAREST)", "Rounding modes for floating-point and rational results."},
    operation{"modular_arithmetic", "mod_gcd, mod_inverse", "Modular operations."},
    operation{"floor_ceil", "floor_log2, ceil_log2", "Floor and ceiling logarithms."},
}};

inline constexpr category ppl_coefficients_and_arithmetic_category{
    "ppl.coefficients_and_arithmetic", library::ppl, "Coefficient types and arbitrary precision arithmetic.", ppl_coefficients_and_arithmetic.data(), ppl_coefficients_and_arithmetic.size()};

static_assert(detail::fields_well_formed(ppl_coefficients_and_arithmetic), "ppl_coefficients_and_arithmetic: empty or malformed name/symbol/description");
static_assert(detail::names_unique(ppl_coefficients_and_arithmetic), "ppl_coefficients_and_arithmetic: duplicate operation name");
static_assert(detail::symbols_well_formed(ppl_coefficients_and_arithmetic), "ppl_coefficients_and_arithmetic: malformed symbol list");
static_assert(detail::symbols_unique_per_operation(ppl_coefficients_and_arithmetic), "ppl_coefficients_and_arithmetic: symbol listed twice in one operation");
static_assert(detail::category_metadata_well_formed(ppl_coefficients_and_arithmetic_category), "ppl_coefficients_and_arithmetic: malformed category metadata");
static_assert(ppl_coefficients_and_arithmetic_category.begin() == ppl_coefficients_and_arithmetic.data() && ppl_coefficients_and_arithmetic_category.size() == ppl_coefficients_and_arithmetic.size(),
              "ppl_coefficients_and_arithmetic: category storage mismatch");
static_assert(!ppl_coefficients_and_arithmetic.empty(), "ppl_coefficients_and_arithmetic: empty catalog");

}  // namespace metanum::catalog

#endif  // METANUM_PPL_COEFFICIENTS_AND_ARITHMETIC_HPP

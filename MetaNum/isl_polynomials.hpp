// MetaNum catalog: isl.polynomials
// Piecewise quasi-polynomials for counting and summation.
//
// GENERATED from MANIFEST.yaml by gen_catalogs.py - edit the manifest, not this file.
// Describes the native ISL API (Integer Set Library); it does not reimplement or link it.
// Symbol names follow the manifest and may differ across ISL versions; check
// the installed upstream headers before relying on an exact symbol.
#ifndef METANUM_ISL_POLYNOMIALS_HPP
#define METANUM_ISL_POLYNOMIALS_HPP

#include "catalog_core.hpp"

namespace metanum::catalog {

inline constexpr std::array<operation, 7> isl_polynomials = {{
    operation{"qpolynomial_add", "isl_qpolynomial_add", "Sum of quasi-polynomials."},
    operation{"qpolynomial_mul", "isl_qpolynomial_mul", "Product of quasi-polynomials."},
    operation{"qpolynomial_pow", "isl_qpolynomial_pow", "Power of a quasi-polynomial."},
    operation{"pw_qpolynomial_sum", "isl_pw_qpolynomial_sum", "Sum over a domain (summation of a polynomial over integer points)."},
    operation{"pw_qpolynomial_fold", "isl_pw_qpolynomial_fold", "Fold (max, min, sum) of piecewise quasi-polynomials."},
    operation{"pw_qpolynomial_bound", "isl_pw_qpolynomial_bound", "Upper or lower bounds of quasi-polynomials."},
    operation{"pw_qpolynomial_eval", "isl_pw_qpolynomial_eval", "Evaluate at a point."},
}};

inline constexpr category isl_polynomials_category{
    "isl.polynomials", library::isl, "Piecewise quasi-polynomials for counting and summation.", isl_polynomials.data(), isl_polynomials.size()};

static_assert(detail::fields_well_formed(isl_polynomials), "isl_polynomials: empty or malformed name/symbol/description");
static_assert(detail::names_unique(isl_polynomials), "isl_polynomials: duplicate operation name");
static_assert(detail::symbols_well_formed(isl_polynomials), "isl_polynomials: malformed symbol list");
static_assert(detail::symbols_unique_per_operation(isl_polynomials), "isl_polynomials: symbol listed twice in one operation");
static_assert(detail::category_metadata_well_formed(isl_polynomials_category), "isl_polynomials: malformed category metadata");
static_assert(isl_polynomials_category.begin() == isl_polynomials.data() && isl_polynomials_category.size() == isl_polynomials.size(),
              "isl_polynomials: category storage mismatch");
static_assert(!isl_polynomials.empty(), "isl_polynomials: empty catalog");

}  // namespace metanum::catalog

#endif  // METANUM_ISL_POLYNOMIALS_HPP

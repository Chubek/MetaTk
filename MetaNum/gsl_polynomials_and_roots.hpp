// MetaNum catalog: gsl.polynomials_and_roots
// Polynomial evaluation and root finding.
//
// GENERATED from MANIFEST.yaml by gen_catalogs.py - edit the manifest, not this file.
// Describes the native GSL API (GNU Scientific Library); it does not reimplement or link it.
// Symbol names follow the manifest and may differ across GSL versions; check
// the installed upstream headers before relying on an exact symbol.
#ifndef METANUM_GSL_POLYNOMIALS_AND_ROOTS_HPP
#define METANUM_GSL_POLYNOMIALS_AND_ROOTS_HPP

#include "catalog_core.hpp"

namespace metanum::catalog {

inline constexpr std::array<operation, 5> gsl_polynomials_and_roots = {{
    operation{"poly_eval", "gsl_poly_eval, gsl_poly_eval_derivs", "Evaluate polynomial and derivatives."},
    operation{"poly_complex_eval", "gsl_poly_complex_eval", "Evaluate polynomial at a complex point."},
    operation{"poly_solve", "gsl_poly_solve_quadratic, gsl_poly_solve_cubic, gsl_poly_complex_solve", "Closed-form and general polynomial roots."},
    operation{"poly_dd", "gsl_poly_dd_init, gsl_poly_dd_eval, gsl_poly_dd_taylor", "Divided differences and Taylor coefficients."},
    operation{"poly_pqr", "gsl_poly_complex_workspace_alloc", "Workspace for companion-matrix root finding."},
}};

inline constexpr category gsl_polynomials_and_roots_category{
    "gsl.polynomials_and_roots", library::gsl, "Polynomial evaluation and root finding.", gsl_polynomials_and_roots.data(), gsl_polynomials_and_roots.size()};

static_assert(detail::fields_well_formed(gsl_polynomials_and_roots), "gsl_polynomials_and_roots: empty or malformed name/symbol/description");
static_assert(detail::names_unique(gsl_polynomials_and_roots), "gsl_polynomials_and_roots: duplicate operation name");
static_assert(detail::symbols_well_formed(gsl_polynomials_and_roots), "gsl_polynomials_and_roots: malformed symbol list");
static_assert(detail::symbols_unique_per_operation(gsl_polynomials_and_roots), "gsl_polynomials_and_roots: symbol listed twice in one operation");
static_assert(detail::category_metadata_well_formed(gsl_polynomials_and_roots_category), "gsl_polynomials_and_roots: malformed category metadata");
static_assert(gsl_polynomials_and_roots_category.begin() == gsl_polynomials_and_roots.data() && gsl_polynomials_and_roots_category.size() == gsl_polynomials_and_roots.size(),
              "gsl_polynomials_and_roots: category storage mismatch");
static_assert(!gsl_polynomials_and_roots.empty(), "gsl_polynomials_and_roots: empty catalog");

}  // namespace metanum::catalog

#endif  // METANUM_GSL_POLYNOMIALS_AND_ROOTS_HPP

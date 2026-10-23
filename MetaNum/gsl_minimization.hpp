// MetaNum catalog: gsl.minimization
// Function minimization.
//
// GENERATED from MANIFEST.yaml by gen_catalogs.py - edit the manifest, not this file.
// Describes the native GSL API (GNU Scientific Library); it does not reimplement or link it.
// Symbol names follow the manifest and may differ across GSL versions; check
// the installed upstream headers before relying on an exact symbol.
#ifndef METANUM_GSL_MINIMIZATION_HPP
#define METANUM_GSL_MINIMIZATION_HPP

#include "catalog_core.hpp"

namespace metanum::catalog {

inline constexpr std::array<operation, 4> gsl_minimization = {{
    operation{"min_1d", "gsl_min_fminimizer_goldensection, gsl_min_fminimizer_brent, gsl_min_fminimizer_quad_golden", "One-dimensional minimizers."},
    operation{"min_multidim_no_deriv", "gsl_multimin_fminimizer_nmsimplex, gsl_multimin_fminimizer_nmsimplex2, gsl_multimin_fminimizer_nmsimplex2rand", "Nelder-Mead simplex methods."},
    operation{"min_multidim_deriv", "gsl_multimin_fdfminimizer_conjugate_fr, gsl_multimin_fdfminimizer_conjugate_pr, gsl_multimin_fdfminimizer_vector_bfgs, gsl_multimin_fdfminimizer_vector_bfgs2, gsl_multimin_fdfminimizer_steepest_descent", "Gradient-based minimizers (conjugate gradient, BFGS, steepest descent)."},
    // Manifest repeats gsl_multifit_fdfsolver_lmsder; listed once here.
    operation{"nonlinear_least_squares", "gsl_multifit_fdfsolver_lmsder, gsl_multifit_fdfsolver_lmder", "Levenberg-Marquardt for nonlinear least squares."},
}};

inline constexpr category gsl_minimization_category{
    "gsl.minimization", library::gsl, "Function minimization.", gsl_minimization.data(), gsl_minimization.size()};

static_assert(detail::fields_well_formed(gsl_minimization), "gsl_minimization: empty or malformed name/symbol/description");
static_assert(detail::names_unique(gsl_minimization), "gsl_minimization: duplicate operation name");
static_assert(detail::symbols_well_formed(gsl_minimization), "gsl_minimization: malformed symbol list");
static_assert(detail::symbols_unique_per_operation(gsl_minimization), "gsl_minimization: symbol listed twice in one operation");
static_assert(detail::category_metadata_well_formed(gsl_minimization_category), "gsl_minimization: malformed category metadata");
static_assert(gsl_minimization_category.begin() == gsl_minimization.data() && gsl_minimization_category.size() == gsl_minimization.size(),
              "gsl_minimization: category storage mismatch");
static_assert(!gsl_minimization.empty(), "gsl_minimization: empty catalog");

}  // namespace metanum::catalog

#endif  // METANUM_GSL_MINIMIZATION_HPP

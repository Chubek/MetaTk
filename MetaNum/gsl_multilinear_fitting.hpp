// MetaNum catalog: gsl.multilinear_fitting
// Least-squares and fitting.
//
// GENERATED from MANIFEST.yaml by gen_catalogs.py - edit the manifest, not this file.
// Describes the native GSL API (GNU Scientific Library); it does not reimplement or link it.
// Symbol names follow the manifest and may differ across GSL versions; check
// the installed upstream headers before relying on an exact symbol.
#ifndef METANUM_GSL_MULTILINEAR_FITTING_HPP
#define METANUM_GSL_MULTILINEAR_FITTING_HPP

#include "catalog_core.hpp"

namespace metanum::catalog {

inline constexpr std::array<operation, 3> gsl_multilinear_fitting = {{
    operation{"linear_least_squares", "gsl_fit_linear, gsl_fit_wlinear, gsl_fit_mul, gsl_fit_linear_est", "Linear regression and fitting."},
    operation{"multifit", "gsl_multifit_linear, gsl_multifit_wlinear, gsl_multifit_linear_svd", "Multi-parameter linear least squares."},
    operation{"robust_fit", "gsl_multifit_robust", "Robust least squares."},
}};

inline constexpr category gsl_multilinear_fitting_category{
    "gsl.multilinear_fitting", library::gsl, "Least-squares and fitting.", gsl_multilinear_fitting.data(), gsl_multilinear_fitting.size()};

static_assert(detail::fields_well_formed(gsl_multilinear_fitting), "gsl_multilinear_fitting: empty or malformed name/symbol/description");
static_assert(detail::names_unique(gsl_multilinear_fitting), "gsl_multilinear_fitting: duplicate operation name");
static_assert(detail::symbols_well_formed(gsl_multilinear_fitting), "gsl_multilinear_fitting: malformed symbol list");
static_assert(detail::symbols_unique_per_operation(gsl_multilinear_fitting), "gsl_multilinear_fitting: symbol listed twice in one operation");
static_assert(detail::category_metadata_well_formed(gsl_multilinear_fitting_category), "gsl_multilinear_fitting: malformed category metadata");
static_assert(gsl_multilinear_fitting_category.begin() == gsl_multilinear_fitting.data() && gsl_multilinear_fitting_category.size() == gsl_multilinear_fitting.size(),
              "gsl_multilinear_fitting: category storage mismatch");
static_assert(!gsl_multilinear_fitting.empty(), "gsl_multilinear_fitting: empty catalog");

}  // namespace metanum::catalog

#endif  // METANUM_GSL_MULTILINEAR_FITTING_HPP

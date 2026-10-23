// MetaNum catalog: gsl.interpolation_and_splines
// Interpolation of 1D data sets.
//
// GENERATED from MANIFEST.yaml by gen_catalogs.py - edit the manifest, not this file.
// Describes the native GSL API (GNU Scientific Library); it does not reimplement or link it.
// Symbol names follow the manifest and may differ across GSL versions; check
// the installed upstream headers before relying on an exact symbol.
#ifndef METANUM_GSL_INTERPOLATION_AND_SPLINES_HPP
#define METANUM_GSL_INTERPOLATION_AND_SPLINES_HPP

#include "catalog_core.hpp"

namespace metanum::catalog {

inline constexpr std::array<operation, 4> gsl_interpolation_and_splines = {{
    operation{"interp_1d", "gsl_interp_alloc, gsl_interp_eval, gsl_interp_eval_deriv, gsl_interp_eval_deriv2, gsl_interp_eval_integ", "Linear, polynomial, cubic spline, Akima, Steffen interpolation."},
    operation{"spline_types", "gsl_interp_linear, gsl_interp_polynomial, gsl_interp_cspline, gsl_interp_cspline_periodic, gsl_interp_akima, gsl_interp_steffen", "Available interpolation types."},
    operation{"interp_2d", "gsl_interp2d_alloc, gsl_interp2d_eval", "Two-dimensional interpolation on grids."},
    operation{"spline_derivatives", "gsl_spline_eval_deriv, gsl_spline_eval_integ", "Derivatives and integrals of splines."},
}};

inline constexpr category gsl_interpolation_and_splines_category{
    "gsl.interpolation_and_splines", library::gsl, "Interpolation of 1D data sets.", gsl_interpolation_and_splines.data(), gsl_interpolation_and_splines.size()};

static_assert(detail::fields_well_formed(gsl_interpolation_and_splines), "gsl_interpolation_and_splines: empty or malformed name/symbol/description");
static_assert(detail::names_unique(gsl_interpolation_and_splines), "gsl_interpolation_and_splines: duplicate operation name");
static_assert(detail::symbols_well_formed(gsl_interpolation_and_splines), "gsl_interpolation_and_splines: malformed symbol list");
static_assert(detail::symbols_unique_per_operation(gsl_interpolation_and_splines), "gsl_interpolation_and_splines: symbol listed twice in one operation");
static_assert(detail::category_metadata_well_formed(gsl_interpolation_and_splines_category), "gsl_interpolation_and_splines: malformed category metadata");
static_assert(gsl_interpolation_and_splines_category.begin() == gsl_interpolation_and_splines.data() && gsl_interpolation_and_splines_category.size() == gsl_interpolation_and_splines.size(),
              "gsl_interpolation_and_splines: category storage mismatch");
static_assert(!gsl_interpolation_and_splines.empty(), "gsl_interpolation_and_splines: empty catalog");

}  // namespace metanum::catalog

#endif  // METANUM_GSL_INTERPOLATION_AND_SPLINES_HPP

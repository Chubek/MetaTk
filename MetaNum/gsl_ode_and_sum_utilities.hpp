// MetaNum catalog: gsl.ode_and_sum_utilities
// Miscellaneous numeric helpers.
//
// GENERATED from MANIFEST.yaml by gen_catalogs.py - edit the manifest, not this file.
// Describes the native GSL API (GNU Scientific Library); it does not reimplement or link it.
// Symbol names follow the manifest and may differ across GSL versions; check
// the installed upstream headers before relying on an exact symbol.
#ifndef METANUM_GSL_ODE_AND_SUM_UTILITIES_HPP
#define METANUM_GSL_ODE_AND_SUM_UTILITIES_HPP

#include "catalog_core.hpp"

namespace metanum::catalog {

inline constexpr std::array<operation, 4> gsl_ode_and_sum_utilities = {{
    operation{"min_max_helpers", "gsl_fcmp", "Floating-point comparison within tolerance."},
    operation{"ieee_utils", "gsl_ieee_env_setup, gsl_isnan, gsl_isinf, gsl_finite", "IEEE floating-point handling."},
    operation{"machine_constants", "GSL_DBL_EPSILON, GSL_DBL_MAX, GSL_DBL_MIN", "Machine constants."},
    operation{"unit_conversion", "GSL_CONST_*", "Physical constants and unit conversions."},
}};

inline constexpr category gsl_ode_and_sum_utilities_category{
    "gsl.ode_and_sum_utilities", library::gsl, "Miscellaneous numeric helpers.", gsl_ode_and_sum_utilities.data(), gsl_ode_and_sum_utilities.size()};

static_assert(detail::fields_well_formed(gsl_ode_and_sum_utilities), "gsl_ode_and_sum_utilities: empty or malformed name/symbol/description");
static_assert(detail::names_unique(gsl_ode_and_sum_utilities), "gsl_ode_and_sum_utilities: duplicate operation name");
static_assert(detail::symbols_well_formed(gsl_ode_and_sum_utilities), "gsl_ode_and_sum_utilities: malformed symbol list");
static_assert(detail::symbols_unique_per_operation(gsl_ode_and_sum_utilities), "gsl_ode_and_sum_utilities: symbol listed twice in one operation");
static_assert(detail::category_metadata_well_formed(gsl_ode_and_sum_utilities_category), "gsl_ode_and_sum_utilities: malformed category metadata");
static_assert(gsl_ode_and_sum_utilities_category.begin() == gsl_ode_and_sum_utilities.data() && gsl_ode_and_sum_utilities_category.size() == gsl_ode_and_sum_utilities.size(),
              "gsl_ode_and_sum_utilities: category storage mismatch");
static_assert(!gsl_ode_and_sum_utilities.empty(), "gsl_ode_and_sum_utilities: empty catalog");

}  // namespace metanum::catalog

#endif  // METANUM_GSL_ODE_AND_SUM_UTILITIES_HPP

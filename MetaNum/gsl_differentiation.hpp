// MetaNum catalog: gsl.differentiation
// Numerical differentiation.
//
// GENERATED from MANIFEST.yaml by gen_catalogs.py - edit the manifest, not this file.
// Describes the native GSL API (GNU Scientific Library); it does not reimplement or link it.
// Symbol names follow the manifest and may differ across GSL versions; check
// the installed upstream headers before relying on an exact symbol.
#ifndef METANUM_GSL_DIFFERENTIATION_HPP
#define METANUM_GSL_DIFFERENTIATION_HPP

#include "catalog_core.hpp"

namespace metanum::catalog {

inline constexpr std::array<operation, 3> gsl_differentiation = {{
    operation{"deriv_central", "gsl_deriv_central", "Central difference derivative."},
    operation{"deriv_forward", "gsl_deriv_forward", "Forward difference derivative."},
    operation{"deriv_backward", "gsl_deriv_backward", "Backward difference derivative."},
}};

inline constexpr category gsl_differentiation_category{
    "gsl.differentiation", library::gsl, "Numerical differentiation.", gsl_differentiation.data(), gsl_differentiation.size()};

static_assert(detail::fields_well_formed(gsl_differentiation), "gsl_differentiation: empty or malformed name/symbol/description");
static_assert(detail::names_unique(gsl_differentiation), "gsl_differentiation: duplicate operation name");
static_assert(detail::symbols_well_formed(gsl_differentiation), "gsl_differentiation: malformed symbol list");
static_assert(detail::symbols_unique_per_operation(gsl_differentiation), "gsl_differentiation: symbol listed twice in one operation");
static_assert(detail::category_metadata_well_formed(gsl_differentiation_category), "gsl_differentiation: malformed category metadata");
static_assert(gsl_differentiation_category.begin() == gsl_differentiation.data() && gsl_differentiation_category.size() == gsl_differentiation.size(),
              "gsl_differentiation: category storage mismatch");
static_assert(!gsl_differentiation.empty(), "gsl_differentiation: empty catalog");

}  // namespace metanum::catalog

#endif  // METANUM_GSL_DIFFERENTIATION_HPP

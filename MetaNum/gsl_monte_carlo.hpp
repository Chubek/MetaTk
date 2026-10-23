// MetaNum catalog: gsl.monte_carlo
// Monte Carlo integration.
//
// GENERATED from MANIFEST.yaml by gen_catalogs.py - edit the manifest, not this file.
// Describes the native GSL API (GNU Scientific Library); it does not reimplement or link it.
// Symbol names follow the manifest and may differ across GSL versions; check
// the installed upstream headers before relying on an exact symbol.
#ifndef METANUM_GSL_MONTE_CARLO_HPP
#define METANUM_GSL_MONTE_CARLO_HPP

#include "catalog_core.hpp"

namespace metanum::catalog {

inline constexpr std::array<operation, 3> gsl_monte_carlo = {{
    operation{"vegas", "gsl_monte_vegas_integrate", "VEGAS adaptive importance sampling."},
    operation{"miser", "gsl_monte_miser_integrate", "MISER recursive stratified sampling."},
    operation{"plain", "gsl_monte_plain_integrate", "Plain Monte Carlo integration."},
}};

inline constexpr category gsl_monte_carlo_category{
    "gsl.monte_carlo", library::gsl, "Monte Carlo integration.", gsl_monte_carlo.data(), gsl_monte_carlo.size()};

static_assert(detail::fields_well_formed(gsl_monte_carlo), "gsl_monte_carlo: empty or malformed name/symbol/description");
static_assert(detail::names_unique(gsl_monte_carlo), "gsl_monte_carlo: duplicate operation name");
static_assert(detail::symbols_well_formed(gsl_monte_carlo), "gsl_monte_carlo: malformed symbol list");
static_assert(detail::symbols_unique_per_operation(gsl_monte_carlo), "gsl_monte_carlo: symbol listed twice in one operation");
static_assert(detail::category_metadata_well_formed(gsl_monte_carlo_category), "gsl_monte_carlo: malformed category metadata");
static_assert(gsl_monte_carlo_category.begin() == gsl_monte_carlo.data() && gsl_monte_carlo_category.size() == gsl_monte_carlo.size(),
              "gsl_monte_carlo: category storage mismatch");
static_assert(!gsl_monte_carlo.empty(), "gsl_monte_carlo: empty catalog");

}  // namespace metanum::catalog

#endif  // METANUM_GSL_MONTE_CARLO_HPP

// MetaNum catalog: gsl.series_acceleration
// Convergence acceleration.
//
// GENERATED from MANIFEST.yaml by gen_catalogs.py - edit the manifest, not this file.
// Describes the native GSL API (GNU Scientific Library); it does not reimplement or link it.
// Symbol names follow the manifest and may differ across GSL versions; check
// the installed upstream headers before relying on an exact symbol.
#ifndef METANUM_GSL_SERIES_ACCELERATION_HPP
#define METANUM_GSL_SERIES_ACCELERATION_HPP

#include "catalog_core.hpp"

namespace metanum::catalog {

inline constexpr std::array<operation, 2> gsl_series_acceleration = {{
    operation{"series_accel", "gsl_sum_levin_u_alloc, gsl_sum_levin_u_accel, gsl_sum_levin_utrunc_alloc, gsl_sum_levin_utrunc_accel", "Levin u-transform for series acceleration."},
    operation{"sum_epsilon", "gsl_sum_levin_u_accel", "Convergence acceleration via epsilon algorithm."},
}};

inline constexpr category gsl_series_acceleration_category{
    "gsl.series_acceleration", library::gsl, "Convergence acceleration.", gsl_series_acceleration.data(), gsl_series_acceleration.size()};

static_assert(detail::fields_well_formed(gsl_series_acceleration), "gsl_series_acceleration: empty or malformed name/symbol/description");
static_assert(detail::names_unique(gsl_series_acceleration), "gsl_series_acceleration: duplicate operation name");
static_assert(detail::symbols_well_formed(gsl_series_acceleration), "gsl_series_acceleration: malformed symbol list");
static_assert(detail::symbols_unique_per_operation(gsl_series_acceleration), "gsl_series_acceleration: symbol listed twice in one operation");
static_assert(detail::category_metadata_well_formed(gsl_series_acceleration_category), "gsl_series_acceleration: malformed category metadata");
static_assert(gsl_series_acceleration_category.begin() == gsl_series_acceleration.data() && gsl_series_acceleration_category.size() == gsl_series_acceleration.size(),
              "gsl_series_acceleration: category storage mismatch");
static_assert(!gsl_series_acceleration.empty(), "gsl_series_acceleration: empty catalog");

}  // namespace metanum::catalog

#endif  // METANUM_GSL_SERIES_ACCELERATION_HPP

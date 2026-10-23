// MetaNum catalog: gsl.wavelets
// Discrete wavelet transforms.
//
// GENERATED from MANIFEST.yaml by gen_catalogs.py - edit the manifest, not this file.
// Describes the native GSL API (GNU Scientific Library); it does not reimplement or link it.
// Symbol names follow the manifest and may differ across GSL versions; check
// the installed upstream headers before relying on an exact symbol.
#ifndef METANUM_GSL_WAVELETS_HPP
#define METANUM_GSL_WAVELETS_HPP

#include "catalog_core.hpp"

namespace metanum::catalog {

inline constexpr std::array<operation, 3> gsl_wavelets = {{
    operation{"wavelet_transform", "gsl_wavelet_transform, gsl_wavelet_transform_forward, gsl_wavelet_transform_inverse", "Forward and inverse DWT."},
    operation{"wavelet_2d", "gsl_wavelet2d_transform, gsl_wavelet2d_nstransform", "2D wavelet transforms."},
    operation{"wavelet_families", "gsl_wavelet_daubechies, gsl_wavelet_haar, gsl_wavelet_bspline", "Daubechies, Haar, and B-spline wavelet families."},
}};

inline constexpr category gsl_wavelets_category{
    "gsl.wavelets", library::gsl, "Discrete wavelet transforms.", gsl_wavelets.data(), gsl_wavelets.size()};

static_assert(detail::fields_well_formed(gsl_wavelets), "gsl_wavelets: empty or malformed name/symbol/description");
static_assert(detail::names_unique(gsl_wavelets), "gsl_wavelets: duplicate operation name");
static_assert(detail::symbols_well_formed(gsl_wavelets), "gsl_wavelets: malformed symbol list");
static_assert(detail::symbols_unique_per_operation(gsl_wavelets), "gsl_wavelets: symbol listed twice in one operation");
static_assert(detail::category_metadata_well_formed(gsl_wavelets_category), "gsl_wavelets: malformed category metadata");
static_assert(gsl_wavelets_category.begin() == gsl_wavelets.data() && gsl_wavelets_category.size() == gsl_wavelets.size(),
              "gsl_wavelets: category storage mismatch");
static_assert(!gsl_wavelets.empty(), "gsl_wavelets: empty catalog");

}  // namespace metanum::catalog

#endif  // METANUM_GSL_WAVELETS_HPP

// MetaNum catalog: gsl.fft
// Fast Fourier transforms.
//
// GENERATED from MANIFEST.yaml by gen_catalogs.py - edit the manifest, not this file.
// Describes the native GSL API (GNU Scientific Library); it does not reimplement or link it.
// Symbol names follow the manifest and may differ across GSL versions; check
// the installed upstream headers before relying on an exact symbol.
#ifndef METANUM_GSL_FFT_HPP
#define METANUM_GSL_FFT_HPP

#include "catalog_core.hpp"

namespace metanum::catalog {

inline constexpr std::array<operation, 3> gsl_fft = {{
    operation{"complex_fft", "gsl_fft_complex_radix2_forward, gsl_fft_complex_radix2_backward, gsl_fft_complex_forward, gsl_fft_complex_backward, gsl_fft_complex_inverse", "Complex FFT (radix-2 and mixed radix)."},
    operation{"real_fft", "gsl_fft_real_radix2_transform, gsl_fft_real_transform, gsl_fft_halfcomplex_inverse", "Real-valued FFT."},
    operation{"wavetable", "gsl_fft_complex_wavetable_alloc, gsl_fft_real_wavetable_alloc", "Precomputed trigonometric tables."},
}};

inline constexpr category gsl_fft_category{
    "gsl.fft", library::gsl, "Fast Fourier transforms.", gsl_fft.data(), gsl_fft.size()};

static_assert(detail::fields_well_formed(gsl_fft), "gsl_fft: empty or malformed name/symbol/description");
static_assert(detail::names_unique(gsl_fft), "gsl_fft: duplicate operation name");
static_assert(detail::symbols_well_formed(gsl_fft), "gsl_fft: malformed symbol list");
static_assert(detail::symbols_unique_per_operation(gsl_fft), "gsl_fft: symbol listed twice in one operation");
static_assert(detail::category_metadata_well_formed(gsl_fft_category), "gsl_fft: malformed category metadata");
static_assert(gsl_fft_category.begin() == gsl_fft.data() && gsl_fft_category.size() == gsl_fft.size(),
              "gsl_fft: category storage mismatch");
static_assert(!gsl_fft.empty(), "gsl_fft: empty catalog");

}  // namespace metanum::catalog

#endif  // METANUM_GSL_FFT_HPP

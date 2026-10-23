// MetaNum catalog: gsl.histograms
// Binned data and histograms.
//
// GENERATED from MANIFEST.yaml by gen_catalogs.py - edit the manifest, not this file.
// Describes the native GSL API (GNU Scientific Library); it does not reimplement or link it.
// Symbol names follow the manifest and may differ across GSL versions; check
// the installed upstream headers before relying on an exact symbol.
#ifndef METANUM_GSL_HISTOGRAMS_HPP
#define METANUM_GSL_HISTOGRAMS_HPP

#include "catalog_core.hpp"

namespace metanum::catalog {

inline constexpr std::array<operation, 4> gsl_histograms = {{
    operation{"histogram_1d", "gsl_histogram_alloc, gsl_histogram_set_ranges_uniform, gsl_histogram_increment, gsl_histogram_accumulate, gsl_histogram_find, gsl_histogram_get, gsl_histogram_sum", "One-dimensional histograms."},
    operation{"histogram_2d", "gsl_histogram2d_alloc, gsl_histogram2d_increment, gsl_histogram2d_find", "Two-dimensional histograms."},
    operation{"histogram_ops", "gsl_histogram_add, gsl_histogram_sub, gsl_histogram_mul, gsl_histogram_div, gsl_histogram_scale", "Arithmetic on histograms."},
    operation{"histogram_pdf", "gsl_histogram_pdf_alloc, gsl_histogram_pdf_sample", "Sampling from histogram distributions."},
}};

inline constexpr category gsl_histograms_category{
    "gsl.histograms", library::gsl, "Binned data and histograms.", gsl_histograms.data(), gsl_histograms.size()};

static_assert(detail::fields_well_formed(gsl_histograms), "gsl_histograms: empty or malformed name/symbol/description");
static_assert(detail::names_unique(gsl_histograms), "gsl_histograms: duplicate operation name");
static_assert(detail::symbols_well_formed(gsl_histograms), "gsl_histograms: malformed symbol list");
static_assert(detail::symbols_unique_per_operation(gsl_histograms), "gsl_histograms: symbol listed twice in one operation");
static_assert(detail::category_metadata_well_formed(gsl_histograms_category), "gsl_histograms: malformed category metadata");
static_assert(gsl_histograms_category.begin() == gsl_histograms.data() && gsl_histograms_category.size() == gsl_histograms.size(),
              "gsl_histograms: category storage mismatch");
static_assert(!gsl_histograms.empty(), "gsl_histograms: empty catalog");

}  // namespace metanum::catalog

#endif  // METANUM_GSL_HISTOGRAMS_HPP

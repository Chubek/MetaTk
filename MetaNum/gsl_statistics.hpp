// MetaNum catalog: gsl.statistics
// Descriptive and robust statistics.
//
// GENERATED from MANIFEST.yaml by gen_catalogs.py - edit the manifest, not this file.
// Describes the native GSL API (GNU Scientific Library); it does not reimplement or link it.
// Symbol names follow the manifest and may differ across GSL versions; check
// the installed upstream headers before relying on an exact symbol.
#ifndef METANUM_GSL_STATISTICS_HPP
#define METANUM_GSL_STATISTICS_HPP

#include "catalog_core.hpp"

namespace metanum::catalog {

inline constexpr std::array<operation, 7> gsl_statistics = {{
    operation{"moments", "gsl_stats_mean, gsl_stats_variance, gsl_stats_sd, gsl_stats_variance_with_fixed_mean, gsl_stats_sd_with_fixed_mean, gsl_stats_tss, gsl_stats_tss_m", "Mean, variance, standard deviation."},
    operation{"higher_moments", "gsl_stats_skew, gsl_stats_kurtosis, gsl_stats_skew_m_sd, gsl_stats_kurtosis_m_sd, gsl_stats_absdev, gsl_stats_absdev_m", "Skewness, kurtosis, mean absolute deviation."},
    operation{"order_stats", "gsl_stats_median_from_sorted_data, gsl_stats_quantile_from_sorted_data, gsl_stats_max, gsl_stats_min, gsl_stats_minmax", "Median, quantiles, extremes."},
    operation{"robust_stats", "gsl_stats_median, gsl_stats_mad0, gsl_stats_mad", "Median absolute deviation."},
    operation{"weighted_stats", "gsl_stats_wmean, gsl_stats_wvariance, gsl_stats_wsd, gsl_stats_wvariance_with_fixed_mean", "Weighted statistics."},
    operation{"covariance_correlation", "gsl_stats_covariance, gsl_stats_correlation, gsl_stats_spearman, gsl_stats_pvariance, gsl_stats_ttest", "Covariance, correlation, Spearman, t-test."},
    operation{"ranking", "gsl_sort_ranking", "Rank computation."},
}};

inline constexpr category gsl_statistics_category{
    "gsl.statistics", library::gsl, "Descriptive and robust statistics.", gsl_statistics.data(), gsl_statistics.size()};

static_assert(detail::fields_well_formed(gsl_statistics), "gsl_statistics: empty or malformed name/symbol/description");
static_assert(detail::names_unique(gsl_statistics), "gsl_statistics: duplicate operation name");
static_assert(detail::symbols_well_formed(gsl_statistics), "gsl_statistics: malformed symbol list");
static_assert(detail::symbols_unique_per_operation(gsl_statistics), "gsl_statistics: symbol listed twice in one operation");
static_assert(detail::category_metadata_well_formed(gsl_statistics_category), "gsl_statistics: malformed category metadata");
static_assert(gsl_statistics_category.begin() == gsl_statistics.data() && gsl_statistics_category.size() == gsl_statistics.size(),
              "gsl_statistics: category storage mismatch");
static_assert(!gsl_statistics.empty(), "gsl_statistics: empty catalog");

}  // namespace metanum::catalog

#endif  // METANUM_GSL_STATISTICS_HPP

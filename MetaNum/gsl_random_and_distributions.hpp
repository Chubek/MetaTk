// MetaNum catalog: gsl.random_and_distributions
// Random number generators and probability distributions.
//
// GENERATED from MANIFEST.yaml by gen_catalogs.py - edit the manifest, not this file.
// Describes the native GSL API (GNU Scientific Library); it does not reimplement or link it.
// Symbol names follow the manifest and may differ across GSL versions; check
// the installed upstream headers before relying on an exact symbol.
#ifndef METANUM_GSL_RANDOM_AND_DISTRIBUTIONS_HPP
#define METANUM_GSL_RANDOM_AND_DISTRIBUTIONS_HPP

#include "catalog_core.hpp"

namespace metanum::catalog {

inline constexpr std::array<operation, 8> gsl_random_and_distributions = {{
    operation{"rng_generators", "gsl_rng_mt19937, gsl_rng_ranlxs0, gsl_rng_ranlxd1, gsl_rng_taus, gsl_rng_gfsr4, gsl_rng_ranf, gsl_rng_random_bsd", "Available pseudo-random generator algorithms."},
    operation{"rng_quasirandom", "gsl_qrng_sobol, gsl_qrng_halton, gsl_qrng_reversehalton, gsl_qrng_niederreiter_2", "Quasi-random sequence generators."},
    operation{"gaussian", "gsl_ran_gaussian, gsl_ran_gaussian_pdf, gsl_ran_gaussian_ziggurat, gsl_ran_ugaussian", "Gaussian distribution."},
    operation{"continuous_dists", "gsl_ran_exponential, gsl_ran_gamma, gsl_ran_beta, gsl_ran_chisq, gsl_ran_cauchy, gsl_ran_laplace, gsl_ran_lognormal, gsl_ran_weibull, gsl_ran_logistic, gsl_ran_pareto, gsl_ran_rayleigh, gsl_ran_t, gsl_ran_F, gsl_ran_flat", "Common continuous distributions."},
    operation{"discrete_dists", "gsl_ran_binomial, gsl_ran_poisson, gsl_ran_geometric, gsl_ran_negative_binomial, gsl_ran_pascal, gsl_ran_hypergeometric, gsl_ran_logarithmic", "Common discrete distributions."},
    operation{"multivariate_dists", "gsl_ran_dirichlet, gsl_ran_multinomial, gsl_ran_multivariate_gaussian", "Multivariate distributions."},
    operation{"shuffling_sampling", "gsl_ran_shuffle, gsl_ran_choose, gsl_ran_sample, gsl_ran_discrete_preproc", "Shuffling and sampling."},
    operation{"cdf_pdf_functions", "gsl_cdf_gaussian_P, gsl_cdf_gaussian_Q, gsl_cdf_chisq_P, gsl_cdf_fdist_P, gsl_cdf_tdist_P, gsl_cdf_binomial_P, gsl_cdf_poisson_P", "Cumulative distribution and quantile functions."},
}};

inline constexpr category gsl_random_and_distributions_category{
    "gsl.random_and_distributions", library::gsl, "Random number generators and probability distributions.", gsl_random_and_distributions.data(), gsl_random_and_distributions.size()};

static_assert(detail::fields_well_formed(gsl_random_and_distributions), "gsl_random_and_distributions: empty or malformed name/symbol/description");
static_assert(detail::names_unique(gsl_random_and_distributions), "gsl_random_and_distributions: duplicate operation name");
static_assert(detail::symbols_well_formed(gsl_random_and_distributions), "gsl_random_and_distributions: malformed symbol list");
static_assert(detail::symbols_unique_per_operation(gsl_random_and_distributions), "gsl_random_and_distributions: symbol listed twice in one operation");
static_assert(detail::category_metadata_well_formed(gsl_random_and_distributions_category), "gsl_random_and_distributions: malformed category metadata");
static_assert(gsl_random_and_distributions_category.begin() == gsl_random_and_distributions.data() && gsl_random_and_distributions_category.size() == gsl_random_and_distributions.size(),
              "gsl_random_and_distributions: category storage mismatch");
static_assert(!gsl_random_and_distributions.empty(), "gsl_random_and_distributions: empty catalog");

}  // namespace metanum::catalog

#endif  // METANUM_GSL_RANDOM_AND_DISTRIBUTIONS_HPP

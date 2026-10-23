// MetaNum umbrella header.
//
// GENERATED from MANIFEST.yaml by gen_catalogs.py - edit the manifest, not this file.
// Pulls in the numeric routines (numeric.hpp) and every operation catalog, and
// exposes a registry of all 40 categories / 298 operations.
#ifndef METANUM_ALL_HPP
#define METANUM_ALL_HPP

#include "numeric.hpp"
#include "catalog_core.hpp"

#include "isl_basic_objects.hpp"
#include "isl_set_algebra.hpp"
#include "isl_composition_and_application.hpp"
#include "isl_optimization_and_lattice.hpp"
#include "isl_affine_and_piecewise.hpp"
#include "isl_polynomials.hpp"
#include "isl_scheduling_and_dependence.hpp"
#include "isl_printing_and_io.hpp"
#include "gsl_elementary_and_special_functions.hpp"
#include "gsl_complex_numbers.hpp"
#include "gsl_polynomials_and_roots.hpp"
#include "gsl_linear_algebra_basic.hpp"
#include "gsl_linear_algebra_decompositions.hpp"
#include "gsl_eigensystems.hpp"
#include "gsl_fft.hpp"
#include "gsl_wavelets.hpp"
#include "gsl_numerical_integration.hpp"
#include "gsl_differentiation.hpp"
#include "gsl_root_finding.hpp"
#include "gsl_minimization.hpp"
#include "gsl_ordinary_differential_equations.hpp"
#include "gsl_interpolation_and_splines.hpp"
#include "gsl_series_acceleration.hpp"
#include "gsl_statistics.hpp"
#include "gsl_random_and_distributions.hpp"
#include "gsl_histograms.hpp"
#include "gsl_sorting_and_permutations.hpp"
#include "gsl_ode_and_sum_utilities.hpp"
#include "gsl_monte_carlo.hpp"
#include "gsl_vector_and_matrix_views.hpp"
#include "gsl_sparse_and_structured.hpp"
#include "gsl_multilinear_fitting.hpp"
#include "ppl_space_dimensions_and_expressions.hpp"
#include "ppl_polyhedra_constructors.hpp"
#include "ppl_polyhedra_queries.hpp"
#include "ppl_polyhedra_set_operations.hpp"
#include "ppl_grid_operations.hpp"
#include "ppl_optimization.hpp"
#include "ppl_coefficients_and_arithmetic.hpp"
#include "ppl_bit_and_numeric_utilities.hpp"

namespace metanum::catalog {

/// MANIFEST.yaml `manifest.version` these headers were generated from.
inline constexpr int manifest_version = 1;

/// Every category, in manifest order.
inline constexpr std::array<const category*, 40> all_categories = {{
    &isl_basic_objects_category,
    &isl_set_algebra_category,
    &isl_composition_and_application_category,
    &isl_optimization_and_lattice_category,
    &isl_affine_and_piecewise_category,
    &isl_polynomials_category,
    &isl_scheduling_and_dependence_category,
    &isl_printing_and_io_category,
    &gsl_elementary_and_special_functions_category,
    &gsl_complex_numbers_category,
    &gsl_polynomials_and_roots_category,
    &gsl_linear_algebra_basic_category,
    &gsl_linear_algebra_decompositions_category,
    &gsl_eigensystems_category,
    &gsl_fft_category,
    &gsl_wavelets_category,
    &gsl_numerical_integration_category,
    &gsl_differentiation_category,
    &gsl_root_finding_category,
    &gsl_minimization_category,
    &gsl_ordinary_differential_equations_category,
    &gsl_interpolation_and_splines_category,
    &gsl_series_acceleration_category,
    &gsl_statistics_category,
    &gsl_random_and_distributions_category,
    &gsl_histograms_category,
    &gsl_sorting_and_permutations_category,
    &gsl_ode_and_sum_utilities_category,
    &gsl_monte_carlo_category,
    &gsl_vector_and_matrix_views_category,
    &gsl_sparse_and_structured_category,
    &gsl_multilinear_fitting_category,
    &ppl_space_dimensions_and_expressions_category,
    &ppl_polyhedra_constructors_category,
    &ppl_polyhedra_queries_category,
    &ppl_polyhedra_set_operations_category,
    &ppl_grid_operations_category,
    &ppl_optimization_category,
    &ppl_coefficients_and_arithmetic_category,
    &ppl_bit_and_numeric_utilities_category,
}};

/// Total number of operations across all categories.
inline constexpr std::size_t total_operations = [] {
    std::size_t n = 0;
    for (const category* c : all_categories) n += c->size();
    return n;
}();

/// Look up a category by manifest id (e.g. "gsl.fft"); nullptr if absent.
[[nodiscard]] constexpr const category* find_category(std::string_view id) noexcept {
    for (const category* c : all_categories)
        if (c->id == id) return c;
    return nullptr;
}

/// Look up an operation by category id and operation name; nullptr if absent.
[[nodiscard]] constexpr const operation* find_operation(std::string_view category_id,
                                                        std::string_view name) noexcept {
    for (const category* c : all_categories)
        if (c->id == category_id) return find(*c, name);
    return nullptr;
}

namespace detail {
constexpr bool category_ids_unique() noexcept {
    for (std::size_t i = 0; i < all_categories.size(); ++i)
        for (std::size_t j = i + 1; j < all_categories.size(); ++j)
            if (all_categories[i]->id == all_categories[j]->id) return false;
    return true;
}
constexpr bool category_prefixes_match_library() noexcept {
    for (const category* c : all_categories) {
        if (!category_metadata_well_formed(*c)) return false;
    }
    return true;
}
}  // namespace detail

static_assert(total_operations == 298, "operation total differs from MANIFEST.yaml");
static_assert(detail::category_ids_unique(), "duplicate category id");
static_assert(detail::category_prefixes_match_library(), "category id does not match its library");

}  // namespace metanum::catalog

#endif  // METANUM_ALL_HPP

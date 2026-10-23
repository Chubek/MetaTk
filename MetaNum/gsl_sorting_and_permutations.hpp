// MetaNum catalog: gsl.sorting_and_permutations
// Sorting and permutation utilities.
//
// GENERATED from MANIFEST.yaml by gen_catalogs.py - edit the manifest, not this file.
// Describes the native GSL API (GNU Scientific Library); it does not reimplement or link it.
// Symbol names follow the manifest and may differ across GSL versions; check
// the installed upstream headers before relying on an exact symbol.
#ifndef METANUM_GSL_SORTING_AND_PERMUTATIONS_HPP
#define METANUM_GSL_SORTING_AND_PERMUTATIONS_HPP

#include "catalog_core.hpp"

namespace metanum::catalog {

inline constexpr std::array<operation, 4> gsl_sorting_and_permutations = {{
    operation{"sort", "gsl_sort, gsl_sort2, gsl_sort_vector, gsl_sort_vector2, gsl_sort_index", "In-place sorting and index sorting."},
    operation{"sort_selection", "gsl_sort_smallest, gsl_sort_largest, gsl_sort_vector_smallest, gsl_sort_vector_largest", "Partial sorting for k-smallest and k-largest."},
    operation{"permutation", "gsl_permutation_alloc, gsl_permutation_next, gsl_permutation_prev, gsl_permutation_mul, gsl_permutation_inverse, gsl_permutation_swap", "Permutation operations."},
    operation{"combination", "gsl_combination_alloc, gsl_combination_next, gsl_combination_prev", "Combination enumeration."},
}};

inline constexpr category gsl_sorting_and_permutations_category{
    "gsl.sorting_and_permutations", library::gsl, "Sorting and permutation utilities.", gsl_sorting_and_permutations.data(), gsl_sorting_and_permutations.size()};

static_assert(detail::fields_well_formed(gsl_sorting_and_permutations), "gsl_sorting_and_permutations: empty or malformed name/symbol/description");
static_assert(detail::names_unique(gsl_sorting_and_permutations), "gsl_sorting_and_permutations: duplicate operation name");
static_assert(detail::symbols_well_formed(gsl_sorting_and_permutations), "gsl_sorting_and_permutations: malformed symbol list");
static_assert(detail::symbols_unique_per_operation(gsl_sorting_and_permutations), "gsl_sorting_and_permutations: symbol listed twice in one operation");
static_assert(detail::category_metadata_well_formed(gsl_sorting_and_permutations_category), "gsl_sorting_and_permutations: malformed category metadata");
static_assert(gsl_sorting_and_permutations_category.begin() == gsl_sorting_and_permutations.data() && gsl_sorting_and_permutations_category.size() == gsl_sorting_and_permutations.size(),
              "gsl_sorting_and_permutations: category storage mismatch");
static_assert(!gsl_sorting_and_permutations.empty(), "gsl_sorting_and_permutations: empty catalog");

}  // namespace metanum::catalog

#endif  // METANUM_GSL_SORTING_AND_PERMUTATIONS_HPP

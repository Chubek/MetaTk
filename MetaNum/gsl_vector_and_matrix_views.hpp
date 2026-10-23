// MetaNum catalog: gsl.vector_and_matrix_views
// Views and tensor-like utilities.
//
// GENERATED from MANIFEST.yaml by gen_catalogs.py - edit the manifest, not this file.
// Describes the native GSL API (GNU Scientific Library); it does not reimplement or link it.
// Symbol names follow the manifest and may differ across GSL versions; check
// the installed upstream headers before relying on an exact symbol.
#ifndef METANUM_GSL_VECTOR_AND_MATRIX_VIEWS_HPP
#define METANUM_GSL_VECTOR_AND_MATRIX_VIEWS_HPP

#include "catalog_core.hpp"

namespace metanum::catalog {

inline constexpr std::array<operation, 1> gsl_vector_and_matrix_views = {{
    operation{"views", "gsl_vector_subvector, gsl_matrix_submatrix, gsl_matrix_row, gsl_matrix_column, gsl_matrix_diagonal", "Non-owning sub-views of vectors and matrices."},
}};

inline constexpr category gsl_vector_and_matrix_views_category{
    "gsl.vector_and_matrix_views", library::gsl, "Views and tensor-like utilities.", gsl_vector_and_matrix_views.data(), gsl_vector_and_matrix_views.size()};

static_assert(detail::fields_well_formed(gsl_vector_and_matrix_views), "gsl_vector_and_matrix_views: empty or malformed name/symbol/description");
static_assert(detail::names_unique(gsl_vector_and_matrix_views), "gsl_vector_and_matrix_views: duplicate operation name");
static_assert(detail::symbols_well_formed(gsl_vector_and_matrix_views), "gsl_vector_and_matrix_views: malformed symbol list");
static_assert(detail::symbols_unique_per_operation(gsl_vector_and_matrix_views), "gsl_vector_and_matrix_views: symbol listed twice in one operation");
static_assert(detail::category_metadata_well_formed(gsl_vector_and_matrix_views_category), "gsl_vector_and_matrix_views: malformed category metadata");
static_assert(gsl_vector_and_matrix_views_category.begin() == gsl_vector_and_matrix_views.data() && gsl_vector_and_matrix_views_category.size() == gsl_vector_and_matrix_views.size(),
              "gsl_vector_and_matrix_views: category storage mismatch");
static_assert(!gsl_vector_and_matrix_views.empty(), "gsl_vector_and_matrix_views: empty catalog");

}  // namespace metanum::catalog

#endif  // METANUM_GSL_VECTOR_AND_MATRIX_VIEWS_HPP

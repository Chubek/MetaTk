// MetaNum catalog: gsl.sparse_and_structured
// Sparse matrix support.
//
// GENERATED from MANIFEST.yaml by gen_catalogs.py - edit the manifest, not this file.
// Describes the native GSL API (GNU Scientific Library); it does not reimplement or link it.
// Symbol names follow the manifest and may differ across GSL versions; check
// the installed upstream headers before relying on an exact symbol.
#ifndef METANUM_GSL_SPARSE_AND_STRUCTURED_HPP
#define METANUM_GSL_SPARSE_AND_STRUCTURED_HPP

#include "catalog_core.hpp"

namespace metanum::catalog {

inline constexpr std::array<operation, 3> gsl_sparse_and_structured = {{
    operation{"sparse_storage", "gsl_spmatrix_alloc, gsl_spmatrix_set, gsl_spmatrix_get, gsl_spmatrix_compress", "Sparse matrix construction and storage formats."},
    operation{"sparse_ops", "gsl_spblas_dgemv, gsl_spblas_dgemm", "Sparse BLAS operations."},
    operation{"sparse_solve", "gsl_splinalg_itersolve_alloc, gsl_splinalg_itersolve_iterate", "Iterative sparse linear solvers."},
}};

inline constexpr category gsl_sparse_and_structured_category{
    "gsl.sparse_and_structured", library::gsl, "Sparse matrix support.", gsl_sparse_and_structured.data(), gsl_sparse_and_structured.size()};

static_assert(detail::fields_well_formed(gsl_sparse_and_structured), "gsl_sparse_and_structured: empty or malformed name/symbol/description");
static_assert(detail::names_unique(gsl_sparse_and_structured), "gsl_sparse_and_structured: duplicate operation name");
static_assert(detail::symbols_well_formed(gsl_sparse_and_structured), "gsl_sparse_and_structured: malformed symbol list");
static_assert(detail::symbols_unique_per_operation(gsl_sparse_and_structured), "gsl_sparse_and_structured: symbol listed twice in one operation");
static_assert(detail::category_metadata_well_formed(gsl_sparse_and_structured_category), "gsl_sparse_and_structured: malformed category metadata");
static_assert(gsl_sparse_and_structured_category.begin() == gsl_sparse_and_structured.data() && gsl_sparse_and_structured_category.size() == gsl_sparse_and_structured.size(),
              "gsl_sparse_and_structured: category storage mismatch");
static_assert(!gsl_sparse_and_structured.empty(), "gsl_sparse_and_structured: empty catalog");

}  // namespace metanum::catalog

#endif  // METANUM_GSL_SPARSE_AND_STRUCTURED_HPP

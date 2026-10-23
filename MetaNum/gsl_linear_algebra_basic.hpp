// MetaNum catalog: gsl.linear_algebra_basic
// Vector and matrix primitives, BLAS.
//
// GENERATED from MANIFEST.yaml by gen_catalogs.py - edit the manifest, not this file.
// Describes the native GSL API (GNU Scientific Library); it does not reimplement or link it.
// Symbol names follow the manifest and may differ across GSL versions; check
// the installed upstream headers before relying on an exact symbol.
#ifndef METANUM_GSL_LINEAR_ALGEBRA_BASIC_HPP
#define METANUM_GSL_LINEAR_ALGEBRA_BASIC_HPP

#include "catalog_core.hpp"

namespace metanum::catalog {

inline constexpr std::array<operation, 6> gsl_linear_algebra_basic = {{
    operation{"vector_ops", "gsl_vector_add, gsl_vector_sub, gsl_vector_mul, gsl_vector_div, gsl_vector_scale, gsl_vector_add_constant, gsl_vector_max, gsl_vector_min, gsl_vector_minmax", "Element-wise vector operations and extremes."},
    operation{"matrix_ops", "gsl_matrix_add, gsl_matrix_sub, gsl_matrix_mul_elements, gsl_matrix_div_elements, gsl_matrix_scale, gsl_matrix_transpose", "Element-wise matrix operations and transpose."},
    operation{"blas_level1", "gsl_blas_sdsdot, gsl_blas_dsdot, gsl_blas_sasum, gsl_blas_dasum, gsl_blas_dnrm2, gsl_blas_ddot, gsl_blas_daxpy, gsl_blas_dscal, gsl_blas_drot, gsl_blas_idamax", "BLAS level 1 (vector-vector) operations."},
    operation{"blas_level2", "gsl_blas_dgemv, gsl_blas_dtrmv, gsl_blas_dtrsv, gsl_blas_dsymv, gsl_blas_dger, gsl_blas_dsyr, gsl_blas_dsyr2", "BLAS level 2 (matrix-vector) operations."},
    operation{"blas_level3", "gsl_blas_dgemm, gsl_blas_dsymm, gsl_blas_dtrmm, gsl_blas_dtrsm, gsl_blas_dsyrk, gsl_blas_dsyr2k", "BLAS level 3 (matrix-matrix) operations."},
    operation{"complex_blas", "gsl_blas_zdotu, gsl_blas_zgemm, gsl_blas_zhemm", "Complex BLAS routines."},
}};

inline constexpr category gsl_linear_algebra_basic_category{
    "gsl.linear_algebra_basic", library::gsl, "Vector and matrix primitives, BLAS.", gsl_linear_algebra_basic.data(), gsl_linear_algebra_basic.size()};

static_assert(detail::fields_well_formed(gsl_linear_algebra_basic), "gsl_linear_algebra_basic: empty or malformed name/symbol/description");
static_assert(detail::names_unique(gsl_linear_algebra_basic), "gsl_linear_algebra_basic: duplicate operation name");
static_assert(detail::symbols_well_formed(gsl_linear_algebra_basic), "gsl_linear_algebra_basic: malformed symbol list");
static_assert(detail::symbols_unique_per_operation(gsl_linear_algebra_basic), "gsl_linear_algebra_basic: symbol listed twice in one operation");
static_assert(detail::category_metadata_well_formed(gsl_linear_algebra_basic_category), "gsl_linear_algebra_basic: malformed category metadata");
static_assert(gsl_linear_algebra_basic_category.begin() == gsl_linear_algebra_basic.data() && gsl_linear_algebra_basic_category.size() == gsl_linear_algebra_basic.size(),
              "gsl_linear_algebra_basic: category storage mismatch");
static_assert(!gsl_linear_algebra_basic.empty(), "gsl_linear_algebra_basic: empty catalog");

}  // namespace metanum::catalog

#endif  // METANUM_GSL_LINEAR_ALGEBRA_BASIC_HPP

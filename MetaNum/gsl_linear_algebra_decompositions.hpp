// MetaNum catalog: gsl.linear_algebra_decompositions
// Factorizations and linear system solvers.
//
// GENERATED from MANIFEST.yaml by gen_catalogs.py - edit the manifest, not this file.
// Describes the native GSL API (GNU Scientific Library); it does not reimplement or link it.
// Symbol names follow the manifest and may differ across GSL versions; check
// the installed upstream headers before relying on an exact symbol.
#ifndef METANUM_GSL_LINEAR_ALGEBRA_DECOMPOSITIONS_HPP
#define METANUM_GSL_LINEAR_ALGEBRA_DECOMPOSITIONS_HPP

#include "catalog_core.hpp"

namespace metanum::catalog {

inline constexpr std::array<operation, 14> gsl_linear_algebra_decompositions = {{
    operation{"lu", "gsl_linalg_LU_decomp, gsl_linalg_LU_solve, gsl_linalg_LU_invert, gsl_linalg_LU_det, gsl_linalg_LU_lndet, gsl_linalg_LU_refine", "LU decomposition, solve, invert, determinant."},
    operation{"cholesky", "gsl_linalg_cholesky_decomp, gsl_linalg_cholesky_solve, gsl_linalg_cholesky_invert, gsl_linalg_cholesky_det", "Cholesky decomposition and related."},
    operation{"ldlt", "gsl_linalg_ldlt_decomp", "LDL^T decomposition (for symmetric indefinite)."},
    operation{"qr", "gsl_linalg_QR_decomp, gsl_linalg_QR_solve, gsl_linalg_QR_lssolve, gsl_linalg_QR_Q_solve, gsl_linalg_QR_unpack", "QR decomposition and least squares."},
    operation{"qrpt", "gsl_linalg_QRPT_decomp", "QR with column pivoting."},
    operation{"lq", "gsl_linalg_LQ_decomp", "LQ decomposition."},
    operation{"svd", "gsl_linalg_SV_decomp, gsl_linalg_SV_decomp_jacobi, gsl_linalg_SV_solve", "Singular value decomposition and pseudo-inverse solves."},
    operation{"bidiagonal", "gsl_linalg_bidiag_decomp, gsl_linalg_bidiag_unpack", "Bidiagonalization."},
    operation{"hessenberg", "gsl_linalg_hessenberg_decomp, gsl_linalg_hessenberg_unpack, gsl_linalg_hessenberg_set_zero", "Hessenberg reduction."},
    operation{"tridiagonal", "gsl_linalg_solve_tridiag, gsl_linalg_solve_symm_tridiag, gsl_linalg_solve_cyc_tridiag", "Tridiagonal and cyclic tridiagonal solvers."},
    operation{"householder", "gsl_linalg_householder_transform, gsl_linalg_householder_hm", "Householder reflections."},
    operation{"balance", "gsl_linalg_balance_matrix", "Balancing matrices before eigenvalue computation."},
    operation{"condition_number", "gsl_linalg_cond, gsl_linalg_rcond", "Condition number estimation."},
    operation{"complex_linalg", "gsl_linalg_complex_LU_decomp, gsl_linalg_complex_cholesky_decomp, gsl_linalg_complex_QR_decomp", "Complex versions of the decompositions."},
}};

inline constexpr category gsl_linear_algebra_decompositions_category{
    "gsl.linear_algebra_decompositions", library::gsl, "Factorizations and linear system solvers.", gsl_linear_algebra_decompositions.data(), gsl_linear_algebra_decompositions.size()};

static_assert(detail::fields_well_formed(gsl_linear_algebra_decompositions), "gsl_linear_algebra_decompositions: empty or malformed name/symbol/description");
static_assert(detail::names_unique(gsl_linear_algebra_decompositions), "gsl_linear_algebra_decompositions: duplicate operation name");
static_assert(detail::symbols_well_formed(gsl_linear_algebra_decompositions), "gsl_linear_algebra_decompositions: malformed symbol list");
static_assert(detail::symbols_unique_per_operation(gsl_linear_algebra_decompositions), "gsl_linear_algebra_decompositions: symbol listed twice in one operation");
static_assert(detail::category_metadata_well_formed(gsl_linear_algebra_decompositions_category), "gsl_linear_algebra_decompositions: malformed category metadata");
static_assert(gsl_linear_algebra_decompositions_category.begin() == gsl_linear_algebra_decompositions.data() && gsl_linear_algebra_decompositions_category.size() == gsl_linear_algebra_decompositions.size(),
              "gsl_linear_algebra_decompositions: category storage mismatch");
static_assert(!gsl_linear_algebra_decompositions.empty(), "gsl_linear_algebra_decompositions: empty catalog");

}  // namespace metanum::catalog

#endif  // METANUM_GSL_LINEAR_ALGEBRA_DECOMPOSITIONS_HPP

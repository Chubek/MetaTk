// MetaNum catalog: gsl.eigensystems
// Eigenvalue and eigenvector computations.
//
// GENERATED from MANIFEST.yaml by gen_catalogs.py - edit the manifest, not this file.
// Describes the native GSL API (GNU Scientific Library); it does not reimplement or link it.
// Symbol names follow the manifest and may differ across GSL versions; check
// the installed upstream headers before relying on an exact symbol.
#ifndef METANUM_GSL_EIGENSYSTEMS_HPP
#define METANUM_GSL_EIGENSYSTEMS_HPP

#include "catalog_core.hpp"

namespace metanum::catalog {

inline constexpr std::array<operation, 7> gsl_eigensystems = {{
    operation{"symm_eigen", "gsl_eigen_symm, gsl_eigen_symmv", "Eigenvalues (and vectors) of real symmetric matrices."},
    operation{"herm_eigen", "gsl_eigen_herm, gsl_eigen_hermv", "Eigenvalues (and vectors) of complex Hermitian matrices."},
    operation{"nonsymm_eigen", "gsl_eigen_nonsymm, gsl_eigen_nonsymmv", "Eigenvalues (and vectors) of general real matrices."},
    operation{"gen_symm_eigen", "gsl_eigen_gensymm, gsl_eigen_gensymmv", "Generalized symmetric eigenproblem."},
    operation{"gen_nonsymm_eigen", "gsl_eigen_gen, gsl_eigen_genv", "Generalized nonsymmetric eigenproblem."},
    operation{"symm_jacobi", "gsl_eigen_jacobi", "Jacobi eigenvalue method."},
    operation{"symm_tridiag", "gsl_eigen_symmv_sort", "Sorting of eigenvalues and eigenvectors."},
}};

inline constexpr category gsl_eigensystems_category{
    "gsl.eigensystems", library::gsl, "Eigenvalue and eigenvector computations.", gsl_eigensystems.data(), gsl_eigensystems.size()};

static_assert(detail::fields_well_formed(gsl_eigensystems), "gsl_eigensystems: empty or malformed name/symbol/description");
static_assert(detail::names_unique(gsl_eigensystems), "gsl_eigensystems: duplicate operation name");
static_assert(detail::symbols_well_formed(gsl_eigensystems), "gsl_eigensystems: malformed symbol list");
static_assert(detail::symbols_unique_per_operation(gsl_eigensystems), "gsl_eigensystems: symbol listed twice in one operation");
static_assert(detail::category_metadata_well_formed(gsl_eigensystems_category), "gsl_eigensystems: malformed category metadata");
static_assert(gsl_eigensystems_category.begin() == gsl_eigensystems.data() && gsl_eigensystems_category.size() == gsl_eigensystems.size(),
              "gsl_eigensystems: category storage mismatch");
static_assert(!gsl_eigensystems.empty(), "gsl_eigensystems: empty catalog");

}  // namespace metanum::catalog

#endif  // METANUM_GSL_EIGENSYSTEMS_HPP

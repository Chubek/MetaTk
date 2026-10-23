// MetaNum catalog: gsl.root_finding
// One-dimensional and multidimensional root solvers.
//
// GENERATED from MANIFEST.yaml by gen_catalogs.py - edit the manifest, not this file.
// Describes the native GSL API (GNU Scientific Library); it does not reimplement or link it.
// Symbol names follow the manifest and may differ across GSL versions; check
// the installed upstream headers before relying on an exact symbol.
#ifndef METANUM_GSL_ROOT_FINDING_HPP
#define METANUM_GSL_ROOT_FINDING_HPP

#include "catalog_core.hpp"

namespace metanum::catalog {

inline constexpr std::array<operation, 5> gsl_root_finding = {{
    operation{"bracketing_1d", "gsl_root_fsolver_bisection, gsl_root_fsolver_brent, gsl_root_fsolver_falsepos", "Bracketing solvers for 1D functions."},
    operation{"derivative_1d", "gsl_root_fdfsolver_newton, gsl_root_fdfsolver_secant, gsl_root_fdfsolver_steffenson", "Derivative-based 1D solvers."},
    operation{"multiroot_hybrid", "gsl_multiroot_fsolver_hybrid, gsl_multiroot_fsolver_hybrids, gsl_multiroot_fsolver_dnewton", "Multidimensional root finding (Powell hybrid and Newton variants)."},
    operation{"multiroot_broyden", "gsl_multiroot_fsolver_broyden", "Broyden's method."},
    operation{"multiroot_fdf", "gsl_multiroot_fdfsolver_gnewton", "Gauss-Newton and Newton derivative-based solvers."},
}};

inline constexpr category gsl_root_finding_category{
    "gsl.root_finding", library::gsl, "One-dimensional and multidimensional root solvers.", gsl_root_finding.data(), gsl_root_finding.size()};

static_assert(detail::fields_well_formed(gsl_root_finding), "gsl_root_finding: empty or malformed name/symbol/description");
static_assert(detail::names_unique(gsl_root_finding), "gsl_root_finding: duplicate operation name");
static_assert(detail::symbols_well_formed(gsl_root_finding), "gsl_root_finding: malformed symbol list");
static_assert(detail::symbols_unique_per_operation(gsl_root_finding), "gsl_root_finding: symbol listed twice in one operation");
static_assert(detail::category_metadata_well_formed(gsl_root_finding_category), "gsl_root_finding: malformed category metadata");
static_assert(gsl_root_finding_category.begin() == gsl_root_finding.data() && gsl_root_finding_category.size() == gsl_root_finding.size(),
              "gsl_root_finding: category storage mismatch");
static_assert(!gsl_root_finding.empty(), "gsl_root_finding: empty catalog");

}  // namespace metanum::catalog

#endif  // METANUM_GSL_ROOT_FINDING_HPP

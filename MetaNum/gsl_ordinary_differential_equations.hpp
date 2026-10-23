// MetaNum catalog: gsl.ordinary_differential_equations
// ODE integrators.
//
// GENERATED from MANIFEST.yaml by gen_catalogs.py - edit the manifest, not this file.
// Describes the native GSL API (GNU Scientific Library); it does not reimplement or link it.
// Symbol names follow the manifest and may differ across GSL versions; check
// the installed upstream headers before relying on an exact symbol.
#ifndef METANUM_GSL_ORDINARY_DIFFERENTIAL_EQUATIONS_HPP
#define METANUM_GSL_ORDINARY_DIFFERENTIAL_EQUATIONS_HPP

#include "catalog_core.hpp"

namespace metanum::catalog {

inline constexpr std::array<operation, 3> gsl_ordinary_differential_equations = {{
    operation{"odeiv2_step", "gsl_odeiv2_step_rk2, gsl_odeiv2_step_rk4, gsl_odeiv2_step_rkf45, gsl_odeiv2_step_rkck, gsl_odeiv2_step_rk8pd, gsl_odeiv2_step_rk1imp, gsl_odeiv2_step_rk2imp, gsl_odeiv2_step_rk4imp, gsl_odeiv2_step_bsimp, gsl_odeiv2_step_gear1, gsl_odeiv2_step_gear2, gsl_odeiv2_step_msadams, gsl_odeiv2_step_msbdf", "Explicit, implicit, and stiff step functions."},
    operation{"odeiv2_control", "gsl_odeiv2_control_y_new, gsl_odeiv2_control_yp_new, gsl_odeiv2_control_standard_new", "Adaptive step-size control."},
    operation{"odeiv2_evolve", "gsl_odeiv2_evolve_apply", "Evolve the ODE system."},
}};

inline constexpr category gsl_ordinary_differential_equations_category{
    "gsl.ordinary_differential_equations", library::gsl, "ODE integrators.", gsl_ordinary_differential_equations.data(), gsl_ordinary_differential_equations.size()};

static_assert(detail::fields_well_formed(gsl_ordinary_differential_equations), "gsl_ordinary_differential_equations: empty or malformed name/symbol/description");
static_assert(detail::names_unique(gsl_ordinary_differential_equations), "gsl_ordinary_differential_equations: duplicate operation name");
static_assert(detail::symbols_well_formed(gsl_ordinary_differential_equations), "gsl_ordinary_differential_equations: malformed symbol list");
static_assert(detail::symbols_unique_per_operation(gsl_ordinary_differential_equations), "gsl_ordinary_differential_equations: symbol listed twice in one operation");
static_assert(detail::category_metadata_well_formed(gsl_ordinary_differential_equations_category), "gsl_ordinary_differential_equations: malformed category metadata");
static_assert(gsl_ordinary_differential_equations_category.begin() == gsl_ordinary_differential_equations.data() && gsl_ordinary_differential_equations_category.size() == gsl_ordinary_differential_equations.size(),
              "gsl_ordinary_differential_equations: category storage mismatch");
static_assert(!gsl_ordinary_differential_equations.empty(), "gsl_ordinary_differential_equations: empty catalog");

}  // namespace metanum::catalog

#endif  // METANUM_GSL_ORDINARY_DIFFERENTIAL_EQUATIONS_HPP

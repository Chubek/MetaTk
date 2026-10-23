// MetaNum catalog: gsl.numerical_integration
// Quadrature and adaptive integration.
//
// GENERATED from MANIFEST.yaml by gen_catalogs.py - edit the manifest, not this file.
// Describes the native GSL API (GNU Scientific Library); it does not reimplement or link it.
// Symbol names follow the manifest and may differ across GSL versions; check
// the installed upstream headers before relying on an exact symbol.
#ifndef METANUM_GSL_NUMERICAL_INTEGRATION_HPP
#define METANUM_GSL_NUMERICAL_INTEGRATION_HPP

#include "catalog_core.hpp"

namespace metanum::catalog {

inline constexpr std::array<operation, 9> gsl_numerical_integration = {{
    operation{"qng", "gsl_integration_qng", "Non-adaptive Gauss-Kronrod integration."},
    operation{"qag", "gsl_integration_qag, gsl_integration_qags, gsl_integration_qagp, gsl_integration_qagi, gsl_integration_qagiu, gsl_integration_qagil", "Adaptive integration over finite and infinite ranges."},
    operation{"qawc", "gsl_integration_qawc", "Cauchy principal value integrals."},
    operation{"qaws", "gsl_integration_qaws", "Integrals with algebraic-logarithmic endpoint singularities."},
    operation{"qawo", "gsl_integration_qawo", "Oscillatory integrals (Fourier type)."},
    operation{"qawf", "gsl_integration_qawf", "Fourier integrals over infinite ranges."},
    operation{"cquad", "gsl_integration_cquad", "Clenshaw-Curtis quadrature."},
    operation{"romberg", "gsl_integration_romberg", "Romberg integration."},
    operation{"fixed_gauss", "gsl_integration_fixed_alloc, gsl_integration_fixed", "Fixed-point Gauss-type rules (Legendre, Chebyshev, Laguerre, Hermite, Jacobi)."},
}};

inline constexpr category gsl_numerical_integration_category{
    "gsl.numerical_integration", library::gsl, "Quadrature and adaptive integration.", gsl_numerical_integration.data(), gsl_numerical_integration.size()};

static_assert(detail::fields_well_formed(gsl_numerical_integration), "gsl_numerical_integration: empty or malformed name/symbol/description");
static_assert(detail::names_unique(gsl_numerical_integration), "gsl_numerical_integration: duplicate operation name");
static_assert(detail::symbols_well_formed(gsl_numerical_integration), "gsl_numerical_integration: malformed symbol list");
static_assert(detail::symbols_unique_per_operation(gsl_numerical_integration), "gsl_numerical_integration: symbol listed twice in one operation");
static_assert(detail::category_metadata_well_formed(gsl_numerical_integration_category), "gsl_numerical_integration: malformed category metadata");
static_assert(gsl_numerical_integration_category.begin() == gsl_numerical_integration.data() && gsl_numerical_integration_category.size() == gsl_numerical_integration.size(),
              "gsl_numerical_integration: category storage mismatch");
static_assert(!gsl_numerical_integration.empty(), "gsl_numerical_integration: empty catalog");

}  // namespace metanum::catalog

#endif  // METANUM_GSL_NUMERICAL_INTEGRATION_HPP

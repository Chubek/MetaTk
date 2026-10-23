// MetaNum catalog: gsl.elementary_and_special_functions
// Special mathematical functions.
//
// GENERATED from MANIFEST.yaml by gen_catalogs.py - edit the manifest, not this file.
// Describes the native GSL API (GNU Scientific Library); it does not reimplement or link it.
// Symbol names follow the manifest and may differ across GSL versions; check
// the installed upstream headers before relying on an exact symbol.
#ifndef METANUM_GSL_ELEMENTARY_AND_SPECIAL_FUNCTIONS_HPP
#define METANUM_GSL_ELEMENTARY_AND_SPECIAL_FUNCTIONS_HPP

#include "catalog_core.hpp"

namespace metanum::catalog {

inline constexpr std::array<operation, 23> gsl_elementary_and_special_functions = {{
    operation{"gamma", "gsl_sf_gamma, gsl_sf_lngamma, gsl_sf_gammainv, gsl_sf_gamma_inc, gsl_sf_gamma_inc_P, gsl_sf_gamma_inc_Q", "Gamma function, log-gamma, incomplete gamma."},
    operation{"beta", "gsl_sf_beta, gsl_sf_lnbeta, gsl_sf_beta_inc", "Beta function and incomplete beta."},
    operation{"psi", "gsl_sf_psi, gsl_sf_psi_1, gsl_sf_psi_n", "Digamma and polygamma functions."},
    operation{"bessel_j", "gsl_sf_bessel_J0, gsl_sf_bessel_J1, gsl_sf_bessel_Jn", "Bessel functions of the first kind."},
    operation{"bessel_y", "gsl_sf_bessel_Y0, gsl_sf_bessel_Y1, gsl_sf_bessel_Yn", "Bessel functions of the second kind."},
    operation{"bessel_i", "gsl_sf_bessel_I0, gsl_sf_bessel_I1, gsl_sf_bessel_In", "Modified Bessel functions of the first kind."},
    operation{"bessel_k", "gsl_sf_bessel_K0, gsl_sf_bessel_K1, gsl_sf_bessel_Kn", "Modified Bessel functions of the second kind."},
    operation{"spherical_bessel", "gsl_sf_bessel_jl, gsl_sf_bessel_yl", "Spherical Bessel functions."},
    operation{"airy", "gsl_sf_airy_Ai, gsl_sf_airy_Bi", "Airy functions."},
    operation{"erf", "gsl_sf_erf, gsl_sf_erfc, gsl_sf_log_erfc, gsl_sf_erf_Z, gsl_sf_erf_Q", "Error function and complement."},
    operation{"fresnel", "gsl_sf_fresnel_s, gsl_sf_fresnel_c", "Fresnel integrals."},
    operation{"exponential_integral", "gsl_sf_expint_E1, gsl_sf_expint_E2, gsl_sf_expint_Ei, gsl_sf_Shi, gsl_sf_Chi", "Exponential and hyperbolic integrals."},
    operation{"elliptic_integral", "gsl_sf_ellint_Kcomp, gsl_sf_ellint_Ecomp, gsl_sf_ellint_F, gsl_sf_ellint_E, gsl_sf_ellint_P, gsl_sf_ellint_D, gsl_sf_ellint_RC, gsl_sf_ellint_RD, gsl_sf_ellint_RF, gsl_sf_ellint_RJ", "Complete and incomplete elliptic integrals."},
    operation{"hypergeometric", "gsl_sf_hyperg_0F1, gsl_sf_hyperg_1F1, gsl_sf_hyperg_2F0, gsl_sf_hyperg_U", "Confluent hypergeometric and related functions."},
    operation{"laguerre", "gsl_sf_laguerre_n, gsl_sf_laguerre_1, gsl_sf_laguerre_2, gsl_sf_laguerre_3", "Generalized Laguerre polynomials."},
    operation{"gegenbauer", "gsl_sf_gegenpoly_n", "Gegenbauer polynomials."},
    operation{"legendre", "gsl_sf_legendre_Pl, gsl_sf_legendre_Plm, gsl_sf_legendre_sphPlm, gsl_sf_legendre_Ql", "Legendre polynomials and associated functions."},
    operation{"conical", "gsl_sf_conicalP_half, gsl_sf_conicalP_mhalf", "Conical functions."},
    operation{"zeta", "gsl_sf_zeta, gsl_sf_zeta_int, gsl_sf_eta, gsl_sf_lambert_W0, gsl_sf_lambert_Wm1", "Riemann zeta, Dirichlet eta, and Lambert W."},
    operation{"coulomb", "gsl_sf_coulomb_wave_F_array", "Coulomb wave functions."},
    operation{"transport", "gsl_sf_transport_2, gsl_sf_transport_3", "Transport functions."},
    operation{"sinc_and_trig", "gsl_sf_sinc, gsl_sf_lnsinh, gsl_sf_lncosh, gsl_sf_hypot", "Sinc, log of sinh and cosh, robust hypot."},
    operation{"combinatorics", "gsl_sf_fact, gsl_sf_doublefact, gsl_sf_lnfact, gsl_sf_choose, gsl_sf_lnchoose, gsl_sf_poch, gsl_sf_lnpoch, gsl_sf_pochrel", "Factorials, binomial coefficients, Pochhammer symbols."},
}};

inline constexpr category gsl_elementary_and_special_functions_category{
    "gsl.elementary_and_special_functions", library::gsl, "Special mathematical functions.", gsl_elementary_and_special_functions.data(), gsl_elementary_and_special_functions.size()};

static_assert(detail::fields_well_formed(gsl_elementary_and_special_functions), "gsl_elementary_and_special_functions: empty or malformed name/symbol/description");
static_assert(detail::names_unique(gsl_elementary_and_special_functions), "gsl_elementary_and_special_functions: duplicate operation name");
static_assert(detail::symbols_well_formed(gsl_elementary_and_special_functions), "gsl_elementary_and_special_functions: malformed symbol list");
static_assert(detail::symbols_unique_per_operation(gsl_elementary_and_special_functions), "gsl_elementary_and_special_functions: symbol listed twice in one operation");
static_assert(detail::category_metadata_well_formed(gsl_elementary_and_special_functions_category), "gsl_elementary_and_special_functions: malformed category metadata");
static_assert(gsl_elementary_and_special_functions_category.begin() == gsl_elementary_and_special_functions.data() && gsl_elementary_and_special_functions_category.size() == gsl_elementary_and_special_functions.size(),
              "gsl_elementary_and_special_functions: category storage mismatch");
static_assert(!gsl_elementary_and_special_functions.empty(), "gsl_elementary_and_special_functions: empty catalog");

}  // namespace metanum::catalog

#endif  // METANUM_GSL_ELEMENTARY_AND_SPECIAL_FUNCTIONS_HPP

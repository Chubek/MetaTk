// MetaNum catalog: gsl.complex_numbers
// Complex arithmetic and utilities.
//
// GENERATED from MANIFEST.yaml by gen_catalogs.py - edit the manifest, not this file.
// Describes the native GSL API (GNU Scientific Library); it does not reimplement or link it.
// Symbol names follow the manifest and may differ across GSL versions; check
// the installed upstream headers before relying on an exact symbol.
#ifndef METANUM_GSL_COMPLEX_NUMBERS_HPP
#define METANUM_GSL_COMPLEX_NUMBERS_HPP

#include "catalog_core.hpp"

namespace metanum::catalog {

inline constexpr std::array<operation, 5> gsl_complex_numbers = {{
    operation{"complex_arithmetic", "gsl_complex_add, gsl_complex_sub, gsl_complex_mul, gsl_complex_div", "Basic complex arithmetic."},
    operation{"complex_functions", "gsl_complex_exp, gsl_complex_log, gsl_complex_sqrt, gsl_complex_pow, gsl_complex_sin, gsl_complex_cos, gsl_complex_tan", "Elementary complex functions."},
    operation{"complex_inverse_trig", "gsl_complex_arcsin, gsl_complex_arccos, gsl_complex_arctan", "Inverse trigonometric functions."},
    operation{"complex_hyperbolic", "gsl_complex_sinh, gsl_complex_cosh, gsl_complex_tanh", "Hyperbolic functions."},
    operation{"complex_properties", "gsl_complex_abs, gsl_complex_abs2, gsl_complex_arg, gsl_complex_conjugate, gsl_complex_inverse, gsl_complex_negative", "Modulus, argument, conjugate, inverse, negation."},
}};

inline constexpr category gsl_complex_numbers_category{
    "gsl.complex_numbers", library::gsl, "Complex arithmetic and utilities.", gsl_complex_numbers.data(), gsl_complex_numbers.size()};

static_assert(detail::fields_well_formed(gsl_complex_numbers), "gsl_complex_numbers: empty or malformed name/symbol/description");
static_assert(detail::names_unique(gsl_complex_numbers), "gsl_complex_numbers: duplicate operation name");
static_assert(detail::symbols_well_formed(gsl_complex_numbers), "gsl_complex_numbers: malformed symbol list");
static_assert(detail::symbols_unique_per_operation(gsl_complex_numbers), "gsl_complex_numbers: symbol listed twice in one operation");
static_assert(detail::category_metadata_well_formed(gsl_complex_numbers_category), "gsl_complex_numbers: malformed category metadata");
static_assert(gsl_complex_numbers_category.begin() == gsl_complex_numbers.data() && gsl_complex_numbers_category.size() == gsl_complex_numbers.size(),
              "gsl_complex_numbers: category storage mismatch");
static_assert(!gsl_complex_numbers.empty(), "gsl_complex_numbers: empty catalog");

}  // namespace metanum::catalog

#endif  // METANUM_GSL_COMPLEX_NUMBERS_HPP

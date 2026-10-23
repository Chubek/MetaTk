// MetaNum catalog: isl.affine_and_piecewise
// Affine, piecewise-affine, and quasi-affine expressions.
//
// GENERATED from MANIFEST.yaml by gen_catalogs.py - edit the manifest, not this file.
// Describes the native ISL API (Integer Set Library); it does not reimplement or link it.
// Symbol names follow the manifest and may differ across ISL versions; check
// the installed upstream headers before relying on an exact symbol.
#ifndef METANUM_ISL_AFFINE_AND_PIECEWISE_HPP
#define METANUM_ISL_AFFINE_AND_PIECEWISE_HPP

#include "catalog_core.hpp"

namespace metanum::catalog {

inline constexpr std::array<operation, 10> isl_affine_and_piecewise = {{
    operation{"aff_construction", "isl_aff_*, isl_multi_aff_*, isl_pw_aff_*", "Build affine, multi-affine, and piecewise affine expressions."},
    operation{"aff_add", "isl_aff_add / isl_pw_aff_add", "Sum of affine expressions."},
    operation{"aff_sub", "isl_aff_sub", "Difference of affine expressions."},
    operation{"aff_scale", "isl_aff_scale_val", "Scale an affine expression by a rational constant."},
    operation{"aff_floor", "isl_aff_floor", "Floor of a quasi-affine expression (integer division)."},
    operation{"aff_mod", "isl_pw_aff_mod", "Modulo operation on piecewise affine expressions."},
    operation{"pw_aff_max", "isl_pw_aff_max", "Pointwise maximum."},
    operation{"pw_aff_min", "isl_pw_aff_min", "Pointwise minimum."},
    operation{"pw_aff_eval", "isl_pw_aff_eval", "Evaluate at a point."},
    operation{"pw_aff_sign", "isl_pw_aff_sign_set", "Set where a piecewise affine expression is nonnegative or nonpositive."},
}};

inline constexpr category isl_affine_and_piecewise_category{
    "isl.affine_and_piecewise", library::isl, "Affine, piecewise-affine, and quasi-affine expressions.", isl_affine_and_piecewise.data(), isl_affine_and_piecewise.size()};

static_assert(detail::fields_well_formed(isl_affine_and_piecewise), "isl_affine_and_piecewise: empty or malformed name/symbol/description");
static_assert(detail::names_unique(isl_affine_and_piecewise), "isl_affine_and_piecewise: duplicate operation name");
static_assert(detail::symbols_well_formed(isl_affine_and_piecewise), "isl_affine_and_piecewise: malformed symbol list");
static_assert(detail::symbols_unique_per_operation(isl_affine_and_piecewise), "isl_affine_and_piecewise: symbol listed twice in one operation");
static_assert(detail::category_metadata_well_formed(isl_affine_and_piecewise_category), "isl_affine_and_piecewise: malformed category metadata");
static_assert(isl_affine_and_piecewise_category.begin() == isl_affine_and_piecewise.data() && isl_affine_and_piecewise_category.size() == isl_affine_and_piecewise.size(),
              "isl_affine_and_piecewise: category storage mismatch");
static_assert(!isl_affine_and_piecewise.empty(), "isl_affine_and_piecewise: empty catalog");

}  // namespace metanum::catalog

#endif  // METANUM_ISL_AFFINE_AND_PIECEWISE_HPP

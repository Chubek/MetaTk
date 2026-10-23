// MetaNum catalog: ppl.space_dimensions_and_expressions
// Linear expressions, constraints, and generators.
//
// GENERATED from MANIFEST.yaml by gen_catalogs.py - edit the manifest, not this file.
// Describes the native PPL API (Parma Polyhedra Library); it does not reimplement or link it.
// Symbol names follow the manifest and may differ across PPL versions; check
// the installed upstream headers before relying on an exact symbol.
#ifndef METANUM_PPL_SPACE_DIMENSIONS_AND_EXPRESSIONS_HPP
#define METANUM_PPL_SPACE_DIMENSIONS_AND_EXPRESSIONS_HPP

#include "catalog_core.hpp"

namespace metanum::catalog {

inline constexpr std::array<operation, 6> ppl_space_dimensions_and_expressions = {{
    operation{"linear_expression", "Linear_Expression", "Build affine expressions over variables."},
    operation{"constraint", "Constraint (e.g. ==, >=, >, <=, <)", "Linear equalities and inequalities."},
    operation{"generator", "Generator (point, closure_point, ray, line)", "Generators describing a polyhedron's vertices, rays, and lines."},
    operation{"congruence", "Congruence", "Linear congruences for grids."},
    operation{"grid_generator", "Grid_Generator", "Generators for grids."},
    operation{"linear_form", "Linear_Expression arithmetic", "Addition, subtraction, and scalar multiplication of expressions."},
}};

inline constexpr category ppl_space_dimensions_and_expressions_category{
    "ppl.space_dimensions_and_expressions", library::ppl, "Linear expressions, constraints, and generators.", ppl_space_dimensions_and_expressions.data(), ppl_space_dimensions_and_expressions.size()};

static_assert(detail::fields_well_formed(ppl_space_dimensions_and_expressions), "ppl_space_dimensions_and_expressions: empty or malformed name/symbol/description");
static_assert(detail::names_unique(ppl_space_dimensions_and_expressions), "ppl_space_dimensions_and_expressions: duplicate operation name");
static_assert(detail::symbols_well_formed(ppl_space_dimensions_and_expressions), "ppl_space_dimensions_and_expressions: malformed symbol list");
static_assert(detail::symbols_unique_per_operation(ppl_space_dimensions_and_expressions), "ppl_space_dimensions_and_expressions: symbol listed twice in one operation");
static_assert(detail::category_metadata_well_formed(ppl_space_dimensions_and_expressions_category), "ppl_space_dimensions_and_expressions: malformed category metadata");
static_assert(ppl_space_dimensions_and_expressions_category.begin() == ppl_space_dimensions_and_expressions.data() && ppl_space_dimensions_and_expressions_category.size() == ppl_space_dimensions_and_expressions.size(),
              "ppl_space_dimensions_and_expressions: category storage mismatch");
static_assert(!ppl_space_dimensions_and_expressions.empty(), "ppl_space_dimensions_and_expressions: empty catalog");

}  // namespace metanum::catalog

#endif  // METANUM_PPL_SPACE_DIMENSIONS_AND_EXPRESSIONS_HPP

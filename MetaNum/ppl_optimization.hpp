// MetaNum catalog: ppl.optimization
// Linear and mixed-integer programming.
//
// GENERATED from MANIFEST.yaml by gen_catalogs.py - edit the manifest, not this file.
// Describes the native PPL API (Parma Polyhedra Library); it does not reimplement or link it.
// Symbol names follow the manifest and may differ across PPL versions; check
// the installed upstream headers before relying on an exact symbol.
#ifndef METANUM_PPL_OPTIMIZATION_HPP
#define METANUM_PPL_OPTIMIZATION_HPP

#include "catalog_core.hpp"

namespace metanum::catalog {

inline constexpr std::array<operation, 6> ppl_optimization = {{
    operation{"lp_problem", "LP_Problem", "Linear programming problem definition and solution."},
    operation{"mip_problem", "MIP_Problem", "Mixed-integer programming problem."},
    operation{"lp_solve", "LP_Problem::solve / MIP_Problem::solve", "Solve for optimal value and point."},
    operation{"lp_objective", "set_objective_function", "Set the linear objective."},
    operation{"lp_constraints", "add_constraint / add_constraints", "Add constraints to an LP or MIP."},
    operation{"lp_integer_vars", "set_integer_space_dimensions", "Declare integer variables for MIP."},
}};

inline constexpr category ppl_optimization_category{
    "ppl.optimization", library::ppl, "Linear and mixed-integer programming.", ppl_optimization.data(), ppl_optimization.size()};

static_assert(detail::fields_well_formed(ppl_optimization), "ppl_optimization: empty or malformed name/symbol/description");
static_assert(detail::names_unique(ppl_optimization), "ppl_optimization: duplicate operation name");
static_assert(detail::symbols_well_formed(ppl_optimization), "ppl_optimization: malformed symbol list");
static_assert(detail::symbols_unique_per_operation(ppl_optimization), "ppl_optimization: symbol listed twice in one operation");
static_assert(detail::category_metadata_well_formed(ppl_optimization_category), "ppl_optimization: malformed category metadata");
static_assert(ppl_optimization_category.begin() == ppl_optimization.data() && ppl_optimization_category.size() == ppl_optimization.size(),
              "ppl_optimization: category storage mismatch");
static_assert(!ppl_optimization.empty(), "ppl_optimization: empty catalog");

}  // namespace metanum::catalog

#endif  // METANUM_PPL_OPTIMIZATION_HPP

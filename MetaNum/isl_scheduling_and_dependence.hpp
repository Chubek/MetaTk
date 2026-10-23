// MetaNum catalog: isl.scheduling_and_dependence
// Dependence analysis, scheduling, and code-generation support.
//
// GENERATED from MANIFEST.yaml by gen_catalogs.py - edit the manifest, not this file.
// Describes the native ISL API (Integer Set Library); it does not reimplement or link it.
// Symbol names follow the manifest and may differ across ISL versions; check
// the installed upstream headers before relying on an exact symbol.
#ifndef METANUM_ISL_SCHEDULING_AND_DEPENDENCE_HPP
#define METANUM_ISL_SCHEDULING_AND_DEPENDENCE_HPP

#include "catalog_core.hpp"

namespace metanum::catalog {

inline constexpr std::array<operation, 7> isl_scheduling_and_dependence = {{
    operation{"flow_dependence", "isl_union_access_info_compute_flow", "Exact read-after-write dependences via access analysis."},
    operation{"dependence_analysis", "isl_union_access_info_compute_dependences", "Compute flow, anti, and output dependences."},
    operation{"schedule_compute", "isl_schedule_constraints_compute_schedule", "Compute a schedule satisfying dependence constraints (Feautrier / Pluto-style)."},
    operation{"schedule_map", "isl_schedule_get_map", "Retrieve the schedule as a union map."},
    operation{"schedule_tree_band", "isl_schedule_node_band_*", "Manipulate tiling and fusion bands in schedule trees."},
    operation{"ast_build", "isl_ast_build_from_context", "Build loop-nest ASTs from schedules."},
    operation{"ast_generate", "isl_ast_build_ast_from_schedule", "Generate abstract syntax tree for code generation."},
}};

inline constexpr category isl_scheduling_and_dependence_category{
    "isl.scheduling_and_dependence", library::isl, "Dependence analysis, scheduling, and code-generation support.", isl_scheduling_and_dependence.data(), isl_scheduling_and_dependence.size()};

static_assert(detail::fields_well_formed(isl_scheduling_and_dependence), "isl_scheduling_and_dependence: empty or malformed name/symbol/description");
static_assert(detail::names_unique(isl_scheduling_and_dependence), "isl_scheduling_and_dependence: duplicate operation name");
static_assert(detail::symbols_well_formed(isl_scheduling_and_dependence), "isl_scheduling_and_dependence: malformed symbol list");
static_assert(detail::symbols_unique_per_operation(isl_scheduling_and_dependence), "isl_scheduling_and_dependence: symbol listed twice in one operation");
static_assert(detail::category_metadata_well_formed(isl_scheduling_and_dependence_category), "isl_scheduling_and_dependence: malformed category metadata");
static_assert(isl_scheduling_and_dependence_category.begin() == isl_scheduling_and_dependence.data() && isl_scheduling_and_dependence_category.size() == isl_scheduling_and_dependence.size(),
              "isl_scheduling_and_dependence: category storage mismatch");
static_assert(!isl_scheduling_and_dependence.empty(), "isl_scheduling_and_dependence: empty catalog");

}  // namespace metanum::catalog

#endif  // METANUM_ISL_SCHEDULING_AND_DEPENDENCE_HPP

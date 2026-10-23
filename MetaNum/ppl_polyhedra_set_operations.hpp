// MetaNum catalog: ppl.polyhedra_set_operations
// Boolean and geometric operations on polyhedra.
//
// GENERATED from MANIFEST.yaml by gen_catalogs.py - edit the manifest, not this file.
// Describes the native PPL API (Parma Polyhedra Library); it does not reimplement or link it.
// Symbol names follow the manifest and may differ across PPL versions; check
// the installed upstream headers before relying on an exact symbol.
#ifndef METANUM_PPL_POLYHEDRA_SET_OPERATIONS_HPP
#define METANUM_PPL_POLYHEDRA_SET_OPERATIONS_HPP

#include "catalog_core.hpp"

namespace metanum::catalog {

inline constexpr std::array<operation, 28> ppl_polyhedra_set_operations = {{
    operation{"intersection", "intersection_assign()", "Intersection of two polyhedra."},
    operation{"convex_hull", "poly_hull_assign()", "Convex polyhedral hull of two polyhedra."},
    operation{"bounded_hull", "bHull_assign / bounded_hull", "Bounded convex hull approximation."},
    operation{"poly_difference", "poly_difference_assign()", "Convex-polyhedral over-approximation of set difference."},
    operation{"pairwise_merge", "pairwise_merge / pairwise_merge_assign", "Merging of disjunctive polyhedra in a collection."},
    operation{"cc_hull", "concatenation / convex_hull of Pointset_Powerset", "Pointset powerset operations (union, difference, hull)."},
    operation{"powerset_upper_approx", "upper_bound_assign (Pointset_Powerset)", "Upper approximation for powerset domains."},
    operation{"time_elapse", "time_elapse_assign()", "Time-elapse operator for hybrid-system reachability."},
    operation{"topological_closure", "topological_closure_assign()", "Closure of an NNC polyhedron."},
    operation{"affine_image", "affine_image(Variable, Linear_Expression, denominator)", "Image of a polyhedron under an invertible or non-invertible affine map."},
    operation{"affine_preimage", "affine_preimage(Variable, Linear_Expression, denominator)", "Preimage under an affine map."},
    operation{"generalized_affine_image", "generalized_affine_image(...)", "Affine transformation with relation (equality or inequality)."},
    operation{"generalized_affine_preimage", "generalized_affine_preimage(...)", "Preimage with relation."},
    operation{"bounded_affine_image", "bounded_affine_image", "Affine image with bounds on expressions."},
    operation{"unconstrain", "unconstrain(Variable)", "Existential quantification (projection) away from a variable."},
    operation{"remove_higher_space_dimensions", "remove_higher_space_dimensions(dim)", "Dimension reduction."},
    operation{"add_space_dimensions", "add_space_dimensions_and_embed(n)", "Embedding into a higher-dimensional space."},
    operation{"concatenate", "concatenate_assign()", "Cartesian product of polyhedra."},
    operation{"swap_dimensions", "swap_space_dimensions", "Renaming dimensions."},
    operation{"map_space_dimensions", "map_space_dimensions", "General dimension remapping."},
    operation{"minimized_form", "minimized_constraints / minimized_generators", "Minimal constraint or generator representation."},
    operation{"bounding_box", "bounds / get_bounding_box", "Interval bounding box of a polyhedron."},
    operation{"affine_dimension", "affine_dimension()", "Dimension of the affine hull."},
    operation{"space_dimension", "space_dimension()", "Dimension of the ambient space."},
    operation{"widen", "H79_widening_assign, BHRZ03_widening_assign, BHZ09_widening_assign, limited_H79_extrapolation_assign, bounded_H79_extrapolation_assign", "Widening operators for fixpoint computations."},
    operation{"extrapolation", "limited_extrapolation_assign, bounded_extrapolation_assign", "Extrapolation operators for convergence acceleration."},
    operation{"narrow", "H79_narrowing_assign", "Narrowing operator."},
    operation{"upper_bound", "upper_bound_assign", "Least upper bound of two polyhedra."},
}};

inline constexpr category ppl_polyhedra_set_operations_category{
    "ppl.polyhedra_set_operations", library::ppl, "Boolean and geometric operations on polyhedra.", ppl_polyhedra_set_operations.data(), ppl_polyhedra_set_operations.size()};

static_assert(detail::fields_well_formed(ppl_polyhedra_set_operations), "ppl_polyhedra_set_operations: empty or malformed name/symbol/description");
static_assert(detail::names_unique(ppl_polyhedra_set_operations), "ppl_polyhedra_set_operations: duplicate operation name");
static_assert(detail::symbols_well_formed(ppl_polyhedra_set_operations), "ppl_polyhedra_set_operations: malformed symbol list");
static_assert(detail::symbols_unique_per_operation(ppl_polyhedra_set_operations), "ppl_polyhedra_set_operations: symbol listed twice in one operation");
static_assert(detail::category_metadata_well_formed(ppl_polyhedra_set_operations_category), "ppl_polyhedra_set_operations: malformed category metadata");
static_assert(ppl_polyhedra_set_operations_category.begin() == ppl_polyhedra_set_operations.data() && ppl_polyhedra_set_operations_category.size() == ppl_polyhedra_set_operations.size(),
              "ppl_polyhedra_set_operations: category storage mismatch");
static_assert(!ppl_polyhedra_set_operations.empty(), "ppl_polyhedra_set_operations: empty catalog");

}  // namespace metanum::catalog

#endif  // METANUM_PPL_POLYHEDRA_SET_OPERATIONS_HPP

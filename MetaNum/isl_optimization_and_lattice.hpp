// MetaNum catalog: isl.optimization_and_lattice
// Optimization over parametric integer sets and lattice queries.
//
// GENERATED from MANIFEST.yaml by gen_catalogs.py - edit the manifest, not this file.
// Describes the native ISL API (Integer Set Library); it does not reimplement or link it.
// Symbol names follow the manifest and may differ across ISL versions; check
// the installed upstream headers before relying on an exact symbol.
#ifndef METANUM_ISL_OPTIMIZATION_AND_LATTICE_HPP
#define METANUM_ISL_OPTIMIZATION_AND_LATTICE_HPP

#include "catalog_core.hpp"

namespace metanum::catalog {

inline constexpr std::array<operation, 21> isl_optimization_and_lattice = {{
    operation{"lexmin", "isl_set_lexmin / isl_map_lexmin", "Lexicographically minimal point(s) as a piecewise quasi-affine function of parameters."},
    operation{"lexmax", "isl_set_lexmax / isl_map_lexmax", "Lexicographically maximal point(s) as a piecewise quasi-affine function."},
    operation{"min_val", "isl_set_min_val", "Minimum value of an affine expression over a set."},
    operation{"max_val", "isl_set_max_val", "Maximum value of an affine expression over a set."},
    operation{"dim_min / dim_max", "isl_set_dim_min, isl_set_dim_max", "Minimum or maximum of a dimension, as a piecewise quasi-affine function."},
    operation{"sample_point", "isl_set_sample_point", "Return an integer point in a set."},
    operation{"card", "isl_set_card / isl_map_card", "Number of integer points as a piecewise quasi-polynomial."},
    operation{"count_points", "isl_set_count_val", "Count points for a concrete parameter value."},
    operation{"ehrhart", "isl_set_card (parametric)", "Parametric counting via Ehrhart-style quasi-polynomials."},
    operation{"enumerate", "isl_set_foreach_point", "Iterate over all integer points."},
    operation{"lattice_points", "isl_basic_set_lattice_points", "Enumerate points of a bounded set."},
    operation{"affine_hull", "isl_set_affine_hull", "Smallest affine subspace containing a set."},
    operation{"convex_hull", "isl_set_convex_hull", "Convex hull of an integer set."},
    operation{"simple_hull", "isl_set_simple_hull", "Approximate convex hull using constraints from the input."},
    operation{"coalesce", "isl_set_coalesce / isl_map_coalesce", "Merge pieces of a union into fewer convex pieces."},
    operation{"remove_redundancies", "isl_set_remove_redundancies", "Remove redundant constraints."},
    operation{"gist", "isl_set_gist / isl_map_gist", "Simplify a set given a context set."},
    operation{"project_out", "isl_set_project_out / isl_map_project_out", "Existential projection (Fourier-Motzkin or equivalent)."},
    operation{"eliminate_dims", "isl_set_eliminate", "Eliminate a range of dimensions."},
    operation{"align_params", "isl_set_align_params", "Align parameter spaces of two objects."},
    operation{"lift", "isl_set_lift", "Lift a set to introduce existential variables."},
}};

inline constexpr category isl_optimization_and_lattice_category{
    "isl.optimization_and_lattice", library::isl, "Optimization over parametric integer sets and lattice queries.", isl_optimization_and_lattice.data(), isl_optimization_and_lattice.size()};

static_assert(detail::fields_well_formed(isl_optimization_and_lattice), "isl_optimization_and_lattice: empty or malformed name/symbol/description");
static_assert(detail::names_unique(isl_optimization_and_lattice), "isl_optimization_and_lattice: duplicate operation name");
static_assert(detail::symbols_well_formed(isl_optimization_and_lattice), "isl_optimization_and_lattice: malformed symbol list");
static_assert(detail::symbols_unique_per_operation(isl_optimization_and_lattice), "isl_optimization_and_lattice: symbol listed twice in one operation");
static_assert(detail::category_metadata_well_formed(isl_optimization_and_lattice_category), "isl_optimization_and_lattice: malformed category metadata");
static_assert(isl_optimization_and_lattice_category.begin() == isl_optimization_and_lattice.data() && isl_optimization_and_lattice_category.size() == isl_optimization_and_lattice.size(),
              "isl_optimization_and_lattice: category storage mismatch");
static_assert(!isl_optimization_and_lattice.empty(), "isl_optimization_and_lattice: empty catalog");

}  // namespace metanum::catalog

#endif  // METANUM_ISL_OPTIMIZATION_AND_LATTICE_HPP

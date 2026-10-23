// MetaNum catalog: ppl.grid_operations
// Operations on grids (lattice-based sets).
//
// GENERATED from MANIFEST.yaml by gen_catalogs.py - edit the manifest, not this file.
// Describes the native PPL API (Parma Polyhedra Library); it does not reimplement or link it.
// Symbol names follow the manifest and may differ across PPL versions; check
// the installed upstream headers before relying on an exact symbol.
#ifndef METANUM_PPL_GRID_OPERATIONS_HPP
#define METANUM_PPL_GRID_OPERATIONS_HPP

#include "catalog_core.hpp"

namespace metanum::catalog {

inline constexpr std::array<operation, 6> ppl_grid_operations = {{
    operation{"grid_intersection", "Grid::intersection_assign", "Intersection of grids."},
    operation{"grid_convex_hull", "Grid::poly_hull_assign", "Convex hull of grids."},
    operation{"grid_difference", "Grid::difference_assign", "Set difference of grids."},
    operation{"grid_affine_image", "Grid::affine_image", "Affine image of a grid."},
    operation{"grid_widening", "Grid::limited_extrapolation_assign", "Grid widening."},
    operation{"grid_from_polyhedron", "Grid(Polyhedron)", "Approximate a polyhedron by a grid."},
}};

inline constexpr category ppl_grid_operations_category{
    "ppl.grid_operations", library::ppl, "Operations on grids (lattice-based sets).", ppl_grid_operations.data(), ppl_grid_operations.size()};

static_assert(detail::fields_well_formed(ppl_grid_operations), "ppl_grid_operations: empty or malformed name/symbol/description");
static_assert(detail::names_unique(ppl_grid_operations), "ppl_grid_operations: duplicate operation name");
static_assert(detail::symbols_well_formed(ppl_grid_operations), "ppl_grid_operations: malformed symbol list");
static_assert(detail::symbols_unique_per_operation(ppl_grid_operations), "ppl_grid_operations: symbol listed twice in one operation");
static_assert(detail::category_metadata_well_formed(ppl_grid_operations_category), "ppl_grid_operations: malformed category metadata");
static_assert(ppl_grid_operations_category.begin() == ppl_grid_operations.data() && ppl_grid_operations_category.size() == ppl_grid_operations.size(),
              "ppl_grid_operations: category storage mismatch");
static_assert(!ppl_grid_operations.empty(), "ppl_grid_operations: empty catalog");

}  // namespace metanum::catalog

#endif  // METANUM_PPL_GRID_OPERATIONS_HPP

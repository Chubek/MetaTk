// MetaNum catalog: ppl.polyhedra_constructors
// Constructing polyhedra and grids.
//
// GENERATED from MANIFEST.yaml by gen_catalogs.py - edit the manifest, not this file.
// Describes the native PPL API (Parma Polyhedra Library); it does not reimplement or link it.
// Symbol names follow the manifest and may differ across PPL versions; check
// the installed upstream headers before relying on an exact symbol.
#ifndef METANUM_PPL_POLYHEDRA_CONSTRUCTORS_HPP
#define METANUM_PPL_POLYHEDRA_CONSTRUCTORS_HPP

#include "catalog_core.hpp"

namespace metanum::catalog {

inline constexpr std::array<operation, 7> ppl_polyhedra_constructors = {{
    operation{"from_constraints", "C_Polyhedron(Constraint_System)", "Build a polyhedron from linear constraints (H-representation)."},
    operation{"from_generators", "C_Polyhedron(Generator_System)", "Build a polyhedron from generators (V-representation)."},
    operation{"empty_universe", "C_Polyhedron(dim, EMPTY | UNIVERSE)", "Empty or universe polyhedron of a given dimension."},
    operation{"nnc_polyhedron", "NNC_Polyhedron", "Polyhedra with non-closed (strict) constraints."},
    operation{"closed_polyhedron", "C_Polyhedron", "Closed polyhedra."},
    operation{"grid", "Grid", "Grids (lattice-like sets) represented by congruences or generators."},
    operation{"box_and_octagon", "Box, BD_Shape, Octagonal_Shape", "Interval, difference-bound, and octagonal abstract domains."},
}};

inline constexpr category ppl_polyhedra_constructors_category{
    "ppl.polyhedra_constructors", library::ppl, "Constructing polyhedra and grids.", ppl_polyhedra_constructors.data(), ppl_polyhedra_constructors.size()};

static_assert(detail::fields_well_formed(ppl_polyhedra_constructors), "ppl_polyhedra_constructors: empty or malformed name/symbol/description");
static_assert(detail::names_unique(ppl_polyhedra_constructors), "ppl_polyhedra_constructors: duplicate operation name");
static_assert(detail::symbols_well_formed(ppl_polyhedra_constructors), "ppl_polyhedra_constructors: malformed symbol list");
static_assert(detail::symbols_unique_per_operation(ppl_polyhedra_constructors), "ppl_polyhedra_constructors: symbol listed twice in one operation");
static_assert(detail::category_metadata_well_formed(ppl_polyhedra_constructors_category), "ppl_polyhedra_constructors: malformed category metadata");
static_assert(ppl_polyhedra_constructors_category.begin() == ppl_polyhedra_constructors.data() && ppl_polyhedra_constructors_category.size() == ppl_polyhedra_constructors.size(),
              "ppl_polyhedra_constructors: category storage mismatch");
static_assert(!ppl_polyhedra_constructors.empty(), "ppl_polyhedra_constructors: empty catalog");

}  // namespace metanum::catalog

#endif  // METANUM_PPL_POLYHEDRA_CONSTRUCTORS_HPP

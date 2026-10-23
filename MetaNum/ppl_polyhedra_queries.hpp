// MetaNum catalog: ppl.polyhedra_queries
// Predicates and properties on polyhedra.
//
// GENERATED from MANIFEST.yaml by gen_catalogs.py - edit the manifest, not this file.
// Describes the native PPL API (Parma Polyhedra Library); it does not reimplement or link it.
// Symbol names follow the manifest and may differ across PPL versions; check
// the installed upstream headers before relying on an exact symbol.
#ifndef METANUM_PPL_POLYHEDRA_QUERIES_HPP
#define METANUM_PPL_POLYHEDRA_QUERIES_HPP

#include "catalog_core.hpp"

namespace metanum::catalog {

inline constexpr std::array<operation, 14> ppl_polyhedra_queries = {{
    operation{"is_empty", "is_empty()", "Test whether a polyhedron is empty."},
    operation{"is_universe", "is_universe()", "Test whether a polyhedron is the whole space."},
    operation{"is_bounded", "is_bounded()", "Test boundedness."},
    operation{"is_discrete", "is_discrete()", "Test whether a polyhedron has finitely many points."},
    operation{"contains", "contains(const Polyhedron&)", "Test inclusion."},
    operation{"strictly_contains", "strictly_contains()", "Strict inclusion test."},
    operation{"relation_with", "relation_with(Constraint) / relation_with(Generator)", "Geometric relation between a polyhedron and a constraint or generator."},
    operation{"constrains", "constrains(Variable)", "Test whether a variable is constrained."},
    operation{"bounds_from_above", "bounds_from_above(Linear_Expression)", "Test whether an expression is bounded from above."},
    operation{"bounds_from_below", "bounds_from_below(Linear_Expression)", "Test whether an expression is bounded from below."},
    operation{"maximize", "maximize(Linear_Expression, num, den, closed)", "Maximize a linear objective over the polyhedron."},
    operation{"minimize", "minimize(Linear_Expression, num, den, closed)", "Minimize a linear objective over the polyhedron."},
    operation{"frequency", "frequency(Linear_Expression, value)", "Frequency of a value of an expression over lattice points."},
    operation{"bounded_check", "is_bounded / bounded_hull", "Bounded check utilities."},
}};

inline constexpr category ppl_polyhedra_queries_category{
    "ppl.polyhedra_queries", library::ppl, "Predicates and properties on polyhedra.", ppl_polyhedra_queries.data(), ppl_polyhedra_queries.size()};

static_assert(detail::fields_well_formed(ppl_polyhedra_queries), "ppl_polyhedra_queries: empty or malformed name/symbol/description");
static_assert(detail::names_unique(ppl_polyhedra_queries), "ppl_polyhedra_queries: duplicate operation name");
static_assert(detail::symbols_well_formed(ppl_polyhedra_queries), "ppl_polyhedra_queries: malformed symbol list");
static_assert(detail::symbols_unique_per_operation(ppl_polyhedra_queries), "ppl_polyhedra_queries: symbol listed twice in one operation");
static_assert(detail::category_metadata_well_formed(ppl_polyhedra_queries_category), "ppl_polyhedra_queries: malformed category metadata");
static_assert(ppl_polyhedra_queries_category.begin() == ppl_polyhedra_queries.data() && ppl_polyhedra_queries_category.size() == ppl_polyhedra_queries.size(),
              "ppl_polyhedra_queries: category storage mismatch");
static_assert(!ppl_polyhedra_queries.empty(), "ppl_polyhedra_queries: empty catalog");

}  // namespace metanum::catalog

#endif  // METANUM_PPL_POLYHEDRA_QUERIES_HPP

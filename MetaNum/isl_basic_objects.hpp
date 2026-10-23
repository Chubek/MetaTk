// MetaNum catalog: isl.basic_objects
// Creation, inspection, and memory management of integer sets and maps.
//
// GENERATED from MANIFEST.yaml by gen_catalogs.py - edit the manifest, not this file.
// Describes the native ISL API (Integer Set Library); it does not reimplement or link it.
// Symbol names follow the manifest and may differ across ISL versions; check
// the installed upstream headers before relying on an exact symbol.
#ifndef METANUM_ISL_BASIC_OBJECTS_HPP
#define METANUM_ISL_BASIC_OBJECTS_HPP

#include "catalog_core.hpp"

namespace metanum::catalog {

inline constexpr std::array<operation, 16> isl_basic_objects = {{
    operation{"create_context", "isl_ctx_alloc", "Allocate an ISL context used by all objects."},
    operation{"set_read_from_str", "isl_set_read_from_str", "Parse a set from its textual representation."},
    operation{"map_read_from_str", "isl_map_read_from_str", "Parse a map from its textual representation."},
    operation{"union_set_read_from_str", "isl_union_set_read_from_str", "Parse a union set."},
    operation{"union_map_read_from_str", "isl_union_map_read_from_str", "Parse a union map."},
    operation{"set_to_str", "isl_set_to_str", "Print a set in ISL notation."},
    operation{"dim", "isl_set_dim / isl_map_dim", "Query the number of dimensions of a set or map."},
    operation{"is_empty", "isl_set_is_empty / isl_map_is_empty", "Test whether a set or map has no integer points."},
    operation{"is_universe", "isl_set_is_universe", "Test whether a set contains every integer point."},
    operation{"is_equal", "isl_set_is_equal / isl_map_is_equal", "Test equality of two sets or maps."},
    operation{"is_subset", "isl_set_is_subset / isl_map_is_subset", "Test whether one set or map is contained in another."},
    operation{"is_disjoint", "isl_set_is_disjoint", "Test whether two sets have an empty intersection."},
    operation{"is_single_valued", "isl_map_is_single_valued", "Test whether a map is a function."},
    operation{"is_injective", "isl_map_is_injective", "Test whether a map is one-to-one."},
    operation{"is_bijective", "isl_map_is_bijective", "Test whether a map is a bijection."},
    operation{"copy_free", "isl_set_copy / isl_set_free", "Reference counting and deallocation."},
}};

inline constexpr category isl_basic_objects_category{
    "isl.basic_objects", library::isl, "Creation, inspection, and memory management of integer sets and maps.", isl_basic_objects.data(), isl_basic_objects.size()};

static_assert(detail::fields_well_formed(isl_basic_objects), "isl_basic_objects: empty or malformed name/symbol/description");
static_assert(detail::names_unique(isl_basic_objects), "isl_basic_objects: duplicate operation name");
static_assert(detail::symbols_well_formed(isl_basic_objects), "isl_basic_objects: malformed symbol list");
static_assert(detail::symbols_unique_per_operation(isl_basic_objects), "isl_basic_objects: symbol listed twice in one operation");
static_assert(detail::category_metadata_well_formed(isl_basic_objects_category), "isl_basic_objects: malformed category metadata");
static_assert(isl_basic_objects_category.begin() == isl_basic_objects.data() && isl_basic_objects_category.size() == isl_basic_objects.size(),
              "isl_basic_objects: category storage mismatch");
static_assert(!isl_basic_objects.empty(), "isl_basic_objects: empty catalog");

}  // namespace metanum::catalog

#endif  // METANUM_ISL_BASIC_OBJECTS_HPP

// MetaNum catalog: isl.set_algebra
// Boolean and structural operations on sets and maps.
//
// GENERATED from MANIFEST.yaml by gen_catalogs.py - edit the manifest, not this file.
// Describes the native ISL API (Integer Set Library); it does not reimplement or link it.
// Symbol names follow the manifest and may differ across ISL versions; check
// the installed upstream headers before relying on an exact symbol.
#ifndef METANUM_ISL_SET_ALGEBRA_HPP
#define METANUM_ISL_SET_ALGEBRA_HPP

#include "catalog_core.hpp"

namespace metanum::catalog {

inline constexpr std::array<operation, 16> isl_set_algebra = {{
    operation{"union", "isl_set_union / isl_map_union", "Set or map union."},
    operation{"intersect", "isl_set_intersect / isl_map_intersect", "Set or map intersection."},
    operation{"subtract", "isl_set_subtract / isl_map_subtract", "Set or map difference."},
    operation{"complement", "isl_set_complement", "Complement of a set."},
    operation{"product", "isl_set_product / isl_map_product", "Cartesian product."},
    operation{"flat_product", "isl_map_flat_product", "Product that flattens the range dimensions."},
    operation{"reverse", "isl_map_reverse", "Swap domain and range of a map."},
    operation{"domain", "isl_map_domain", "Project a map onto its domain."},
    operation{"range", "isl_map_range", "Project a map onto its range."},
    operation{"domain_map", "isl_map_domain_map", "Map each domain element to itself paired with its image."},
    operation{"range_map", "isl_map_range_map", "Map each range element to itself paired with its preimage."},
    operation{"deltas", "isl_map_deltas", "Set of differences between range and domain elements."},
    operation{"identity", "isl_map_identity", "Identity map on a set."},
    operation{"lex_lt / lex_le / lex_gt / lex_ge", "isl_map_lex_lt, isl_map_lex_le, isl_map_lex_gt, isl_map_lex_ge", "Lexicographic comparison maps."},
    operation{"universe", "isl_set_universe / isl_map_universe", "Universe set or map of a given space."},
    operation{"empty", "isl_set_empty / isl_map_empty", "Empty set or map of a given space."},
}};

inline constexpr category isl_set_algebra_category{
    "isl.set_algebra", library::isl, "Boolean and structural operations on sets and maps.", isl_set_algebra.data(), isl_set_algebra.size()};

static_assert(detail::fields_well_formed(isl_set_algebra), "isl_set_algebra: empty or malformed name/symbol/description");
static_assert(detail::names_unique(isl_set_algebra), "isl_set_algebra: duplicate operation name");
static_assert(detail::symbols_well_formed(isl_set_algebra), "isl_set_algebra: malformed symbol list");
static_assert(detail::symbols_unique_per_operation(isl_set_algebra), "isl_set_algebra: symbol listed twice in one operation");
static_assert(detail::category_metadata_well_formed(isl_set_algebra_category), "isl_set_algebra: malformed category metadata");
static_assert(isl_set_algebra_category.begin() == isl_set_algebra.data() && isl_set_algebra_category.size() == isl_set_algebra.size(),
              "isl_set_algebra: category storage mismatch");
static_assert(!isl_set_algebra.empty(), "isl_set_algebra: empty catalog");

}  // namespace metanum::catalog

#endif  // METANUM_ISL_SET_ALGEBRA_HPP

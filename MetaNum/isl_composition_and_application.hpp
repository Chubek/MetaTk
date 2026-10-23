// MetaNum catalog: isl.composition_and_application
// Composition, application, and preimage of maps.
//
// GENERATED from MANIFEST.yaml by gen_catalogs.py - edit the manifest, not this file.
// Describes the native ISL API (Integer Set Library); it does not reimplement or link it.
// Symbol names follow the manifest and may differ across ISL versions; check
// the installed upstream headers before relying on an exact symbol.
#ifndef METANUM_ISL_COMPOSITION_AND_APPLICATION_HPP
#define METANUM_ISL_COMPOSITION_AND_APPLICATION_HPP

#include "catalog_core.hpp"

namespace metanum::catalog {

inline constexpr std::array<operation, 10> isl_composition_and_application = {{
    operation{"apply", "isl_set_apply / isl_union_set_apply", "Image of a set under a map."},
    operation{"apply_domain", "isl_map_apply_domain", "Compose a map with the domain of another."},
    operation{"apply_range", "isl_map_apply_range", "Compose a map with the range of another."},
    operation{"preimage_domain", "isl_map_preimage_domain_multi_aff", "Pull back a map through an affine function on its domain."},
    operation{"preimage_range", "isl_map_preimage_range_multi_aff", "Pull back a map through an affine function on its range."},
    operation{"fixed_power", "isl_map_fixed_power", "Compute the n-th power of a map for symbolic or fixed n."},
    operation{"transitive_closure", "isl_map_transitive_closure", "Transitive closure of a map, computed exactly when possible."},
    operation{"reaching_path_lengths", "isl_map_reaching_path_lengths", "Lengths of paths in a relation."},
    operation{"lexmin_of_relation", "isl_map_lexmin", "Lexicographically smallest elements of a map."},
    operation{"lexmax_of_relation", "isl_map_lexmax", "Lexicographically largest elements of a map."},
}};

inline constexpr category isl_composition_and_application_category{
    "isl.composition_and_application", library::isl, "Composition, application, and preimage of maps.", isl_composition_and_application.data(), isl_composition_and_application.size()};

static_assert(detail::fields_well_formed(isl_composition_and_application), "isl_composition_and_application: empty or malformed name/symbol/description");
static_assert(detail::names_unique(isl_composition_and_application), "isl_composition_and_application: duplicate operation name");
static_assert(detail::symbols_well_formed(isl_composition_and_application), "isl_composition_and_application: malformed symbol list");
static_assert(detail::symbols_unique_per_operation(isl_composition_and_application), "isl_composition_and_application: symbol listed twice in one operation");
static_assert(detail::category_metadata_well_formed(isl_composition_and_application_category), "isl_composition_and_application: malformed category metadata");
static_assert(isl_composition_and_application_category.begin() == isl_composition_and_application.data() && isl_composition_and_application_category.size() == isl_composition_and_application.size(),
              "isl_composition_and_application: category storage mismatch");
static_assert(!isl_composition_and_application.empty(), "isl_composition_and_application: empty catalog");

}  // namespace metanum::catalog

#endif  // METANUM_ISL_COMPOSITION_AND_APPLICATION_HPP

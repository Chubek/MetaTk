// MetaNum catalog: isl.printing_and_io
// Serialization of objects.
//
// GENERATED from MANIFEST.yaml by gen_catalogs.py - edit the manifest, not this file.
// Describes the native ISL API (Integer Set Library); it does not reimplement or link it.
// Symbol names follow the manifest and may differ across ISL versions; check
// the installed upstream headers before relying on an exact symbol.
#ifndef METANUM_ISL_PRINTING_AND_IO_HPP
#define METANUM_ISL_PRINTING_AND_IO_HPP

#include "catalog_core.hpp"

namespace metanum::catalog {

inline constexpr std::array<operation, 2> isl_printing_and_io = {{
    operation{"print_isl_format", "isl_printer_*", "ISL, C, Omega, and LaTeX-style output."},
    operation{"read_from_file", "isl_*_read_from_file", "Read objects from a file."},
}};

inline constexpr category isl_printing_and_io_category{
    "isl.printing_and_io", library::isl, "Serialization of objects.", isl_printing_and_io.data(), isl_printing_and_io.size()};

static_assert(detail::fields_well_formed(isl_printing_and_io), "isl_printing_and_io: empty or malformed name/symbol/description");
static_assert(detail::names_unique(isl_printing_and_io), "isl_printing_and_io: duplicate operation name");
static_assert(detail::symbols_well_formed(isl_printing_and_io), "isl_printing_and_io: malformed symbol list");
static_assert(detail::symbols_unique_per_operation(isl_printing_and_io), "isl_printing_and_io: symbol listed twice in one operation");
static_assert(detail::category_metadata_well_formed(isl_printing_and_io_category), "isl_printing_and_io: malformed category metadata");
static_assert(isl_printing_and_io_category.begin() == isl_printing_and_io.data() && isl_printing_and_io_category.size() == isl_printing_and_io.size(),
              "isl_printing_and_io: category storage mismatch");
static_assert(!isl_printing_and_io.empty(), "isl_printing_and_io: empty catalog");

}  // namespace metanum::catalog

#endif  // METANUM_ISL_PRINTING_AND_IO_HPP

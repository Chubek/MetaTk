#!/usr/bin/env python3
"""Generate MetaNum catalog headers from MANIFEST.yaml.

usage: gen_catalogs.py <MetaNum dir>
"""
import re
import sys
from pathlib import Path

import yaml

root = Path(sys.argv[1])
manifest = yaml.safe_load((root / "MANIFEST.yaml").read_text())["manifest"]
LIBS = {lib["id"] for lib in manifest["libraries"]}
ID_RE = re.compile(r"^[a-z][a-z0-9_]*\.[a-z][a-z0-9_]*$")


def cxx(s: str) -> str:
    s = " ".join(str(s).split())  # collapse whitespace / newlines
    if not all(0x20 <= ord(c) <= 0x7E for c in s):
        raise SystemExit(f"non-printable/non-ASCII text in manifest: {s!r}")
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


def split_symbols(s: str):
    out, depth, cur = [], 0, ""
    for c in s:
        if c == "(":
            depth += 1
        elif c == ")":
            depth = max(0, depth - 1)
        if depth == 0 and c in ",/":
            out.append(cur.strip()); cur = ""
        else:
            cur += c
    out.append(cur.strip())
    return out


def dedupe_symbols(s: str):
    toks = split_symbols(s)
    seen, keep = set(), []
    for t in toks:
        if t not in seen:
            seen.add(t); keep.append(t)
    if len(keep) == len(toks):
        return s, []
    dropped = [t for t in toks]
    for t in keep:
        dropped.remove(t)
    return ", ".join(keep), dropped


HEADER = """\
// MetaNum catalog: {id}
// {desc}
//
// GENERATED from MANIFEST.yaml by gen_catalogs.py - edit the manifest, not this file.
// Describes the native {LIB} API ({full}); it does not reimplement or link it.
// Symbol names follow the manifest and may differ across {LIB} versions; check
// the installed upstream headers before relying on an exact symbol.
#ifndef {guard}
#define {guard}

#include "catalog_core.hpp"

namespace metanum::catalog {{

inline constexpr std::array<operation, {n}> {var} = {{{{
{rows}
}}}};

inline constexpr category {var}_category{{
    {id_lit}, library::{lib}, {desc_lit}, {var}.data(), {var}.size()}};

static_assert(detail::fields_well_formed({var}), "{var}: empty or malformed name/symbol/description");
static_assert(detail::names_unique({var}), "{var}: duplicate operation name");
static_assert(detail::symbols_well_formed({var}), "{var}: malformed symbol list");
static_assert(detail::symbols_unique_per_operation({var}), "{var}: symbol listed twice in one operation");
static_assert(detail::category_metadata_well_formed({var}_category), "{var}: malformed category metadata");
static_assert({var}_category.begin() == {var}.data() && {var}_category.size() == {var}.size(),
              "{var}: category storage mismatch");
static_assert(!{var}.empty(), "{var}: empty catalog");

}}  // namespace metanum::catalog

#endif  // {guard}
"""

cats, notes, total = [], [], 0
ids = set()
for c in manifest["categories"]:
    cid, lib = c["id"], c["library"]
    if not ID_RE.match(cid) or cid in ids or lib not in LIBS or not cid.startswith(lib + "."):
        raise SystemExit(f"bad category id/library: {cid} / {lib}")
    ids.add(cid)
    var = cid.replace(".", "_")
    names = set()
    rows = []
    for op in c["operations"]:
        if op["name"] in names:
            raise SystemExit(f"duplicate op {op['name']} in {cid}")
        names.add(op["name"])
        sym, dropped = dedupe_symbols(op["symbol"])
        if dropped:
            notes.append(f"{cid}/{op['name']}: removed repeated symbol(s) {dropped}")
            rows.append(f"    // Manifest repeats {', '.join(dropped)}; listed once here.")
        rows.append(f"    operation{{{cxx(op['name'])}, {cxx(sym)}, {cxx(op['description'])}}},")
    n = len(rows) - sum(r.lstrip().startswith("//") for r in rows)
    total += n
    full = next(l["full_name"] for l in manifest["libraries"] if l["id"] == lib)
    text = HEADER.format(
        id=cid, desc=" ".join(c["description"].split()), LIB=lib.upper(), full=full,
        guard=f"METANUM_{var.upper()}_HPP", n=n, var=var, rows="\n".join(rows),
        id_lit=cxx(cid), lib=lib, desc_lit=cxx(c["description"]))
    (root / f"{var}.hpp").write_text(text)
    cats.append((var, lib))

# all.hpp
inc = "\n".join(f'#include "{v}.hpp"' for v, _ in cats)
reg = "\n".join(f"    &{v}_category," for v, _ in cats)
ALL = f"""\
// MetaNum umbrella header.
//
// GENERATED from MANIFEST.yaml by gen_catalogs.py - edit the manifest, not this file.
// Pulls in the numeric routines (numeric.hpp) and every operation catalog, and
// exposes a registry of all {len(cats)} categories / {total} operations.
#ifndef METANUM_ALL_HPP
#define METANUM_ALL_HPP

#include "numeric.hpp"
#include "catalog_core.hpp"

{inc}

namespace metanum::catalog {{

/// MANIFEST.yaml `manifest.version` these headers were generated from.
inline constexpr int manifest_version = {int(manifest["version"])};

/// Every category, in manifest order.
inline constexpr std::array<const category*, {len(cats)}> all_categories = {{{{
{reg}
}}}};

/// Total number of operations across all categories.
inline constexpr std::size_t total_operations = [] {{
    std::size_t n = 0;
    for (const category* c : all_categories) n += c->size();
    return n;
}}();

/// Look up a category by manifest id (e.g. "gsl.fft"); nullptr if absent.
[[nodiscard]] constexpr const category* find_category(std::string_view id) noexcept {{
    for (const category* c : all_categories)
        if (c->id == id) return c;
    return nullptr;
}}

/// Look up an operation by category id and operation name; nullptr if absent.
[[nodiscard]] constexpr const operation* find_operation(std::string_view category_id,
                                                        std::string_view name) noexcept {{
    for (const category* c : all_categories)
        if (c->id == category_id) return find(*c, name);
    return nullptr;
}}

namespace detail {{
constexpr bool category_ids_unique() noexcept {{
    for (std::size_t i = 0; i < all_categories.size(); ++i)
        for (std::size_t j = i + 1; j < all_categories.size(); ++j)
            if (all_categories[i]->id == all_categories[j]->id) return false;
    return true;
}}
constexpr bool category_prefixes_match_library() noexcept {{
    for (const category* c : all_categories) {{
        if (!category_metadata_well_formed(*c)) return false;
    }}
    return true;
}}
}}  // namespace detail

static_assert(total_operations == {total}, "operation total differs from MANIFEST.yaml");
static_assert(detail::category_ids_unique(), "duplicate category id");
static_assert(detail::category_prefixes_match_library(), "category id does not match its library");

}}  // namespace metanum::catalog

#endif  // METANUM_ALL_HPP
"""
(root / "all.hpp").write_text(ALL)
print(f"{len(cats)} categories, {total} operations")
for n in notes:
    print("NOTE:", n)

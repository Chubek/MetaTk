/**
 * @file metalib/cpp/Table.h
 * @brief MetaML host module `Table` (C++ dialect) — mutable string-keyed hash tables.
 *
 * Primitives (identical names, types and behaviour in metalib/d/Table.d):
 *   Table.clone : Table 'v -> Table 'v             shallow independent map
 *   Table.values : Table 'v -> ['v]                sorted-key order
 *   Table.set_if_absent : String -> 'v -> Table 'v -> Bool   atomic insertion
 *   Table.create : Unit -> Table 'v
 *   Table.set : String -> 'v -> Table 'v -> Unit
 *   Table.get : String -> Table 'v -> Option 'v
 *   Table.has : String -> Table 'v -> Bool
 *   Table.remove : String -> Table 'v -> Unit
 *   Table.size : Table 'v -> Int
 *   Table.keys : Table 'v -> [String]              sorted
 *   Table.to_list : Table 'v -> [(String, 'v)]     sorted by key
 *   Table.of_list : [(String, 'v)] -> Table 'v
 *   Table.clear : Table 'v -> Unit
 *
 * Header-only: this header includes Table.c, the C++ implementation.
 */
#pragma once

#include "Support.h"

namespace metalib
{
/// Registers the `Table.*` primitives (sandboxed; see §10.4).
inline void install_Table (metaml::Session &s);
} // namespace metalib

#include "Table.c"

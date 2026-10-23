/**
 * @file metalib/cpp/Loader.h
 * @brief MetaML host module `Loader` (C++ dialect) — dynamic loading and module search paths.
 *
 * Primitives (identical names, types and behaviour in metalib/d/Loader.d):
 *   Loader.load : String -> Result Unit String
 *   Loader.resolve : String -> Option String
 *   Loader.search_path : Unit -> [String]
 *   Loader.add_path : String -> Unit
 *   Loader.stdlib : Unit -> Option String
 *   Loader.loaded : Unit -> [String]
 *
 * Header-only: this header includes Loader.c, the C++ implementation.
 */
#pragma once

#include "Support.h"
#include "Frontend.h"

namespace metalib
{
/// Registers the `Loader.*` primitives (sandboxed; see §10.4).
inline void install_Loader (metaml::Session &s);
} // namespace metalib

#include "Loader.c"

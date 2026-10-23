/**
 * @file metalib/cpp/Module.h
 * @brief MetaML host module `Module` (C++ dialect) — introspection of the modules known to the checker.
 *
 * Primitives (identical names, types and behaviour in metalib/d/Module.d):
 *   Module.names : Unit -> [String]
 *   Module.functors : Unit -> [String]
 *   Module.signatures : Unit -> [String]
 *   Module.exists : String -> Bool
 *   Module.members : String -> Option [String]
 *   Module.types : String -> Option [String]
 *   Module.submodules : String -> Option [String]
 *   Module.member_type : String -> String -> Option String
 *   Module.signature : String -> Option String
 *
 * Header-only: this header includes Module.c, the C++ implementation.
 */
#pragma once

#include "Support.h"

namespace metalib
{
/// Registers the `Module.*` primitives (sandboxed; see §10.4).
inline void install_Module (metaml::Session &s);
} // namespace metalib

#include "Module.c"

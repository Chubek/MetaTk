/**
 * @file metalib/cpp/Type.h
 * @brief MetaML host module `Type` (C++ dialect) — type expressions: normalisation, unification, instance checks.
 *
 * Primitives (identical names, types and behaviour in metalib/d/Type.d):
 *   Type.normalize : String -> Result String String
 *   Type.unify : String -> String -> Result String String
 *   Type.instance_of : String -> String -> Bool
 *   Type.equal : String -> String -> Bool
 *   Type.arity : String -> Result Int String
 *
 * Header-only: this header includes Type.c, the C++ implementation.
 */
#pragma once

#include "Support.h"
#include "Frontend.h"

namespace metalib
{
/// Registers the `Type.*` primitives (sandboxed; see §10.4).
inline void install_Type (metaml::Session &s);
} // namespace metalib

#include "Type.c"

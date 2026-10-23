/**
 * @file metalib/cpp/SExpr.h
 * @brief MetaML host module `SExpr` (C++ dialect) — S-expressions (DSLtk reader and printer).
 *
 * Primitives (identical names, types and behaviour in metalib/d/SExpr.d):
 *   type SExpr := | Atom String | SList [SExpr]
 *   SExpr.parse : String -> Result [SExpr] String
 *   SExpr.parse_one : String -> Result SExpr String
 *   SExpr.render : SExpr -> String
 *   SExpr.atom_text : SExpr -> Option String
 *
 * Header-only: this header includes SExpr.c, the C++ implementation.
 */
#pragma once

#include "Support.h"

namespace metalib
{
/// Registers the `SExpr.*` primitives (sandboxed; see §10.4).
inline void install_SExpr (metaml::Session &s);
} // namespace metalib

#include "SExpr.c"

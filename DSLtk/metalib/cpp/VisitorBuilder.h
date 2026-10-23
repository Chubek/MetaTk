/**
 * @file metalib/cpp/VisitorBuilder.h
 * @brief MetaML host module `VisitorBuilder` (C++ dialect) — build visitors / folds over `Ast` (functions made with `make_function`).
 *
 * Primitives (identical names, types and behaviour in metalib/d/VisitorBuilder.d):
 *   VisitorBuilder.make : (String -> String -> 'r) -> (String -> ['r] -> 'r) -> (Ast -> 'r)
 *   VisitorBuilder.fold : (String -> String -> 'r) -> (String -> ['r] -> 'r) -> Ast -> 'r
 *   VisitorBuilder.dispatch : [(String, Ast -> 'r)] -> (Ast -> 'r) -> Ast -> 'r
 *   VisitorBuilder.count_tags : Ast -> [(String, Int)]
 *   VisitorBuilder.tags : Ast -> [String]
 *
 * Header-only: this header includes VisitorBuilder.c, the C++ implementation.
 */
#pragma once

#include "Support.h"
#include "ASTBuilder.h"

namespace metalib
{
/// Registers the `VisitorBuilder.*` primitives (sandboxed; see §10.4).
inline void install_VisitorBuilder (metaml::Session &s);
} // namespace metalib

#include "VisitorBuilder.c"

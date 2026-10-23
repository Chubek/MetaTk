/**
 * @file metalib/cpp/WalkVisitor.h
 * @brief MetaML host module `WalkVisitor` (C++ dialect) — callback-driven walks over `Ast` trees.
 *
 * Primitives (identical names, types and behaviour in metalib/d/WalkVisitor.d):
 *   WalkVisitor.walk : (Ast -> Unit) -> Ast -> Unit
 *   WalkVisitor.walk_depth : (Int -> Ast -> Unit) -> Ast -> Unit
 *   WalkVisitor.enter_leave : (Ast -> Unit) -> (Ast -> Unit) -> Ast -> Unit
 *   WalkVisitor.path_to : (Ast -> Bool) -> Ast -> Option [Int]
 *   WalkVisitor.at_path : [Int] -> Ast -> Option Ast
 *
 * Header-only: this header includes WalkVisitor.c, the C++ implementation.
 */
#pragma once

#include "Support.h"
#include "ASTBuilder.h"

namespace metalib
{
/// Registers the `WalkVisitor.*` primitives (sandboxed; see §10.4).
inline void install_WalkVisitor (metaml::Session &s);
} // namespace metalib

#include "WalkVisitor.c"

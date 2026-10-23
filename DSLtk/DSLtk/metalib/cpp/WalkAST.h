/**
 * @file metalib/cpp/WalkAST.h
 * @brief MetaML host module `WalkAST` (C++ dialect) — traversals and rewriting of `Ast` trees.
 *
 * Primitives (identical names, types and behaviour in metalib/d/WalkAST.d):
 *   WalkAST.preorder : Ast -> [Ast]
 *   WalkAST.postorder : Ast -> [Ast]
 *   WalkAST.breadth_first : Ast -> [Ast]
 *   WalkAST.leaves : Ast -> [Ast]
 *   WalkAST.map_bottom_up : (Ast -> Ast) -> Ast -> Ast
 *   WalkAST.map_top_down : (Ast -> Ast) -> Ast -> Ast
 *   WalkAST.rewrite : (Ast -> Option Ast) -> Ast -> Ast
 *   WalkAST.find_all : (Ast -> Bool) -> Ast -> [Ast]
 *
 * Header-only: this header includes WalkAST.c, the C++ implementation.
 */
#pragma once

#include "Support.h"
#include "ASTBuilder.h"

namespace metalib
{
/// Registers the `WalkAST.*` primitives (sandboxed; see §10.4).
inline void install_WalkAST (metaml::Session &s);
} // namespace metalib

#include "WalkAST.c"

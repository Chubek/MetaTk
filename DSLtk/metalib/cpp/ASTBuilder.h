/**
 * @file metalib/cpp/ASTBuilder.h
 * @brief MetaML host module `ASTBuilder` (C++ dialect) — generic syntax trees (`Ast`) bridged to DSLtk `ASTNode`.
 *
 * Primitives (identical names, types and behaviour in metalib/d/ASTBuilder.d):
 *   type Ast := | Leaf String String | Node String [Ast]
 *   ASTBuilder.leaf : String -> String -> Ast
 *   ASTBuilder.node : String -> [Ast] -> Ast
 *   ASTBuilder.dump : Ast -> String
 *   ASTBuilder.pretty : Ast -> String
 *   ASTBuilder.tag : Ast -> String
 *   ASTBuilder.text : Ast -> Option String
 *   ASTBuilder.children : Ast -> [Ast]
 *   ASTBuilder.is_leaf : Ast -> Bool
 *   ASTBuilder.size : Ast -> Int
 *   ASTBuilder.depth : Ast -> Int
 *   ASTBuilder.find : String -> Ast -> [Ast]
 *   ASTBuilder.with_children : [Ast] -> Ast -> Ast
 *   ASTBuilder.to_sexpr : Ast -> SExpr
 *   ASTBuilder.of_sexpr : SExpr -> Result Ast String
 *   ASTBuilder.parse : String -> Result Ast String
 *
 * Header-only: this header includes ASTBuilder.c, the C++ implementation.
 */
#pragma once

#include "Support.h"

namespace metalib
{
/// Registers the `ASTBuilder.*` primitives (sandboxed; see §10.4).
inline void install_ASTBuilder (metaml::Session &s);
} // namespace metalib

#include "ASTBuilder.c"

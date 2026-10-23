/**
 * @file metalib/cpp/YACCBridge.h
 * @brief MetaML host module `YACCBridge` (C++ dialect) — yacc-style grammars parsed with DSLtk Earley `CFG` into `Ast`.
 *
 * Primitives (identical names, types and behaviour in metalib/d/YACCBridge.d):
 *   YACCBridge.check : String -> Result [String] String
 *   YACCBridge.literals : String -> Result [String] String
 *   YACCBridge.parse : String -> String -> Result Ast String
 *   YACCBridge.parse_tokens : String -> [(String, String)] -> Result Ast String
 *
 *   Grammar: `rule : sym sym | sym ;` (first rule is the start symbol). A name
 *   with rules is a nonterminal, any other name is a token kind; "x" or 'x' is
 *   a literal. `parse` lexes input into literals, NUM, ID and STR tokens.
 *   Tokens become `Leaf kind text`; nonterminals become `Node rule [...]`.
 *
 * Header-only: this header includes YACCBridge.c, the C++ implementation.
 */
#pragma once

#include "Support.h"
#include "ASTBuilder.h"

namespace metalib
{
/// Registers the `YACCBridge.*` primitives (sandboxed; see §10.4).
inline void install_YACCBridge (metaml::Session &s);
} // namespace metalib

#include "YACCBridge.c"

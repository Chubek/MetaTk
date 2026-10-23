/**
 * @file metalib/cpp/Frontend.h
 * @brief MetaML host module `Frontend` (C++ dialect) — the MetaML lexer, parser and checker as library functions.
 *
 * Primitives (identical names, types and behaviour in metalib/d/Frontend.d):
 *   Frontend.tokens : String -> Result [(String, String)] String
 *   Frontend.parse : String -> Result String String
 *   Frontend.parse_expr : String -> Result String String
 *   Frontend.type_of : String -> Result String String
 *   Frontend.format_type : String -> Result String String
 *   Frontend.eval : String -> Result String String
 *   Frontend.keywords : Unit -> [String]
 *
 *   `eval` runs in a fresh sandbox session (core primitives only, no metalib);
 *   its output is forwarded to the calling session.
 *
 * Header-only: this header includes Frontend.c, the C++ implementation.
 */
#pragma once

#include "Support.h"

namespace metalib
{
/// Registers the `Frontend.*` primitives (sandboxed; see §10.4).
inline void install_Frontend (metaml::Session &s);
} // namespace metalib

#include "Frontend.c"

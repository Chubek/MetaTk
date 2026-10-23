/**
 * @file metalib/cpp/FlexBridge.h
 * @brief MetaML host module `FlexBridge` (C++ dialect) — flex-style longest-match tokenizer (DSLtk `CompiledPattern`).
 *
 * Primitives (identical names, types and behaviour in metalib/d/FlexBridge.d):
 *   FlexBridge.tokenize : [(String, String)] -> String -> Result [(String, String)] String
 *   FlexBridge.tokenize_spans : [(String, String)] -> String -> Result [(String, String, Int)] String
 *   FlexBridge.valid_pattern : String -> Result Unit String
 *   FlexBridge.match_prefix : String -> String -> Option String
 *   FlexBridge.matches : String -> String -> Bool
 *
 *   Patterns: literals, `.`, `[a-z]`, `[^...]`, quantifiers `* + ?` and `!` (negative
 *   lookahead), escapes `\n \t \r \d \w \s \X`. Matching is greedy without
 *   backtracking. Rules whose name starts with `_` are skipped; the longest
 *   match wins and ties go to the earlier rule.
 *
 * Header-only: this header includes FlexBridge.c, the C++ implementation.
 */
#pragma once

#include "Support.h"

namespace metalib
{
/// Registers the `FlexBridge.*` primitives (sandboxed; see §10.4).
inline void install_FlexBridge (metaml::Session &s);
} // namespace metalib

#include "FlexBridge.c"

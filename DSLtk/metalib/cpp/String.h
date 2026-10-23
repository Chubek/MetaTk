/**
 * @file metalib/cpp/String.h
 * @brief MetaML host module `String` (C++ dialect) — Unicode-aware and formatting string utilities.
 *
 * Primitives (identical names, types and behaviour in metalib/d/String.d):
 *   String.utf8_length : String -> Int            code points (invalid bytes count as one)
 *   String.utf8_chars : String -> [String]        split into code points
 *   String.utf8_valid : String -> Bool
 *   String.codepoints : String -> [Int]
 *   String.of_codepoints : [Int] -> String        invalid code points become U+FFFD
 *   String.pad_left : Int -> String -> String -> String   (width, fill, s)
 *   String.pad_right : Int -> String -> String -> String
 *   String.words : String -> [String]
 *   String.last_index_of : String -> String -> Option Int  (needle, s)
 *   String.count : String -> String -> Int        non-overlapping occurrences
 *   String.strip_prefix : String -> String -> Option String
 *   String.strip_suffix : String -> String -> Option String
 *   String.capitalize : String -> String
 *   String.is_digit / is_alpha / is_space / is_alnum : String -> Bool
 *   String.format : String -> [String] -> String  `{}` placeholders, `{{` and `}}` escapes
 *   String.escape : String -> String              MetaML escapes, no quotes
 *   String.compare_ci : String -> String -> Int   ASCII case-insensitive
 *
 * Header-only: this header includes String.c, the C++ implementation.
 */
#pragma once

#include "Support.h"

namespace metalib
{
/// Registers the `String.*` primitives (sandboxed; see §10.4).
inline void install_String (metaml::Session &s);
} // namespace metalib

#include "String.c"

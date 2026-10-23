/**
 * @file metalib/cpp/URI.h
 * @brief MetaML host module `URI` (C++ dialect) — RFC 3986 URI parsing, rendering and percent-encoding.
 *
 * Primitives (identical names, types and behaviour in metalib/d/URI.d):
 *   type Uri = { scheme : String, user : String, host : String, port : Int,
 *                path : String, query : String, fragment : String }   (port -1 if absent)
 *   URI.parse : String -> Option Uri
 *   URI.render : Uri -> String
 *   URI.encode : String -> String                  percent-encode all but unreserved
 *   URI.decode : String -> Option String           None on malformed escapes
 *   URI.query_params : String -> [(String, String)]  `a=1&b=x+y` (decoded; `+` is a space)
 *
 * Header-only: this header includes URI.c, the C++ implementation.
 */
#pragma once

#include "Support.h"

namespace metalib
{
/// Registers the `URI.*` primitives (sandboxed; see §10.4).
inline void install_URI (metaml::Session &s);
} // namespace metalib

#include "URI.c"

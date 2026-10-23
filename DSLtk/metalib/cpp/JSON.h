/**
 * @file metalib/cpp/JSON.h
 * @brief MetaML host module `JSON` (C++ dialect) — JSON parsing and rendering.
 *
 * Primitives (identical names, types and behaviour in metalib/d/JSON.d):
 *   type Json := | JNull | JBool Bool | JNum Float | JStr String | JArr [Json] | JObj [(String, Json)]
 *   JSON.parse : String -> Result Json String
 *   JSON.render : Json -> String          compact
 *   JSON.pretty : Json -> String          two-space indentation
 *   JSON.get : String -> Json -> Option Json   object member
 *   JSON.at : Int -> Json -> Option Json       array element
 *   JSON.of_value : 'a -> Json            any MetaML value (records become objects)
 *
 * Header-only: this header includes JSON.c, the C++ implementation.
 */
#pragma once

#include "Support.h"

namespace metalib
{
/// Registers the `JSON.*` primitives (sandboxed; see §10.4).
inline void install_JSON (metaml::Session &s);
} // namespace metalib

#include "JSON.c"

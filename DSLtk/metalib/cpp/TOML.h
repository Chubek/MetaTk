/**
 * @file metalib/cpp/TOML.h
 * @brief MetaML host module `TOML` (C++ dialect) — TOML parsing and rendering (TOML 1.0 subset; values map onto `Json`).
 *
 * Primitives (identical names, types and behaviour in metalib/d/TOML.d):
 *   TOML.parse : String -> Result Json String     tables, arrays of tables, dotted keys, inline
 *                                                 tables, all string forms; dates stay strings
 *   TOML.render : Json -> Result String String    the top level must be an object
 *
 * Header-only: this header includes TOML.c, the C++ implementation.
 */
#pragma once

#include "Support.h"
#include "JSON.h"

namespace metalib
{
/// Registers the `TOML.*` primitives (sandboxed; see §10.4).
inline void install_TOML (metaml::Session &s);
} // namespace metalib

#include "TOML.c"

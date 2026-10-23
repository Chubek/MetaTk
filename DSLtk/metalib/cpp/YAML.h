/**
 * @file metalib/cpp/YAML.h
 * @brief MetaML host module `YAML` (C++ dialect) — YAML parsing and rendering (block/flow subset; values map onto `Json`).
 *
 * Primitives (identical names, types and behaviour in metalib/d/YAML.d):
 *   YAML.parse : String -> Result Json String   mappings, sequences, flow collections, quoted
 *                                               and plain scalars, block scalars (| and >);
 *                                               anchors, aliases and tags are rejected
 *   YAML.render : Json -> String
 *
 * Header-only: this header includes YAML.c, the C++ implementation.
 */
#pragma once

#include "Support.h"
#include "JSON.h"

namespace metalib
{
/// Registers the `YAML.*` primitives (sandboxed; see §10.4).
inline void install_YAML (metaml::Session &s);
} // namespace metalib

#include "YAML.c"

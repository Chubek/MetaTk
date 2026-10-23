/**
 * @file metalib/cpp/Graph.h
 * @brief MetaML host module `Graph` (C++ dialect) — graph algorithms over edge lists (DSLtk `DirectedGraph`).
 *
 * Primitives (identical names, types and behaviour in metalib/d/Graph.d):
 *   Graph.nodes : [(String, String)] -> [String]
 *   Graph.successors : String -> [(String, String)] -> [String]
 *   Graph.predecessors : String -> [(String, String)] -> [String]
 *   Graph.topo_sort : [String] -> [(String, String)] -> Result [String] String
 *   Graph.reachable : String -> [(String, String)] -> [String]
 *   Graph.depth_first : String -> [(String, String)] -> [String]
 *   Graph.shortest_path : String -> String -> [(String, String)] -> Option [String]
 *   Graph.sccs : [String] -> [(String, String)] -> [[String]]
 *   Graph.has_cycle : [(String, String)] -> Bool
 *   Graph.to_dot : String -> [String] -> [(String, String)] -> String
 *
 * Header-only: this header includes Graph.c, the C++ implementation.
 */
#pragma once

#include "Support.h"

namespace metalib
{
/// Registers the `Graph.*` primitives (sandboxed; see §10.4).
inline void install_Graph (metaml::Session &s);
} // namespace metalib

#include "Graph.c"

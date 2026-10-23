/**
 * @file metalib/cpp/List.h
 * @brief MetaML host module `List` (C++ dialect) — host-accelerated list utilities.
 *
 * Primitives (identical names, types and behaviour in metalib/d/List.d):
 *   List.take_while : ('a -> Bool) -> ['a] -> ['a]
 *   List.drop_while : ('a -> Bool) -> ['a] -> ['a]
 *   List.chunks : Int -> ['a] -> [['a]]           (size must be positive)
 *   List.windows : Int -> ['a] -> [['a]]          sliding windows
 *   List.group : ['a] -> [['a]]                   runs of equal elements
 *   List.flatten : [['a]] -> ['a]
 *   List.index_of : 'a -> ['a] -> Option Int
 *   List.transpose : [['a]] -> [['a]]             (stops at the shortest row)
 *   List.scan : ('b -> 'a -> 'b) -> 'b -> ['a] -> ['b]   includes the initial value
 *   List.iterate : Int -> ('a -> 'a) -> 'a -> ['a]
 *   List.zip3 : ['a] -> ['b] -> ['c] -> [('a, 'b, 'c)]
 *   List.rotate : Int -> ['a] -> ['a]             left rotation
 *   List.shuffle : Int -> ['a] -> ['a]            deterministic (seeded xorshift64*)
 *   List.max_by : ('a -> 'a -> Int) -> ['a] -> Option 'a
 *   List.min_by : ('a -> 'a -> Int) -> ['a] -> Option 'a
 *
 * Header-only: this header includes List.c, the C++ implementation.
 */
#pragma once

#include "Support.h"

namespace metalib
{
/// Registers the `List.*` primitives (sandboxed; see §10.4).
inline void install_List (metaml::Session &s);
} // namespace metalib

#include "List.c"

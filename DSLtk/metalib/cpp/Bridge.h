/**
 * @file metalib/cpp/Bridge.h
 * @brief MetaML host module `Bridge` (C++ dialect) — runtime inspection and marshaling of MetaML values for host code.
 *
 * Primitives (identical names, types and behaviour in metalib/d/Bridge.d):
 *   Bridge.kind : 'a -> String           Int Float String Bool Unit List Tuple Record Ctor Function Module Foreign
 *   Bridge.encode : 'a -> String         canonical text (same as Core.show)
 *   Bridge.fields : 'a -> [String]       record labels (sorted) or []
 *   Bridge.ctor_name : 'a -> Option String
 *   Bridge.arity : 'a -> Int             element/field/argument count (0 for scalars)
 *   Bridge.size : 'a -> Int              number of nodes in the value tree
 *   Bridge.same : 'a -> 'a -> Bool       physical identity of heap values
 *
 * Header-only: this header includes Bridge.c, the C++ implementation.
 */
#pragma once

#include "Support.h"

namespace metalib
{
/// Registers the `Bridge.*` primitives (sandboxed; see §10.4).
inline void install_Bridge (metaml::Session &s);
} // namespace metalib

#include "Bridge.c"

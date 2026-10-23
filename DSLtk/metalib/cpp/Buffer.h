/**
 * @file metalib/cpp/Buffer.h
 * @brief MetaML host module `Buffer` (C++ dialect) — mutable byte buffers (built on dsl::BinaryStream).
 *
 * Primitives (identical names, types and behaviour in metalib/d/Buffer.d):
 *   Buffer.of_string : String -> Buffer           independent copy
 *   Buffer.sub : Int -> Int -> Buffer -> Option String   checked start/length
 *   Buffer.truncate : Int -> Buffer -> Unit       rejects growth/negative sizes
 *   Buffer.create : Unit -> Buffer
 *   Buffer.add : String -> Buffer -> Unit
 *   Buffer.add_line : String -> Buffer -> Unit
 *   Buffer.add_byte : Int -> Buffer -> Unit
 *   Buffer.add_u16le / add_u32le / add_u64le / add_u32be : Int -> Buffer -> Unit
 *   Buffer.contents : Buffer -> String
 *   Buffer.length : Buffer -> Int
 *   Buffer.byte_at : Int -> Buffer -> Option Int
 *   Buffer.clear : Buffer -> Unit
 *   Buffer.hex : Buffer -> String          lowercase hex of all bytes
 *
 * Header-only: this header includes Buffer.c, the C++ implementation.
 */
#pragma once

#include "Support.h"

namespace metalib
{
/// Registers the `Buffer.*` primitives (sandboxed; see §10.4).
inline void install_Buffer (metaml::Session &s);
} // namespace metalib

#include "Buffer.c"

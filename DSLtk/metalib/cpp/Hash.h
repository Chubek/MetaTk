/**
 * @file metalib/cpp/Hash.h
 * @brief MetaML host module `Hash` (C++ dialect) — hashing and digests.
 *
 * Primitives (identical names, types and behaviour in metalib/d/Hash.d):
 *   Hash.fnv1a : String -> Int        64-bit FNV-1a
 *   Hash.djb2 : String -> Int         64-bit djb2 (h * 33 + c)
 *   Hash.value : 'a -> Int            structural hash of any value
 *   Hash.combine : Int -> Int -> Int  boost-style hash combine
 *   Hash.crc32 : String -> Int        CRC-32 (IEEE 802.3)
 *   Hash.sha256 : String -> String    lowercase hex digest
 *   Hash.hex : Int -> String          16 lowercase hex digits
 *
 * Header-only: this header includes Hash.c, the C++ implementation.
 */
#pragma once

#include "Support.h"

namespace metalib
{
/// Registers the `Hash.*` primitives (sandboxed; see §10.4).
inline void install_Hash (metaml::Session &s);
} // namespace metalib

#include "Hash.c"

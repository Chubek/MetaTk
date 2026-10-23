/**
 * @file metalib/cpp/FFI.h
 * @brief MetaML host module `FFI` (C++ dialect) — calling C functions in shared libraries (dlopen/dlsym).
 *
 * Primitives (identical names, types and behaviour in metalib/d/FFI.d):
 *   type FfiValue := | FInt Int | FFloat Float | FStr String | FVoid
 *   FFI.available : Unit -> Bool
 *   FFI.symbol_exists : String -> String -> Bool
 *   FFI.call : String -> String -> String -> [FfiValue] -> Result FfiValue String
 *
 *   Signatures are `R(A...)` over i (C int), l (int64/pointer), d (double),
 *   s (C string) and v (void, return only), e.g. `d(d)` for `cos`; at most six
 *   integer-class and eight double arguments; x86-64 and AArch64 POSIX only.
 *   Library "" means the running process. Calling with a wrong signature is
 *   undefined behaviour, so FFI is sandboxed (never in the default allowlist).
 *
 * Header-only: this header includes FFI.c, the C++ implementation.
 */
#pragma once

#include "Support.h"
#include <dlfcn.h>

namespace metalib
{
/// Registers the `FFI.*` primitives (sandboxed; see §10.4).
inline void install_FFI (metaml::Session &s);
} // namespace metalib

#include "FFI.c"

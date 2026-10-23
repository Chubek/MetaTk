/**
 * @file metalib/cpp/CBridge.h
 * @brief MetaML host module `CBridge` (C++ dialect) — a C ABI for embedding MetaML (`metaml_new`, `metaml_eval`, ...).
 *
 * Primitives (identical names, types and behaviour in metalib/d/CBridge.d):
 *   CBridge.api_version : Unit -> Int
 *   CBridge.functions : Unit -> [String]
 *
 *   C ABI (define METALIB_CBRIDGE_IMPLEMENTATION in one C++ translation unit
 *   before including this header; the D module always exports it):
 *     metaml_session *metaml_new (void);
 *     void metaml_free (metaml_session *);
 *     int metaml_load_stdlib (metaml_session *, const char *dir);  0 on success
 *     char *metaml_eval (metaml_session *, const char *src, int *ok);
 *     char *metaml_type_of (metaml_session *, const char *expr, int *ok);
 *     void metaml_free_string (char *);
 *     const char *metaml_version (void);
 *
 * Header-only: this header includes CBridge.c, the C++ implementation.
 */
#pragma once

#include "Support.h"
#include <cstdlib>
#include <cstring>

namespace metalib
{
/// Registers the `CBridge.*` primitives (sandboxed; see §10.4).
inline void install_CBridge (metaml::Session &s);
} // namespace metalib

#include "CBridge.c"

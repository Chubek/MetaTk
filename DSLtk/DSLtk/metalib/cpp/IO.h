/**
 * @file metalib/cpp/IO.h
 * @brief MetaML host module `IO` (C++ dialect) — console helpers beyond the core IO primitives.
 *
 * Primitives (identical names, types and behaviour in metalib/d/IO.d):
 *   IO.flush : Unit -> Unit
 *   IO.read_all : Unit -> String              rest of standard input
 *   IO.read_lines : Unit -> [String]          remaining lines of standard input
 *   IO.printf : String -> [String] -> Unit    `{}` formatting (see String.format)
 *   IO.print_lines : [String] -> Unit
 *   IO.is_tty : Unit -> Bool                  is standard output a terminal
 *
 * Header-only: this header includes IO.c, the C++ implementation.
 */
#pragma once

#include "Support.h"

namespace metalib
{
/// Registers the `IO.*` primitives (sandboxed; see §10.4).
inline void install_IO (metaml::Session &s);
} // namespace metalib

#include "IO.c"
